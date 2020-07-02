#include <iostream>
using namespace std;

void OnReceiveFrame(int status) {
    cout << "onReceiveFrame !\n";
}

typedef void(__stdcall * PFOnReceiveFrame)(int status);
typedef int(*PFStartDecodeVideo)(const char *filePath, 
    uint8_t *buf, PFOnReceiveFrame onReceiveFrame);

int main()
{

    return -1;
}

