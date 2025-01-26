#define UNICODE
#include "printf.h"
#include "mem.h"
#include <stdarg.h>
#ifdef __cplusplus
extern "C" {
#endif
#ifndef USE_STDLIB
extern int _vscprintf(const char *format, va_list argptr);
extern int _vscwprintf(const wchar_t *format, va_list argptr);
extern int _vsnprintf(char *buffer, size_t count, const char *format,
                      va_list argptr);
extern int _vsnwprintf(wchar_t *buffer, size_t count, const wchar_t *format,
                       va_list argptr);
#else
#include <stdio.h>
#include <wchar.h>
#endif
#ifdef __cplusplus
}
#endif

void outputa(HANDLE out, char* data, size_t size) {
    DWORD type = GetFileType(out);
    if (type == FILE_TYPE_CHAR) {
        // TODO: Convert to UTF-16 and use WriteConsoleW??
        WriteConsoleA(out, data, size, NULL, NULL);
    } else {
        DWORD written = 0;
        while (written < size) {
            DWORD w;
            if (!WriteFile(out, data + written, size - written, &w, NULL)) {
                break;
            }
            written += w;
        }
    }
}

int _printf_h(HANDLE dest, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int count = _vscprintf(fmt, args);
    if (count == -1) {
        return -1;
    }
    if (count < 1024) { // Note: not inclusive for NULL terminator
        char buf[1024];
        _vsnprintf(buf, count, fmt, args);
        WriteConsoleA(dest, buf, count, NULL, NULL);
    } else {
        HANDLE heap = GetProcessHeap();
        char *buf = (char *)Mem_alloc(count);
        if (buf == NULL) {
            return -1;
        }
        _vsnprintf(buf, count, fmt, args);
        outputa(dest, buf, count);
        Mem_free(buf);
    }
    va_end(args);
    return count;
}

void outputw(HANDLE out, wchar_t* data, size_t size) {
    DWORD type = GetFileType(out);
    if (type == FILE_TYPE_CHAR) {
        WriteConsoleW(out, data, size, NULL, NULL);
    } else {
        char buf[1024];
        int outsize = WideCharToMultiByte(CP_UTF8, 0, data, size, NULL, 0, NULL, NULL);
        char* output;
        if (outsize <= 1024) {
            output = buf;
        } else {
            output = (char*)Mem_alloc(outsize);
            if (output == NULL) {
                return;
            }
        }
        WideCharToMultiByte(CP_UTF8, 0, data, size, output, outsize, NULL, NULL);
        DWORD written = 0;
        while (written < outsize) {
            DWORD w;
            if (!WriteFile(out, output + written, outsize - written, &w, NULL)) {
                break;
            }
            written += w;
        }
        if (outsize > 1024) {
            Mem_free(output);
        }
    }
}

int _wprintf_h(HANDLE dest, const wchar_t *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int count = _vscwprintf(fmt, args);
    if (count == -1) {
        return -1;
    }
    if (count < 512) { // Note: not inclusive for NULL terminator
        wchar_t buf[512];
        _vsnwprintf(buf, count, fmt, args);
        outputw(dest, buf, count);
    } else {
        wchar_t *buf = (wchar_t *)Mem_alloc(count * sizeof(wchar_t));
        if (buf == NULL) {
            return -1;
        }
        _vsnwprintf(buf, count, fmt, args);
        outputw(dest, buf, count);
        Mem_free(buf);
    }
    va_end(args);
    return count;
}
