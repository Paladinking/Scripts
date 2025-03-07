#ifndef UNICODE
#define UNICODE
#endif

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "args.h"
#include "glob.h"
#include "printf.h"

const wchar_t* data[] = {
    L"123456789",
    L"abcdefghi",
    L"asfoamsfmaslfmalskmflkasmlfk",
    L"asfasofk",
    L"vkgfmdlnbkllkndlkn",
    L"amlkdmaslkdmlaksmdlamslkdmalskmdlkmasd",
    L"dasdmbmkfdmllkdldldldl",
    L",----",
    L"aaslalalla",
    L"asjdkasjdkajsk",
    L"123456789",
    L"",
};

/*
    uint64_t cap = 100;
    uint64_t size = 0;
    wchar_t* ptr = Mem_alloc(100 * sizeof(wchar_t));

    for (uint32_t ix = 0; ix < 100; ++ix) {
        uint64_t len = wcslen(data[ix % 12]);
        while (size + len + 1 > cap) {
            cap *= 2;
            ptr = Mem_realloc(ptr, cap * sizeof(wchar_t));
        }
        memcpy(ptr + size, data[ix % 12], (len + 1) * sizeof(wchar_t));
        size += (len + 1);
    }
*/

int main() {
    LARGE_INTEGER li;
    QueryPerformanceFrequency(&li);
    uint64_t freq = li.QuadPart;
    QueryPerformanceCounter(&li);
    uint64_t now = li.QuadPart;

    uint64_t cap = 100;
    uint64_t size = 0;
    wchar_t* ptr = Mem_alloc(100 * sizeof(wchar_t));

    for (uint32_t ix = 0; ix < 1000; ++ix) {
        uint64_t len = wcslen(data[ix % 12]);
        while (size + len + 1 > cap) {
            cap *= 2;
            ptr = Mem_realloc(ptr, cap * sizeof(wchar_t));
        }
        memcpy(ptr + size, data[ix % 12], (len + 1) * sizeof(wchar_t));
        size += (len + 1);
    }

    QueryPerformanceCounter(&li);
    uint64_t delta = ((li.QuadPart - now));

    _wprintf(L"Passed: %llu\n", delta);

    return 0;
}
