#define no_init_all deprecated
#include <wtypes.h>
#include <iostream>

using namespace std;

void __stdcall OnReceiveFrame(int status) {
    cout << "onReceiveFrame !\n";
}

typedef void(__stdcall * PFOnReceiveFrame)(int status);
typedef int(*PFStartDecodeVideo)(string filePath, 
    shared_ptr<uint8_t> buf, PFOnReceiveFrame onReceiveFrame);

int main() {
    HINSTANCE handle = (HINSTANCE)LoadLibraryA("FFmpegCodec.dll");
    shared_ptr<uint8_t> pBuf(new uint8_t[480*272*8]);
    if (handle != NULL) {
        PFStartDecodeVideo pfStartDecodeVideo = (PFStartDecodeVideo)
            GetProcAddress(handle, "StartDecodeVideo");
        if (pfStartDecodeVideo != NULL) {
            pfStartDecodeVideo("a.h265", pBuf, OnReceiveFrame);
        }
    }
    return -1;
}

