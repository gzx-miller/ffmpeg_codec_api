#ifdef DLL_EXPORTS
#define DLL_API extern "C" __declspec(dllexport)
#else
#define DLL_API extern "C" __declspec(dllimport)
#endif

#include <stdint.h>

typedef void(__stdcall * OnReceiveFrame)(int status);
DLL_API int StartDecoceVideo(const char *filePath, uint8_t *buf,
    OnReceiveFrame onReceiveFrame);

