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

bool FFMpegCodec::YuvToRgb(uint8_t *yuv, int w, int h, uint8_t *rgb) {
    int wh = w * h;
    for (int i = 0; i < wh / 4; i++) {
        int ou = wh + i;
        int ov = wh * 5 / 4 + i;
        int r = i * 3;
        rgb[r + 2] = yuv[i] + 1.772*(yuv[ou] - 128);
        rgb[r + 1] = yuv[i] - 0.34413*(yuv[ou] - 128) - 0.71414*(yuv[ov] - 128);
        rgb[r + 0] = yuv[i] + 1.402*(yuv[ov] - 128);
    }
    return true;
}

void FFMpegDecoderProc(string filePath,
    shared_ptr<uint8_t> outputBuf,
    OnReceiveFrame onReceiveFrame) {
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
        unique_lock<mutex> lock(mtx);
        auto find = g_map_codecs.find(index);
        if (find != g_map_codecs.end()) {
            shared_ptr<FFMpegCodec> pCodec = find->second;
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