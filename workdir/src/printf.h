#pragma once
#include <windows.h>

#define _printf(...) _printf_h(GetStdHandle(STD_OUTPUT_HANDLE), __VA_ARGS__)
#define _wprintf(...) _wprintf_h(GetStdHandle(STD_OUTPUT_HANDLE), __VA_ARGS__)
#define outputa(str, count) outputa_h(GetStdHandle(STD_OUTPUT_HANDLE), str, count)
#define outputw(str, count) outputw_h(GetStdHandle(STD_OUTPUT_HANDLE), str, count)
#define outputstr(str) do { const char* s = str; outputa_h(GetStdHandle(STD_OUTPUT_HANDLE), s, strlen(s)); } while (0)
#define outputwstr(str) do { const wchar_t* s = str; outputw_h(GetStdHandle(STD_OUTPUT_HANDLE), s, wcslen(s)); } while (0)

#define _printf_e(...) _printf_h(GetStdHandle(STD_ERROR_HANDLE), __VA_ARGS__)
#define _wprintf_e(...) _wprintf_h(GetStdHandle(STD_ERROR_HANDLE), __VA_ARGS__)

int _printf_h(HANDLE dest, const char* fmt, ...);

int _wprintf_h(HANDLE dest, const wchar_t* fmt, ...);

void outputw_h(HANDLE out, const wchar_t* data, size_t size);

void outputa_h(HANDLE out, const char* data, size_t size);
