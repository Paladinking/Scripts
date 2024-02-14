#pragma once
#include <windows.h>

#define _printf(...) _printf_h(GetStdHandle(STD_OUTPUT_HANDLE), __VA_ARGS__)
#define _wprintf(...) _wprintf_h(GetStdHandle(STD_OUTPUT_HANDLE), __VA_ARGS__)

int _printf_h(HANDLE dest, const char* fmt, ...);

int _wprintf_h(HANDLE dest, const wchar_t* fmt, ...);
