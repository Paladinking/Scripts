#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

DWORD find_flag(LPWSTR* argv, int* argc, LPCWSTR flag, LPCWSTR long_flag);

LPWSTR* parse_command_line(LPCWSTR args, int* argc);
