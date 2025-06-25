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

void outputUtf8_h(HANDLE out, const uint8_t* data, size_t size) {
    if (size > INT_MAX) {
        size = INT_MAX;
    }
    DWORD type = GetFileType(out);
    if (type == FILE_TYPE_CHAR) {
        wchar_t buf[512];
        int outsize = MultiByteToWideChar(CP_UTF8, 0, (const char*)data, size, NULL, 0);
        wchar_t* output = buf;
        if (outsize > 512) {
            output = Mem_alloc(outsize * sizeof(wchar_t));
            if (output == NULL) {
                return;
            }
        }
        MultiByteToWideChar(CP_UTF8, 0, (const char*)data, size, output, outsize);
        DWORD written = 0;
        while (written < outsize) {
            DWORD w;
            if (!WriteConsoleW(out, output + written, outsize - written, &w, NULL)) {
                break;
            }
            written += w;
        }
        if (outsize > 512) {
            Mem_free(output);
        }
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

void outputa_h(HANDLE out, const char* data, size_t size) {
    DWORD type = GetFileType(out);
    if (type == FILE_TYPE_CHAR) {
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
        uint8_t buf[1024];
        _vsnprintf((char*)buf, count, fmt, args);
        outputUtf8_h(dest, buf, count);
    } else {
        uint8_t *buf = (uint8_t *)Mem_alloc(count);
        if (buf == NULL) {
            return -1;
        }
        _vsnprintf((char*)buf, count, fmt, args);
        outputUtf8_h(dest, buf, count);
        Mem_free(buf);
    }
    va_end(args);
    return count;
}

void outputw_h(HANDLE out, const wchar_t* data, size_t size) {
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
        outputw_h(dest, buf, count);
    } else {
        wchar_t *buf = (wchar_t *)Mem_alloc(count * sizeof(wchar_t));
        if (buf == NULL) {
            return -1;
        }
        _vsnwprintf(buf, count, fmt, args);
        outputw_h(dest, buf, count);
        Mem_free(buf);
    }
    va_end(args);
    return count;
}
