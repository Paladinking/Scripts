#pragma once
#include <windows.h>
#include <stdint.h>
#include "ochar.h"

#ifdef NARROW_OCHAR
    #define oprintf(...) _printf(__VA_ARGS__)
    #define oprintf_h(...) _printf_h(__VA_ARGS__)
    #define oprintf_e(...) _printf_e(__VA_ARGS__)
#else
    #define oprintf(...) _wprintf(L##__VA_ARGS__)
    #define oprintf_h(handle, ...) _wprintf_h(handle, L##__VA_ARGS__)
    #define oprintf_e(...) _wprintf_e(L##__VA_ARGS__)
#endif

#define _printf(...) _printf_h(GetStdHandle(STD_OUTPUT_HANDLE), __VA_ARGS__)
#define _wprintf(...) _wprintf_h(GetStdHandle(STD_OUTPUT_HANDLE), __VA_ARGS__)

#define outputa(str, count) outputa_h(GetStdHandle(STD_OUTPUT_HANDLE), str, count)
#define outputw(str, count) outputw_h(GetStdHandle(STD_OUTPUT_HANDLE), str, count)
#define outputUtf8(str, count) outputUtf8_h(GetStdHandle(STD_OUTPUT_HANDLE), (const uint8_t*)str, count)

#define outputstr(str) do { const char* s = str; outputUtf8_h(GetStdHandle(STD_OUTPUT_HANDLE), (const uint8_t*)s, strlen(s)); } while (0)
#define outputwstr(str) do { const wchar_t* s = str; outputw_h(GetStdHandle(STD_OUTPUT_HANDLE), s, wcslen(s)); } while (0)

#define _printf_e(...) _printf_h(GetStdHandle(STD_ERROR_HANDLE), __VA_ARGS__)
#define _wprintf_e(...) _wprintf_h(GetStdHandle(STD_ERROR_HANDLE), __VA_ARGS__)
#define outputw_e(str, count) outputw_h(GetStdHandle(STD_ERROR_HANDLE), str, count)
#define outputUtf8_e(str, count) outputUtf8_h(GetStdHandle(STD_ERROR_HANDLE), (const uint8_t*)str, count)

int _printf_h(HANDLE dest, const char* fmt, ...);

int _wprintf_h(HANDLE dest, const wchar_t* fmt, ...);

void outputw_h(HANDLE out, const wchar_t* data, size_t size);

void outputa_h(HANDLE out, const char* data, size_t size);

void outputUtf8_h(HANDLE out, const uint8_t* data, size_t size);
