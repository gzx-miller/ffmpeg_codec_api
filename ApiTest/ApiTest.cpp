#define no_init_all deprecated
#include <wtypes.h>
#include <iostream>

using namespace std;

shared_ptr<uint8_t> pBuf(new uint8_t[480 * 272 * 8]);

struct MyBITMAPFILEHEADER {
    unsigned int   bfSize;           /* Size of file */
    unsigned short bfReserved1;      /* Reserved */
    unsigned short bfReserved2;      /* ... */
    unsigned int   bfOffBits;        /* Offset to bitmap data */
};
struct MyBITMAPINFOHEADER {
    unsigned int biSize;
    int biWidth;
    int biHeight;
    unsigned short biPlanes;         /* Number of color planes */
    unsigned short biBitCount;       /* Number of bits per pixel */
    unsigned int   biCompression;    /* Type of compression to use */
    unsigned int   biSizeImage;      /* Size of image data */
    int            biXPelsPerMeter;  /* X pixels per meter */
    int            biYPelsPerMeter;  /* Y pixels per meter */
    unsigned int   biClrUsed;        /* Number of colors used */
    unsigned int   biClrImportant;   /* Number of important colors */
};
void SaveRGBtoBMP(const char *filename, uint8_t *rgbbuf, int width, int height) {
    MyBITMAPFILEHEADER bfh;
    MyBITMAPINFOHEADER bih;
    /* Magic number for file. It does not fit in the header structure due to alignment requirements, so put it outside */
    unsigned short bfType = 0x4d42;
    bfh.bfReserved1 = 0;
    bfh.bfReserved2 = 0;
    bfh.bfSize = 2 + sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + width * height * 3;
    bfh.bfOffBits = 0x36;

    bih.biSize = sizeof(BITMAPINFOHEADER);
    bih.biWidth = width;
    bih.biHeight = height;
    bih.biPlanes = 1;
    bih.biBitCount = 24;
    bih.biCompression = 0;
    bih.biSizeImage = 0;
    bih.biXPelsPerMeter = 5000;
    bih.biYPelsPerMeter = 5000;
    bih.biClrUsed = 0;
    bih.biClrImportant = 0;

    FILE *file = nullptr;
    fopen_s(&file, filename, "wb");
    if (!file)
    {
        printf("Could not write file\n");
        return;
    }

    fwrite(&bfType, sizeof(bfType), 1, file);
    fwrite(&bfh, sizeof(bfh), 1, file);
    fwrite(&bih, sizeof(bih), 1, file);

    fwrite(rgbbuf, width*height * 3, 1, file);
    fclose(file);
}

typedef bool (* PFStopDecodeWork)(int index);
PFStopDecodeWork pfStopDecodeWork = nullptr;
int workerIndex = 0;

void __stdcall OnReceiveFrame(int status, string msg) {
    if(status == 1) {
        static int id = 0;
        SaveRGBtoBMP("1.bmp", pBuf.get(), 480, 272);
    } else {
        pfStopDecodeWork(workerIndex);
    }
}

typedef void(__stdcall * PFOnReceiveFrame)(int status, string msg);
typedef int(*PFStartDecodeWork)(string filePath, 
    shared_ptr<uint8_t> buf, PFOnReceiveFrame onReceiveFrame);

int main() {
    HINSTANCE handle = (HINSTANCE)LoadLibraryA("FFmpegCodec.dll");
    if (handle != NULL) {
        PFStartDecodeWork pfStartDecodeWork = (PFStartDecodeWork)
            GetProcAddress(handle, "StartDecodeWork");
        pfStopDecodeWork = (PFStopDecodeWork)
            GetProcAddress(handle, "StopDecodeWork");
        if (pfStartDecodeWork != NULL && pfStopDecodeWork != NULL) {
            workerIndex = pfStartDecodeWork("a.h265", pBuf, OnReceiveFrame);
        }
    }
    char ch = 'r';
    while(ch != 'q' || ch != 'Q') {
        ch = getchar();
    }
    return -1;
}

