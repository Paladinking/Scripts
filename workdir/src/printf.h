#pragma once
#include <windows.h>

#define _printf(fmt, ...) _printf_h(GetStdHandle(STD_OUTPUT_HANDLE), fmt, __VA_ARGS__)
#define _wprintf(fmt, ...) _wprintf_h(GetStdHandle(STD_OUTPUT_HANDLE), fmt, __VA_ARGS__)

int _printf_h(HANDLE dest, const char* fmt, ...);

int _wprintf_h(HANDLE dest, const wchar_t* fmt, ...);
