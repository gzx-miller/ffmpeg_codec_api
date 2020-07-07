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

void FFMpegDecoderProc(int threadIndex, string filePath,
    shared_ptr<uint8_t> outputBuf,
    OnReceiveFrame onReceiveFrame) {
    AVFormatContext	*pFormatCtx = avformat_alloc_context();
    if (avformat_open_input(&pFormatCtx, filePath.c_str(), NULL, NULL) != 0) {
        return DecoderExit(threadIndex, "open file failed!");
    }
    if (avformat_find_stream_info(pFormatCtx, NULL) < 0) {
        return DecoderExit(threadIndex, "find stream info failed!");
    }

    int i = 0;
    int videoIndex = GetVideoIndex(pFormatCtx, i);
    if (videoIndex < 0) {
        return DecoderExit(threadIndex, "get video stream failed!");
    }

    AVCodecParameters *pCodecParam = pFormatCtx->streams[videoIndex]->codecpar;
    AVCodec *pCodec = avcodec_find_decoder(pCodecParam->codec_id);
    if (pCodec == NULL) {
        return DecoderExit(threadIndex, "find decoder of videoIndex failed!");
    }
    AVCodecParserContext *parser = av_parser_init(pCodec->id);
    if (!parser) {
        return DecoderExit(threadIndex, "parser init failed!");
    }
    AVCodecContext *codecContext = avcodec_alloc_context3(pCodec);
    if (!codecContext) {
        return DecoderExit(threadIndex, "alloc context failed!");
    }

    if (avcodec_parameters_to_context(codecContext, pCodecParam) < 0) {
        return DecoderExit(threadIndex, "parameters to context failed!");
    }

    if (avcodec_open2(codecContext, pCodec, NULL) < 0) {
        return DecoderExit(threadIndex, "open decoder failed!");
    }

    AVFrame *pFrame = av_frame_alloc();
    AVFrame * pFrameYUV = av_frame_alloc();
    AVPacket* pkt = av_packet_alloc();
    if (!pkt) {
        return DecoderExit(threadIndex, "alloc packet failed!");
    }

    av_dump_format(pFormatCtx, 0, filePath.c_str(), 0);
    
    uint8_t inbuf[INBUF_SIZE + AV_INPUT_BUFFER_PADDING_SIZE];
    memset(inbuf + INBUF_SIZE, 0, AV_INPUT_BUFFER_PADDING_SIZE);
    uint8_t *data;
    FILE* f = nullptr;
    fopen_s(&f, filePath.c_str(), "rb");
    if (!f) {
        return DecoderExit(threadIndex, "open file failed !");
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
                return DecoderExit(threadIndex, "av parse failed!");
            }
            data += ret;
            data_size -= ret;
            if (pkt->size) {
                if (pkt->stream_index == videoIndex) {
                    int retd = DecodePacket(codecContext, pkt, pFrame);
                    if (retd == 0) continue;
                    if (retd == -1) return DecoderExit(threadIndex, "decode packet failed!");

                    yuv420pToRGB24(pFrame->data[0], pFrame->data[1], pFrame->data[2], 
                        pFrame->width, pFrame->height, pFrame->linesize[0], outputBuf.get());
                    onReceiveFrame(1, "frame");
                    this_thread::sleep_for(chrono::milliseconds(40));
                }
            }
        }
    }

    onReceiveFrame(1, "complete!");

    av_parser_close(parser);
    av_frame_free(&pFrameYUV);
    av_frame_free(&pFrame);
    avcodec_free_context(&codecContext);
    av_packet_free(&pkt);
    avformat_close_input(&pFormatCtx);
    return DecoderExit(threadIndex, "complete!");
}

bool FFMpegCodec::StartDecodeThread(int threadIndex, string filePath,
    shared_ptr<uint8_t> outputBuf,
    OnReceiveFrame onReceiveFrame) {
    _isExit = false;
    if (!_isRuning) {
        _pThread.reset(new thread(FFMpegDecoderProc, threadIndex, filePath, outputBuf, onReceiveFrame));
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
    {
        unique_lock<mutex> lock(mtx);
        pCodec->StartDecodeThread(g_handle_index, filePath, outputBuf, onReceiveFrame);
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

static void DecoderExit(int threadIndex, string msg) {
    fprintf(stdout, msg.c_str());
    thread t(StopDecodeWork, threadIndex);
    t.detach();
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

void yuv420pToRGB24(const BYTE *yBuf, const BYTE *uBuf, const BYTE *vBuf,
    const int width, const int height, int lineSize, BYTE *rgbBuf) {
    int dstIndex = 0;
    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
            BYTE y = yBuf[i * lineSize + j];
            BYTE u = uBuf[(i>>1) * (lineSize>>1) + (j>>1)];
            BYTE v = vBuf[(i>>1) * (lineSize>>1) + (j>>1)];

            dstIndex = ((height - i - 1) * width + j) * 3;
            int data = (int)(y + 1.772 * (u - 128));
            rgbBuf[dstIndex] = ((data < 0) ? 0 : (data > 255 ? 255 : data));

            data = (int)(y - 0.34414 * (u - 128) - 0.71414 * (v - 128));
            rgbBuf[dstIndex + 1] = ((data < 0) ? 0 : (data > 255 ? 255 : data));

            data = (int)(y + 1.402 * (v - 128));
            rgbBuf[dstIndex + 2] = ((data < 0) ? 0 : (data > 255 ? 255 : data));
        }
    }
}

