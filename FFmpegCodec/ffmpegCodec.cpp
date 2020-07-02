#define  DLL_EXPORTS
#include "ffmpegcodec.h"
#include <stdio.h>
#include <map>
#include <mutex>

extern "C" {
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include "libavutil/imgutils.h"
}

#define INBUF_SIZE 4096
bool GetVideoIndex(AVFormatContext * pFormatCtx, int &i) {
    int videoIndex = -1;
    for (; i < pFormatCtx->nb_streams; i++) {
        if (pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoIndex = i;
            break;
        }
    }
    return videoIndex;
}

static int DecodePacket(AVCodecContext *dec_ctx, AVPacket *pkt, AVFrame *frame) {
    int ret = avcodec_send_packet(dec_ctx, pkt);
    if (ret < 0) {
        fprintf(stderr, "sending a packet for decoding failed! \n");
        return -1;
    }

    ret = avcodec_receive_frame(dec_ctx, frame);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
        return 0;
    }
    else if (ret < 0) {
        fprintf(stderr, "Error during decoding\n");
        return -1;
    }
    return 1;
}

void FFMpegDecoderProc(string filePath,
    shared_ptr<uint8_t> outputBuf,
    OnReceiveFrame onReceiveFrame) {
    AVFormatContext	*pFormatCtx = avformat_alloc_context();
    if (avformat_open_input(&pFormatCtx, filePath.c_str(), NULL, NULL) != 0) {
        return onReceiveFrame(-1, "open file failed!");
    }
    if (avformat_find_stream_info(pFormatCtx, NULL) < 0) {
        return onReceiveFrame(-1, "find stream info failed!");
    }

    int i = 0;
    int videoIndex = GetVideoIndex(pFormatCtx, i);
    if (videoIndex < 0) {
        return onReceiveFrame(-1, "get video stream failed!");
    }

    AVCodecParameters *pCodecParam = pFormatCtx->streams[videoIndex]->codecpar;
    AVCodec *pCodec = avcodec_find_decoder(pCodecParam->codec_id);
    if (pCodec == NULL) {
        return onReceiveFrame(-1, "find decoder of videoIndex failed!");
    }
    AVCodecParserContext *parser = av_parser_init(pCodec->id);
    if (!parser) {
        return onReceiveFrame(-1, "parser init failed!");
    }
    AVCodecContext *codecContext = avcodec_alloc_context3(pCodec);
    if (!codecContext) {
        return onReceiveFrame(-1, "alloc context failed!");
    }

    if (avcodec_parameters_to_context(codecContext, pCodecParam) < 0) {
        return onReceiveFrame(-1, "parameters to context failed!");
    }

    if (avcodec_open2(codecContext, pCodec, NULL) < 0) {
        return onReceiveFrame(-1, "open decoder failed!");
    }

    AVFrame *pFrame = av_frame_alloc();
    AVFrame * pFrameYUV = av_frame_alloc();
    AVPacket* pkt = av_packet_alloc();
    if (!pkt) {
        return onReceiveFrame(-1, "alloc packet failed!");
    }

    av_dump_format(pFormatCtx, 0, filePath.c_str(), 0);
    
    uint8_t inbuf[INBUF_SIZE + AV_INPUT_BUFFER_PADDING_SIZE];
    memset(inbuf + INBUF_SIZE, 0, AV_INPUT_BUFFER_PADDING_SIZE);
    uint8_t *data;
    FILE* f = nullptr;
    fopen_s(&f, filePath.c_str(), "rb");
    if (!f) {
        return onReceiveFrame(-1, "open file failed !");
    }

    while (!feof(f)) {
        int data_size = fread(inbuf, 1, INBUF_SIZE, f);
        if (!data_size) {
            break;
        }
        data = inbuf;
        while (data_size > 0) {
            int ret = av_parser_parse2(parser, codecContext, &pkt->data, &pkt->size,
                data, data_size, AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0);
            if (ret < 0) {
                return onReceiveFrame(-1, "av parse failed!");
            }
            data += ret;
            data_size -= ret;
            if (pkt->size) {
                if (pkt->stream_index == videoIndex) {
                    int retd = DecodePacket(codecContext, pkt, pFrame);
                    if (retd == 0) continue;
                    if (retd == -1) return onReceiveFrame(-1, "decode packet failed!");
                    yuv420p_to_rgb24(pFrame->data[0], pFrame->data[1], pFrame->data[2],
                        outputBuf.get(), pFrame->width, pFrame->height);
                    onReceiveFrame(1, "frame");
                    this_thread::sleep_for(chrono::milliseconds(40));
                }
            }
        }
    }
    av_parser_close(parser);
    av_frame_free(&pFrameYUV);
    av_frame_free(&pFrame);
    avcodec_free_context(&codecContext);
    av_packet_free(&pkt);
    avformat_close_input(&pFormatCtx);
    return onReceiveFrame(0, "complete!");
}

bool FFMpegCodec::StartDecodeThread(string filePath,
    shared_ptr<uint8_t> outputBuf,
    OnReceiveFrame onReceiveFrame) {
    _isExit = false;
    if (!_isRuning) {
        _pThread.reset(new thread(FFMpegDecoderProc, filePath, outputBuf, onReceiveFrame));
    }
    else {
        fprintf(stderr, "Can't running two worker in same time! \n");
        return false;
    }
    return true;
};

bool FFMpegCodec::StopDecodeThread() {
    if (!_isRuning) return true;
    _isExit = true;
    _pThread->join();
    _pThread.release();
};

mutex mtx;
int g_handle_index = 1;
map<int, shared_ptr<FFMpegCodec>> g_map_codecs;

DLL_API int StartDecodeWork(string filePath,
    shared_ptr<uint8_t> outputBuf,
    OnReceiveFrame onReceiveFrame){
    shared_ptr<FFMpegCodec> pCodec(new FFMpegCodec());
    pCodec->StartDecodeThread(filePath, outputBuf, onReceiveFrame);
    {
        unique_lock<mutex> lock(mtx);
        g_map_codecs.insert(make_pair(g_handle_index++, pCodec));
    }
    return NULL;
}

DLL_API bool StopDecodeWork(int index){
    {
        shared_ptr<FFMpegCodec> pCodec;
        {
            unique_lock<mutex> lock(mtx);
            auto find = g_map_codecs.find(index);
            if (find != g_map_codecs.end()) {
                pCodec = find->second;
                g_map_codecs.erase(find);
            }
        }
        if (pCodec.get() != nullptr) {
            pCodec->StopDecodeThread();
        }
    }
    return TRUE;
}

BOOL __stdcall DllMain(HANDLE hModule, DWORD reason, LPVOID lpReserved) {
    switch (reason) {
        case DLL_PROCESS_ATTACH: break;              // 被load
        case DLL_THREAD_ATTACH:  break;              // 有线程启动了
        case DLL_THREAD_DETACH: break;               // 有线程终止了
        case DLL_PROCESS_DETACH:  break;             // 被卸载
    }
    return TRUE;
}
static long int crv_tab[256];
static long int cbu_tab[256];
static long int cgu_tab[256];
static long int cgv_tab[256];
static long int tab_76309[256];
static uint8_t clp[1024];   //for clip in CCIR601   

void yuv420p_to_rgb24(uint8_t* srcY, uint8_t* srcU, uint8_t* srcV,
    uint8_t* rgbbuffer, int width, int height) {
    int y1, y2, u, v;
    uint8_t *py2;
    int i, j, c1, c2, c3, c4;
    uint8_t *d1, *d2;
    static int init_yuv420p = 0;
    if (init_yuv420p == 0) {
        long int crv, cbu, cgu, cgv;
        int i, ind;
        crv = 104597; cbu = 132201;  /* fra matrise i global.h */
        cgu = 25675;  cgv = 53279;
        for (i = 0; i < 256; i++) {
            crv_tab[i] = (i - 128) * crv;
            cbu_tab[i] = (i - 128) * cbu;
            cgu_tab[i] = (i - 128) * cgu;
            cgv_tab[i] = (i - 128) * cgv;
            tab_76309[i] = 76309 * (i - 16);
        }
        for (i = 0; i < 384; i++)
            clp[i] = 0;
        ind = 384;
        for (i = 0; i < 256; i++)
            clp[ind++] = i;
        ind = 640;
        for (i = 0; i < 384; i++)
            clp[ind++] = 255;
        init_yuv420p = 1;
    }

    py2 = srcY + width;
    d1 = rgbbuffer;
    d2 = d1 + 3 * width;

    for (j = 0; j < height; j += 2)
    {
        for (i = 0; i < width; i += 2)
        {
            u = *srcU++;
            v = *srcV++;

            c1 = crv_tab[v];
            c2 = cgu_tab[u];
            c3 = cgv_tab[v];
            c4 = cbu_tab[u];

            //up-left   
            y1 = tab_76309[*srcY++];
            *d1++ = clp[384 + ((y1 + c1) >> 16)];
            *d1++ = clp[384 + ((y1 - c2 - c3) >> 16)];
            *d1++ = clp[384 + ((y1 + c4) >> 16)];

            //down-left   
            y2 = tab_76309[*py2++];
            *d2++ = clp[384 + ((y2 + c1) >> 16)];
            *d2++ = clp[384 + ((y2 - c2 - c3) >> 16)];
            *d2++ = clp[384 + ((y2 + c4) >> 16)];

            //up-right   
            y1 = tab_76309[*srcY++];
            *d1++ = clp[384 + ((y1 + c1) >> 16)];
            *d1++ = clp[384 + ((y1 - c2 - c3) >> 16)];
            *d1++ = clp[384 + ((y1 + c4) >> 16)];

            //down-right   
            y2 = tab_76309[*py2++];
            *d2++ = clp[384 + ((y2 + c1) >> 16)];
            *d2++ = clp[384 + ((y2 - c2 - c3) >> 16)];
            *d2++ = clp[384 + ((y2 + c4) >> 16)];
        }
        d1 += 3 * width;
        d2 += 3 * width;
        srcY += width;
        py2 += width;
    }
}