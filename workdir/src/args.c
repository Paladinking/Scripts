#define UNICODE
#include "args.h"

DWORD find_flag(LPWSTR *argv, int *argc, LPCWSTR flag, LPCWSTR long_flag) {
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

int find_flag_value(LPWSTR *argv, int *argc, LPCWSTR flag, LPCWSTR long_flag,
                    LPWSTR* dest) {
    int short_len = wcslen(flag);
    for (int i = 0; i < *argc; ++i) {
        if (wcsncmp(argv[i], flag, short_len) == 0) {
            if (argv[i][short_len] == L'\0') {
                if (i + 1 == *argc) {
                    return -1;
                }
                *dest = argv[i + 1];
                for (int j = i + 2; j < *argc; ++j) {
                    argv[j - 2] = argv[j];
                }
                *argc -= 2;
                return 1;
            } else {
                *dest = argv[i] + short_len;
                for (int j = i + 1; j < *argc; ++j) {
                    argv[j - i] = argv[j];
                }
                *argc -= 1;
                return 1;
            }
        } else if (wcscmp(argv[i], long_flag) == 0) {
            if (i + 1 == *argc) {
                return -1;
            }
            *dest = argv[i + 1];
            for (int j = i + 2; j < *argc; ++j) {
                argv[j - 2] = argv[j];
            }
            *argc -= 2;
            return 1;
        }
    }
    return 0;
}

BOOL get_arg_len(const wchar_t* cmd, size_t* ix, size_t* len, BOOL* quoted, unsigned flags) {
    BOOL terminal_chars = flags & ARG_OPTION_TERMINAL_OPERANDS;
    BOOL escape_backslash = flags & ARG_OPTION_BACKSLASH_ESCAPE;
    BOOL in_quotes = FALSE;
    *quoted = FALSE;
    *len = 0;

    while (cmd[*ix] == L' ' || cmd[*ix] == L'\t') {
        ++(*ix);
    }

    unsigned start = *ix;
    if (cmd[*ix] == L'\0') {
        return FALSE;
    }

    if (terminal_chars && (cmd[*ix] == L'>' || cmd[*ix] == L'<' || cmd[*ix] == L'&' || cmd[*ix] == L'|')) {
        if (cmd[*ix + 1] == cmd[*ix]) {
            ++(*ix);
            ++(*len);
        }
        ++(*ix);
        ++(*len);
        return TRUE;
    }

    while (1) {
        if (cmd[*ix] == L'\0') {
            return TRUE;
        }
        if (!in_quotes && (cmd[*ix] == L' ' || cmd[*ix] == L'\t')) {
            return TRUE;
        }
        if (!in_quotes && terminal_chars && (cmd[*ix] == L'>' || 
            cmd[*ix] == L'<' || cmd[*ix] == L'&' || cmd[*ix] == L'|')) {
            return TRUE;
        }
        if (escape_backslash && cmd[*ix] == L'\\') {
            size_t count = 1;
            while (cmd[*ix + count] == L'\\') {
                count += 1;
            }
            if (cmd[*ix + count] == L'"') {
                if (count % 2 == 0) {
                    *len += count / 2;
                } else {
                    *len += count / 2 + 1;
                    ++(*ix);
                }
            } else {
                *len += count;
            }
            *ix += count;
        } else if (cmd[*ix] == L'"') {
            *quoted = TRUE;
            if (in_quotes && cmd[*ix + 1] == L'"') {
                ++(*len);
                ++(*ix);
                in_quotes = !in_quotes;
            } else {
                in_quotes = !in_quotes;
            }
            ++(*ix);

        } else {
            ++(*ix);
            ++(*len);
        }
    }
}

wchar_t* get_arg(const wchar_t* cmd, size_t *ix, wchar_t* dest, unsigned flags) {
    BOOL in_quotes = FALSE;
    BOOL terminal_chars = flags & ARG_OPTION_TERMINAL_OPERANDS;
    BOOL escape_backslash = flags & ARG_OPTION_BACKSLASH_ESCAPE;

    while (cmd[*ix] == L' ' || cmd[*ix] == L'\t') {
        ++(*ix);
    }

    unsigned start = *ix;
    if (cmd[*ix] == L'\0') {
        return NULL;
    }

    if (terminal_chars && (cmd[*ix] == L'>' || cmd[*ix] == L'<' || cmd[*ix] == L'&' || cmd[*ix] == L'|')) {
        if (cmd[*ix + 1] == cmd[*ix]) {
            ++(*ix);
            dest[0] = cmd[*ix];
            ++dest;
        }
        dest[0] = cmd[*ix];
        dest[1] = L'\0';
        ++(*ix);
        return dest + 2;
    }

    while (1) {
        if (cmd[*ix] == L'\0') {
            dest[0] = L'\0';
            ++dest;
            return dest;
        }
        if (!in_quotes && (cmd[*ix] == L' ' || cmd[*ix] == L'\t')) {
            dest[0] = L'\0';
            ++dest;
            return dest;
        }
        if (!in_quotes && terminal_chars && (cmd[*ix] == L'>' || 
            cmd[*ix] == L'<' || cmd[*ix] == L'&' || cmd[*ix] == L'|')) {
            dest[0] = L'\0';
            ++dest;
            return dest;
        }
        if (escape_backslash && cmd[*ix] == L'\\') {
            size_t count = 1;
            while (cmd[*ix + count] == L'\\') {
                count += 1;
            }
            if (cmd[*ix + count] == L'"') {
                for (int i = 0; i < count / 2; ++i) {
                    dest[0] = '\\';
                    ++dest;
                }
                if (count % 2 == 1) {
                    dest[0] = L'"';
                    ++dest;
                    ++(*ix);
                }
            } else {
                for (int i = 0; i < count; ++i) {
                    dest[0] = L'\\';
                    ++dest;
                }
            }
            *ix += count;
        } else if (cmd[*ix] == L'"') {
            if (in_quotes && cmd[*ix + 1] == L'"') {
                dest[0] = L'"';
                ++dest;
                ++(*ix);
                in_quotes = !in_quotes;
            } else {
                in_quotes = !in_quotes;
            }
            ++(*ix);

        } else {
            dest[0] = cmd[*ix];
            ++(*ix);
            ++dest;
        }
    }
}

LPWSTR* parse_command_line_with(const LPCWSTR args, int* argc, unsigned options) {
    HANDLE heap = GetProcessHeap();
    *argc = 0;
    size_t i = 0;
    size_t size = 0;

    size_t len = 0;
    BOOL quoted;
    while (get_arg_len(args, &i, &len, &quoted, options)) {
        size += len;
        *argc += 1;
    }

    size = size * sizeof(WCHAR) + (sizeof(LPWSTR) + sizeof(WCHAR)) * (*argc);
    LPWSTR* argv = HeapAlloc(heap, 0, size);
    if (argv == NULL) {
        return NULL;
    } 
    LPWSTR args_dest = (LPWSTR) (argv + *argc);
    LPWSTR dest = args_dest;
    i = 0;
    int arg = 0;
    while (1) {
        wchar_t* next = get_arg(args, &i, dest, options);
        if (next == NULL) {
            break;
        }
        argv[arg] = dest;
        ++arg;
        dest = next;
    }
    return argv;
}

LPWSTR *parse_command_line(const LPCWSTR args, int *argc) {
    return parse_command_line_with(args, argc, ARG_OPTION_STD);
}
