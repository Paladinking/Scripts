#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

/**
 * Checks if flag <flag> or <long_flag> apears in <argv>.
 * <argc> specifies number of entries in <argv>
 * Return number of occurances. Removes all found instances.
 */
DWORD find_flag(LPWSTR* argv, int* argc, LPCWSTR flag, LPCWSTR long_flag);

/**
 * Convert a commandline into C-style argv and argc.
 * Returns argv, <argc> gets number of arguments.
 */
LPWSTR* parse_command_line(LPCWSTR args, int* argc);

/**
 * Parse a commandline potentialy without escaping of backslashes and qoutes.
 */
LPWSTR* parse_command_line_with(LPCWSTR args, int* argc, BOOL escape_backslash, BOOL escape_quotes);
