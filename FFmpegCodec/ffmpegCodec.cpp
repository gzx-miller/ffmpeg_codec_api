
#define  DLL_EXPORTS
#include "ffmpegcodec.h"
#include <stdio.h>

extern "C" {
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include "libavutil/imgutils.h"
}

DLL_API int StartDecoceVideo(const char *filePath, uint8_t *buf,
    OnReceiveFrame onReceiveFrame) {
    fprintf(stdout, "StartDecoceVideo! \n", filePath);
    onReceiveFrame(1);
    return -1;
}

