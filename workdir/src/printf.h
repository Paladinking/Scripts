#pragma once
#include <windows.h>

int _printf(const char* fmt, ...);

int _wprintf(const wchar_t* fmt, ...);

int _printf_h(HANDLE dest, const char* fmt, ...);

int _wprintf_h(HANDLE dest, const wchar_t* fmt, ...);
