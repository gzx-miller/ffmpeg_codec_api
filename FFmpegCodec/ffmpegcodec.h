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

// 1: one frame, -1: failed, 0: complete
typedef void(__stdcall * OnReceiveFrame)(int status, string msg);

DLL_API int StartDecodeWork(string filePath,
    shared_ptr<uint8_t> outputBuf,
    OnReceiveFrame onReceiveFrame);

DLL_API bool StopDecodeWork(int index);
static void DecoderExit(int threadIndex, string msg);

class FFMpegCodec {
private:
    unique_ptr<thread> _pThread;
    bool _isRuning;
    bool _isExit;
public:
    FFMpegCodec(): _isRuning(false), _isExit(true) {}
    ~FFMpegCodec() {}
    bool StartDecodeThread(int threadIndex, string filePath,
        shared_ptr<uint8_t> outputBuf,
        OnReceiveFrame onReceiveFrame);
    bool StopDecodeThread();
};

void yuv420planarToRGB(const BYTE *yData, const BYTE *uData, const BYTE *vData, const int width, const int height, int lineSize, BYTE *rgb24Data);