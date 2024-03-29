#define UNICODE
#include "args.h"

DWORD find_flag(LPWSTR* argv, int* argc, LPCWSTR flag, LPCWSTR long_flag) {
    DWORD count = 0;
	for (int i = 1; i < *argc; ++i) {
		if (wcscmp(argv[i], flag) == 0 || wcscmp(argv[i], long_flag) == 0) {
			count += 1;
			for (int j = i + 1; j < *argc; ++j) {
				argv[j - 1] = argv[j];
			}
			--(*argc);
			--i;
		}
	}
	return count;
}

LPWSTR* parse_command_line_with(const LPCWSTR args, int* argc, BOOL escape_backslash, BOOL escape_quotes) {
    HANDLE heap = GetProcessHeap();
    *argc = 0;
    size_t i = 0;
    size_t size = 0;
    BOOL in_quotes = FALSE;
    while (args[i] == L' ' || args[i] == L'\t') {
        ++i;
    }
    if (args[i] != L'\0') {
        *argc = 1;
    }
    while (1) {
        if (args[i] == L'\0' || (!in_quotes && (args[i] == L' ' || args[i] == L'\t'))) {
            while (args[i] == L' ' || args[i] == L'\t') {
                ++i;
            }
            if (args[i] == L'\0') {
                break;
            } else {
                *argc += 1;
            }
        } else if (escape_backslash && args[i] == L'\\') {
            size_t count = 1;
            while (args[i + count] == L'\\') {
                count += 1;
            }
            if (args[i + count] == L'"') {
                if (count % 2 == 0) {
                    size += count / 2;
                } else {
                    size += count / 2 + 1;
                    ++i;
                }
            } else {
                size += count;
            }
            i += count;
        } else if (args[i] != L'"') {
            ++size;
            ++i;
        } else {
            if (in_quotes && args[i + 1] == L'"') {
                ++size;
                ++i;
                in_quotes = !in_quotes;
            } else {
                in_quotes = !in_quotes;
            }
            ++i;
        }
    }
    size = size * sizeof(WCHAR) + (sizeof(LPWSTR) + sizeof(WCHAR)) * (*argc);
    LPWSTR* argv = HeapAlloc(heap, 0, size);
    if (argv == NULL) {
        return NULL;
    } 
    LPWSTR args_dest = (LPWSTR) (argv + *argc);
    LPWSTR base = args_dest;
    in_quotes = FALSE;
    i = 0;
    int arg = 0;
    LPWSTR dest = args_dest;
    while (arg < *argc) {
        argv[arg] = args_dest;
        ++arg;
        while (args[i] == L' ' || args[i] == L'\t') {
            ++i;
        }
        while (1) {
            if (args[i] == L'\0' || (!in_quotes && (args[i] == L' ' || args[i] == L'\t'))) {
                *args_dest = L'\0';
                ++args_dest;
                ++i;
                break;
            } else if (escape_backslash && args[i] == L'\\') {
                int count = 1;
                while (args[i + count] == L'\\') {
                    count += 1;
                }
                if (args[i + count] == L'"') {
                    for (int j = 0; j < count / 2; ++j) {
                        *args_dest = '\\';
                        ++args_dest;
                    }
                    if (count % 2 == 1) {
                        *args_dest = '"';
                        ++args_dest;
                        ++i;
                    }
                } else {
                    for (int j = 0; j < count; ++j) {
                        *args_dest = '\\';
                        ++args_dest;
                    }
                }
                i += count;
            } else if (args[i] != L'"') {
                *args_dest = args[i];
                ++args_dest;
                ++i;
            } else {
                if (in_quotes && args[i + 1] == L'"') {
                    *args_dest = L'"';
                    ++args_dest;
                    ++i;
                    in_quotes = !in_quotes;
                } else {
                    in_quotes = !in_quotes;
                }
                ++i;
            }
        }
    }
    return argv;
}



LPWSTR* parse_command_line(const LPCWSTR args, int* argc) {
    return parse_command_line_with(args, argc, TRUE, TRUE);
}
