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
 * Get the value of flag <flag> or <long_flag>.
 * <flag> allows specifying value in same agument, e.g -v10 <=> -v 10.
 * Either <flag> or <long_flag> can be NULL.
 * Puts found value in <dest>, will point to value
 * If this function retuns 1, value and flag are removed from args.
 * Returns 0 if no flag found, -1 if flag but no value found, 1 on success.
 */
int find_flag_value(LPWSTR *argv, int *argc, LPCWSTR flag, LPCWSTR long_flag, LPWSTR* dest);

/**
 * Convert a commandline into C-style argv and argc.
 * Returns argv, <argc> gets number of arguments.
 */
LPWSTR* parse_command_line(LPCWSTR args, int* argc);

/**
 * Parse a commandline potentialy without escaping of backslashes and qoutes.
 */
LPWSTR* parse_command_line_with(LPCWSTR args, int* argc, BOOL escape_backslash, BOOL escape_quotes);
