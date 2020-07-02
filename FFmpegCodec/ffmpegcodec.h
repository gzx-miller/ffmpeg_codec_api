#ifdef DLL_EXPORTS
#define DLL_API extern "C" __declspec(dllexport)
#else
#define DLL_API extern "C" __declspec(dllimport)
#endif
#define no_init_all deprecated

#include <wtypes.h>
#include <stdint.h>
#include <string>
#include <memory>
#include <thread>

using namespace std;
typedef void(__stdcall * OnReceiveFrame)(int status);

DLL_API int StartDecodeWork(string filePath,
    shared_ptr<uint8_t> outputBuf,
    OnReceiveFrame onReceiveFrame);

DLL_API bool StopDecodeWork(int index);

class FFMpegCodec {
private:
    unique_ptr<thread> _pThread;
    bool _isRuning;
    bool _isExit;
public:
    FFMpegCodec(): _isRuning(false), _isExit(true) {}
    ~FFMpegCodec() {}
    bool StartDecodeThread(string filePath,
        shared_ptr<uint8_t> outputBuf,
        OnReceiveFrame onReceiveFrame);

    bool StopDecodeThread();

    bool YuvToRgb(uint8_t *yuv, int w, int h, uint8_t *rgb);
};

// 1. 使用路径, 创建ffmpeg相关对象
// 2. 启动线程, 开始解码
// 3. 通过回调设置每一帧图像到outputBuf
// 4. 停止解码, 退出线程.

