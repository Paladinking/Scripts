#define UNICODE
#include "args.h"
#include "mem.h"

BOOL parse_uint(const wchar_t* s, uint64_t* i) {
    uint64_t n = 0;
    if (*s == L'0' && (*(s + 1) == L'x' || (*s + 1) == L'X')) { // Hex
        s += 2;
        while (*s != L'\0') {
            wchar_t c = *s;
            if (n > 0xfffffffffffffff) {
                return FALSE; // Overflow
            }
            n = n << 4;
            if (c >= L'0' && c <= L'9') {
                n += c - L'0';
            } else if (c >= L'a' && c <= L'f') {
                n += (c - L'a') + 10;
            } else if (c >= L'A' && c <= L'F') {
                n += (c - L'A') + 10;
            } else {
                return FALSE;
            }
            ++s;
        }
    } else if (*s == L'0') { // Octal
        s += 1;
        while (*s != L'\0') {
            wchar_t c = *s;
            if (n > 0x1fffffffffffffff) {
                return FALSE; // Overflow
            }
            n = n << 3;
            if (c >= L'0' && c <= L'7') {
                n += c - L'0';
            } else {
                return FALSE;
            }
            ++s;
        }
    } else { // Base 10
        while (*s != L'\0') {
            wchar_t c = *s;
            if (c < L'0' || c > L'9') {
                return FALSE;
            }
            uint64_t v = c - L'0';
            if (n > 0x1999999999999999) {
                return FALSE;
            } else if (n == 0x1999999999999999) {
                if (v > 5) {
                    return FALSE;
                }
            }
            n = n * 10;
            n += v;
            ++s;
        }
    }
    *i = n;
    return TRUE;
}

BOOL parse_sint(const wchar_t* s, int64_t* i) {
    BOOL negative = FALSE;
    if (*s == L'-') {
        negative = TRUE;
        ++s;
    }
    uint64_t u;
    if (!parse_uint(s, &u)) {
        return FALSE;
    }
    if (negative) {
        if (u > 0x8000000000000000) {
            return FALSE;
        } else if (u == 0x8000000000000000) {
            *i = INT64_MIN;
        } else {
            *i = -((int64_t)u);
        }
    } else {
        if (u > INT64_MAX) {
            return FALSE;
        } else {
            *i = (int64_t) u;
        }
    }
    return TRUE;
}


// Counts the number of leading matching characters.
DWORD wcsmatch(const wchar_t* a, const wchar_t* b) {
    DWORD ix = 0;
    while (a[ix] != L'\0' && b[ix] != L'\0') {
        if (a[ix] != b[ix]) {
            return ix;
        }
        ++ix;
    }
    return ix;
}

DWORD prefix_len(const wchar_t* str, const wchar_t* full, BOOL has_val) {
    DWORD len;
    if (has_val) {
        const wchar_t* eq = wcschr(str, '=');
        if (eq == NULL) {
            len = wcslen(str);
        } else {
            len = eq - str;
        }
    } else {
        len = wcslen(str);
    }
    if (wcsncmp(str, full, len) == 0) {
        return len;
    }
    return 0;
}

BOOL parse_argument(LPWSTR val, FlagValue* valid, unsigned ix, ErrorInfo* err) {
    if (valid->type & FLAG_STRING) {
        valid->str = val;
        valid->has_value = 1;
        return TRUE;
    }
    if (valid->type & FLAG_INT) {
        if (!parse_sint(val, &valid->sint)) {
            err->type = FLAG_INVALID_VALUE;
            err->ix = ix;
            err->value = val;
            return FALSE;
        }
        valid->has_value = 1;
        return TRUE;
    }
    if (valid->type & FLAG_UINT) {
        if (!parse_uint(val, &valid->uint)) {
            err->type = FLAG_INVALID_VALUE;
            err->ix = ix;
            err->value = val;
            return FALSE;
        }
        valid->has_value = 1;
        return TRUE;
    }
    EnumValue* vals = valid->enum_values;
    unsigned count = valid->enum_count;

    DWORD longest_prefix = 0;
    unsigned longest_ix = 0;
    BOOL collision = FALSE;
    for (unsigned j = 0; j < count; ++j) {
        for (unsigned i = 0; i < vals[j].count; ++i) {
            DWORD len = prefix_len(val, vals[j].values[i], FALSE);
            if (len == 0) {
                continue;
            }
            if (len > longest_prefix) {
                longest_prefix = len;
                longest_ix = j;
                collision = FALSE;
            } else if (len == longest_prefix) {
                if (longest_ix != j) {
                    collision = TRUE;
                }
            }
        }
    }
    if (longest_prefix == 0) {
        err->type = FLAG_INVALID_VALUE;
        err->value = val;
        err->ix = ix;
        return FALSE;
    } 
    if (collision) {
        err->type = FLAG_AMBIGUOS_VALUE;
        err->value = val;
        err->ix = ix;
        return FALSE;
    }
    valid->has_value = 1;
    valid->uint = longest_ix;
    return TRUE;
}

BOOL find_flags(LPWSTR* argv, int* argc, FlagInfo* flags, DWORD flag_count, ErrorInfo* err) {
    DWORD order = 1;
    for (DWORD ix = 0; ix < flag_count; ++ix) {
        flags[ix].count = 0;
        flags[ix].ord = 0;
        flags[ix].shared = 0;
        if (flags[ix].value != NULL) {
            flags[ix].value->has_value = 0;
        }
    }
    for (DWORD ix = 0; ix < flag_count; ++ix) {
        if (flags[ix].long_name == NULL) {
            continue;
        }
        for (DWORD j = ix + 1; j < flag_count; ++j) {
            if (flags[j].long_name == NULL) {
                continue;
            }
            DWORD match = wcsmatch(flags[ix].long_name, flags[j].long_name);
            if (match > flags[ix].shared) {
                flags[ix].shared = match;
            }
            if (match > flags[j].shared) {
                flags[j].shared = match;
            }
        }
    }

    for (int ix = 0; ix < *argc; ++ix) {
        if (argv[ix][0] != L'-') {
            continue;
        }
        if (argv[ix][1] == L'-') {
            if (argv[ix][2] == L'\0') {
                return TRUE;
            }
            unsigned char found = 0;
            for (DWORD j = 0; j < flag_count; ++j) {
                if (flags[j].long_name == NULL) {
                    continue;
                }
                DWORD len = prefix_len(argv[ix] + 2, flags[j].long_name, TRUE);
                if (len == 0) {
                    continue;
                }
                if (len <= flags[j].shared) {
                    err->type = FLAG_AMBIGUOS;
                    err->value = argv[ix] + 2;
                    if (argv[ix][len + 2] == L'=') {
                        argv[ix][len + 2] = L'\0';
                    }
                    err->ix = j;
                    return FALSE;
                }
                if (flags[j].value != NULL) {
                    if (argv[ix][len + 2] == L'\0') {
                        ++flags[j].count;
                        flags[j].ord = order++;
                        found = 1;
                        if (flags[j].value->type & FLAG_OPTONAL_VALUE) {
                            flags[j].value->has_value = 0;
                            break;
                        }
                        if (ix + 1 < *argc) {
                            for (int j = ix + 1; j < *argc; ++j) {
                                argv[j - 1] = argv[j];
                            }
                            --(*argc);
                            if (!parse_argument(argv[ix], flags[j].value, j, err)) {
                                return FALSE;
                            }
                        } else {
                            err->type = FLAG_MISSING_VALUE;
                            err->value = argv[ix] + 2;
                            err->ix = j;
                            return FALSE;
                        }
                    } else if (argv[ix][len + 2] == L'=') {
                        ++flags[j].count;
                        flags[j].ord = order++;
                        found = 1;
                        if (!parse_argument(argv[ix] + len + 3, flags[j].value, j, err)) {
                            return FALSE;
                        }
                    } else {
                        continue;
                    }
                    break;
                } else {
                    if (argv[ix][len + 2] == L'=') {
                        err->type = FLAG_UNEXPECTED_VALUE;
                        err->ix = j;
                        err->value = argv[ix] + 2;
                        argv[ix][len + 2] = L'\0';
                        return FALSE;
                    }
                    ++flags[j].count;
                    flags[j].ord = order++;
                    found = 1;
                    break;
                }
            }
            if (!found) {
                err->type = FLAG_UNKOWN;
                err->value = argv[ix] + 2;
                err->ix = flag_count;
                return FALSE;
            }
        } else if (argv[ix][1] == L'\0') {
            continue;
        } else {
            for (int i = 1; argv[ix][i] != L'\0'; ++i) {
                unsigned char found = 0;
                DWORD j = 0;
                for (; j < flag_count; ++j) {
                    if (flags[j].short_name == argv[ix][i]) {
                        ++flags[j].count;
                        flags[j].ord = order++;
                        found = 1;
                        break;
                    }
                }
                if (!found) {
                    err->type = FLAG_UNKOWN;
                    err->value = argv[ix];
                    err->ix = flag_count;
                    argv[ix][0] = argv[ix][i];
                    argv[ix][1] = L'\0';
                    return FALSE;
                } else if (flags[j].value != NULL) {
                    if (argv[ix][i + 1] == L'\0') {
                        if (ix + 1 == *argc)  {
                            if (flags[j].value->type & FLAG_REQUIRED_VALUE) {
                                err->type = FLAG_MISSING_VALUE;
                                err->value = argv[ix];
                                err->ix = j;
                                argv[ix][0] = argv[ix][i];
                                argv[ix][1] = L'\0';
                                return FALSE;
                            }
                            flags[j].value = NULL;
                        } else {
                            for (int j = ix + 1; j < *argc; ++j) {
                                argv[j - 1] = argv[j];
                            }
                            --(*argc);
                            if (!parse_argument(argv[ix], flags[j].value, j, err)) {
                                return FALSE;
                            }
                            break;
                        }
                    } else {
                        if (!parse_argument(argv[ix] + i + 1, flags[j].value, j, err)) {
                            return FALSE;
                        }
                        break;
                    }
                }
            }
        }
        for (int j = ix + 1; j < *argc; ++j) {
            argv[j - 1] = argv[j];
        }
        --(*argc);
        --ix;
    }
    return TRUE;
}

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
    unsigned short_len = wcslen(flag);
    unsigned long_len = wcslen(long_flag);
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
        } else if (wcsncmp(argv[i], long_flag, long_len) == 0) {
            if (argv[i][long_len] != L'\0' && argv[i][long_len] != L'=') {
                continue;
            }
            if (argv[i][long_len] == L'=') {
                *dest = argv[i] + long_len + 1;
                for (int j = i + 1; j < *argc; ++j) {
                    argv[j - i] = argv[j];
                }
                *argc -= 1;
                return 1;
            } else {
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
    LPWSTR* argv = Mem_alloc(size);
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
