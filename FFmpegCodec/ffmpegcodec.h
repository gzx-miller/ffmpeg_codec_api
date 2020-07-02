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
// if return -1 or 0, need call StopDecodeWork for clear thread in g_map_codecs.
typedef void(__stdcall * OnReceiveFrame)(int status, string msg);

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
};

void yuv420p_to_rgb24(uint8_t* srcY, uint8_t* srcU, uint8_t* srcV,
    uint8_t* rgbbuffer, int width, int height);