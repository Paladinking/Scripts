#define WIN32_LEAN_AND_MEAN
#define _NO_CRT_STDIO_INLINE
#include <windows.h>
#include "args.h"
#include "glob.h"
#include "printf.h"

const uint8_t utf8_len_table[256] = {
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1
};


int __CRTDECL _snwprintf_s(wchar_t* const, size_t, size_t,wchar_t const*, ...);
int __CRTDECL _snprintf_s(char* const, size_t, size_t, char const*, ...);

static inline uint8_t utf8_len(uint8_t b) {
    return utf8_len_table[b];
}

// Convert w from utf16 to utf8, storing the result in dest.
// Keep any partial utf16 sequences at the end of w for later processing.
bool convert_utf16_chunk(WString* w, String* dest) {
    if (IS_HIGH_SURROGATE(w->buffer[w->length - 1])) {
        if (!String_from_utf16_bytes(dest, w->buffer, w->length - 1)) {
            return false;
        }
        WString_remove(w, 0, w->length - 1);
    } else {
        if (!String_from_utf16_bytes(dest, w->buffer, w->length)) {
            return false;
        }
        WString_clear(w);
    }
    return true;
}

// Convert s from utf8 to utf16, storing the result in dest.
// Keep any partial utf8 sequences at the end of s for later processing.
bool convert_utf8_chunk(String* s, WString* dest) {
    uint32_t ix = s->length - 1;
    uint8_t *buf = (uint8_t*)s->buffer;
    if ((buf[ix] >= 0x80 && buf[ix] <= 0xBF) || utf8_len(buf[ix]) > 1) {
        while (buf[ix] >= 0x80 && buf[ix] <= 0xBF && ix > s->length - 4) {
            --ix;
        }
        --ix;
    }
    if (!WString_from_utf8_bytes(dest, s->buffer, ix + 1)) {
        return false;
    }
    String_remove(s, 0, ix + 1);
    return true;
}

typedef struct OutputState {
    bool last_cr;
    bool number;
    bool eol;
    bool number_nonblank;
    bool non_blank;
    uint64_t lineno;
} OutputState;


bool output_utf8_lines(HANDLE out, String* s, OutputState* state) {
    char fmt_buf[100];
    if (s->length == 0) {
        return true;
    }

    bool has_header = state->non_blank;
    if (s->buffer[0] != '\n' && s->buffer[0] != '\r') {
        state->non_blank = true;
    } else if (s->buffer[0] == '\n' && state->last_cr &&
               s->length > 1 && s->buffer[1] != '\n' &&
               s->buffer[1] != '\r') {
        state->non_blank = true;
    }
    DWORD w;
    uint32_t last_ix = 0;
    for (uint32_t ix = 0; ix < s->length; ++ix) {
        if ((s->buffer[ix] == '\n' && !state->last_cr) || 
            s->buffer[ix] == '\r') {
            state->last_cr = s->buffer[ix] == '\r';
            if (!has_header && (state->number || 
                (state->number_nonblank && state->non_blank))) {
                int len = _snprintf_s(fmt_buf, 100, 100, "%6llu\t", 
                                       state->lineno);

                if (!WriteFile(out, fmt_buf, len, &w, NULL) || w < len) {
                    return false;
                }
            }
            if (state->number || (state->number_nonblank && state->non_blank)) {
                ++state->lineno;
            }
            has_header = false;
            uint32_t l = ix - last_ix;
            if (!WriteFile(out, s->buffer + last_ix, 
                l, &w, NULL) || w < l) {
                return false;
            }
            if (state->eol) {
                if (!WriteFile(out, "$", 1, &w, NULL) || w < 1) {
                    return false;
                }
            }
            if (!WriteFile(out, s->buffer + ix, 1, &w, NULL) || w < 1) {
                return false;
            }
            last_ix = ix + 1;
            if (ix >= s->length - 1) {
                state->non_blank = false;
            } else if (s->buffer[ix + 1] == '\n' && state->last_cr) {
                if (ix >= s->length - 2) {
                    state->non_blank = false;
                } else {
                    state->non_blank = s->buffer[ix + 2] != '\r' &&
                                       s->buffer[ix + 2] != '\n';
                }
            } else {
                state->non_blank = s->buffer[ix + 1] != '\r' &&
                                   s->buffer[ix + 1] != '\n';
            }
        } else if (s->buffer[ix] == '\n') {
            state->last_cr = false;
            ++last_ix;
            if (!WriteFile(out, "\n", 1, &w, NULL) || w < 1) {
                return false;
            }
        } else {
            state->last_cr = false;
        }
    }
    if (last_ix < s->length) {
        if (!has_header && (state->number || state->number_nonblank)) {
            int len = _snprintf_s(fmt_buf, 100, 100, "%6llu\t", 
                                   state->lineno);

            if (!WriteFile(out, fmt_buf, len, &w, NULL) || w < len) {
                return false;
            }
        }
        uint32_t l = s->length - last_ix;
        if (!WriteFile(out, s->buffer + last_ix, 
            l, &w, NULL) || w < l) {
            return false;
        }
    }
    return true;
}

bool output_utf16_lines(HANDLE out, WString* s, OutputState* state) {
    wchar_t fmt_buf[100];
    if (s->length == 0) {
        return true;
    }

    bool has_header = state->non_blank;
    if (s->buffer[0] != L'\n' && s->buffer[0] != L'\r') {
        state->non_blank = true;
    } else if (s->buffer[0] == L'\n' && state->last_cr &&
               s->length > 1 && s->buffer[1] != L'\n' &&
               s->buffer[1] != L'\r') {
        state->non_blank = true;
    }
    DWORD w;
    uint32_t last_ix = 0;
    for (uint32_t ix = 0; ix < s->length; ++ix) {
        if ((s->buffer[ix] == L'\n' && !state->last_cr) || 
            s->buffer[ix] == L'\r') {
            state->last_cr = s->buffer[ix] == L'\r';
            if (!has_header && (state->number || 
                (state->number_nonblank && state->non_blank))) {
                int len = _snwprintf_s(fmt_buf, 100, 100, L"%6llu\t", 
                                       state->lineno);

                if (!WriteConsoleW(out, fmt_buf, len, &w, NULL) || w < len) {
                    return false;
                }
            }
            if (state->number || (state->number_nonblank && state->non_blank)) {
                ++state->lineno;
            }
            has_header = false;
            uint32_t l = ix - last_ix;
            if (!WriteConsoleW(out, s->buffer + last_ix, 
                l, &w, NULL) || w < l) {
                return false;
            }
            if (state->eol) {
                if (!WriteConsoleW(out, L"$", 1, &w, NULL) || w < 1) {
                    return false;
                }
            }
            if (!WriteConsoleW(out, s->buffer + ix, 1, &w, NULL) || w < 1) {
                return false;
            }
            last_ix = ix + 1;
            if (ix >= s->length - 1) {
                state->non_blank = false;
            } else if (s->buffer[ix + 1] == L'\n' && state->last_cr) {
                if (ix >= s->length - 2) {
                    state->non_blank = false;
                } else {
                    state->non_blank = s->buffer[ix + 2] != L'\r' &&
                                       s->buffer[ix + 2] != L'\n';
                }
            } else {
                state->non_blank = s->buffer[ix + 1] != L'\r' &&
                                   s->buffer[ix + 1] != L'\n';
            }
        } else if (s->buffer[ix] == L'\n') {
            state->last_cr = false;
            ++last_ix;
            if (!WriteConsoleW(out, L"\n", 1, &w, NULL) || w < 1) {
                return false;
            }
        } else {
            state->last_cr = false;
        }
    }
    if (last_ix < s->length) {
        if (!has_header && (state->number || state->number_nonblank)) {
            int len = _snwprintf_s(fmt_buf, 100, 100, L"%6llu\t", 
                                   state->lineno);

            if (!WriteConsoleW(out, fmt_buf, len, &w, NULL) || w < len) {
                return false;
            }
        }
        uint32_t l = s->length - last_ix;
        if (!WriteConsoleW(out, s->buffer + last_ix, 
            l, &w, NULL) || w < l) {
            return false;
        }
    }
    return true;
}


bool read_console_chunked(HANDLE in, bool number, bool eol, bool number_nonblank) {
    HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD type = GetFileType(out);
    bool parse_lines = number || eol || number_nonblank;
    OutputState state;
    if (parse_lines) {
        state.non_blank = false;
        state.last_cr = false;
        state.number_nonblank = number_nonblank;
        state.number = number;
        state.eol = eol;
        state.lineno = 1;
    }
    if (in == INVALID_HANDLE_VALUE) {
        return false;
    }
    WString w;
    if (!WString_create_capacity(&w, 0x500000 + 5)) {
        return false;
    }
    String s;
    if (type != FILE_TYPE_CHAR) {
        if (!String_create_capacity(&s, 0x500000)) {
            WString_free(&w);
            return false;
        }
    }
    bool could_write = true;
    while (could_write) {
        DWORD r;
        if (!ReadConsoleW(in, w.buffer + w.length, 0x500000, &r, NULL) || r == 0) {
            break;
        }
        w.length += r;
        uint32_t written = 0;
        if (type == FILE_TYPE_CHAR) {
            // If output is a console, just write the utf16 data.
            if (parse_lines) {
                could_write = output_utf16_lines(out, &w, &state);
                w.length = 0;
                continue;
            }
            while (written < w.length) {
                if (!WriteConsoleW(out, w.buffer + written, w.length - written,
                               &r, NULL) || r == 0) {
                    could_write = false;
                    break;
                }
                written += r;
            }
            w.length = 0;
            continue;
        }
        w.buffer[w.length] = L'\0';
        // Console can only properly display unicode from utf16, convert
        if (!convert_utf16_chunk(&w, &s)) {
            goto fail;
        }
        if (parse_lines) {
            could_write = output_utf8_lines(out, &s, &state);
            continue;
        }
        while (written < s.length) {
            if (!WriteFile(out, s.buffer + written, s.length - written,
                           &r, NULL) || r == 0) {
                could_write = false;
                break;
            }
            written += r;
        }
    }
    if (could_write && w.length > 0) {
        String_from_utf16_bytes(&s, w.buffer, w.length);
        if (parse_lines) {
            output_utf8_lines(out, &s, &state);
        } else {
            WriteFile(out, s.buffer, s.length, NULL, NULL);
        }
    }
    WString_free(&w);
    if (type != FILE_TYPE_CHAR) {
        String_free(&s);
    }
    return true;
fail:
    WString_free(&w);
    if (type != FILE_TYPE_CHAR) {
        String_free(&s);
    }
    return false;
}


bool read_file_chunked(HANDLE in, bool number, bool eol, bool number_nonblank) {
    HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD type = GetFileType(out);
    bool parse_lines = number || eol || number_nonblank;
    OutputState state;
    if (parse_lines) {
        state.non_blank = false;
        state.last_cr = false;
        state.number_nonblank = number_nonblank;
        state.number = number;
        state.eol = eol;
        state.lineno = 1;
    }
    if (in == INVALID_HANDLE_VALUE) {
        return false;
    }
    String s;
    if (!String_create_capacity(&s, 0x1000000 + 5)) {
        return false;
    }
    WString w;
    if (type == FILE_TYPE_CHAR) {
        if (!WString_create_capacity(&w, 0x1000000)) {
            String_free(&s);
            return false;
        }
    }
    bool could_write = true;
    while (could_write) {
        DWORD r;
        if (!ReadFile(in, s.buffer + s.length, 0x1000000, &r, NULL) || r == 0) {
            break;
        }
        s.length += r;
        uint32_t written = 0;
        if (type != FILE_TYPE_CHAR) {
            // If output is not a console, just write the (assumed) utf8 data.
            if (parse_lines) {
                could_write = output_utf8_lines(out, &s, &state);
                s.length = 0;
                continue;
            }
            while (written < s.length) {
                if (!WriteFile(out, s.buffer + written, s.length - written, 
                               &r, NULL) || r == 0) {
                    could_write = false;
                    break;
                }
                written += r;
            }
            s.length = 0;
            continue;
        }
        s.buffer[s.length] = L'\0';
        // Console can only properly display unicode from utf16, convert
        if (!convert_utf8_chunk(&s, &w)) {
            goto fail;
        }
        if (parse_lines) {
            could_write = output_utf16_lines(out, &w, &state);
            continue;
        }
        while (written < w.length) {
            if (!WriteConsoleW(out, w.buffer + written, w.length - written, 
                        &r, NULL) || r == 0) {
                could_write = false;
                break;
            } 
            written += r;
        }
    }
    if (could_write && s.length > 0) {
        WString_from_utf8_bytes(&w, s.buffer, s.length);
        if (parse_lines) {
            output_utf16_lines(out, &w, &state);
        } else {
            WriteConsoleW(out, w.buffer, w.length, NULL, NULL);
        }
    }
    String_free(&s);
    if (type == FILE_TYPE_CHAR) {
        WString_free(&w);
    }
    return true;
fail:
    String_free(&s);
    if (type == FILE_TYPE_CHAR) {
        WString_free(&w);
    }
    return false;
}


const wchar_t *HELP_MESSAGE =
    L"Usage: %s [OPTION]... DIR\n"
    L"Type the content of a file to stdout\n\n"
    L"-b, --number-nonblank             number nonempty lines, overrides -n\n"
    L"-E, --show-ends                   display $ at end of each line\n"
    L"-h, --help                        display this message and exit\n"
    L"-n, --number                      number all lines\n";


int main() {
    wchar_t* args = GetCommandLineW();
    int argc;
    wchar_t** argv = glob_command_line(args, &argc);

    FlagInfo flags[] = {
        {L'n', L"number"},
        {L'E', L"show-ends"},
        {L'b', L"number-nonblank"},
        {L'h', L"help"}
    };
    const uint32_t flag_count = sizeof(flags) / sizeof(FlagInfo);
    ErrorInfo err;
    if (!find_flags(argv, &argc, flags, flag_count, &err)) {
        wchar_t *err_msg = format_error(&err, flags, flag_count);
        if (err_msg) {
            _wprintf_e(L"%s\n", err_msg);
            Mem_free(err_msg);
        }
        if (argc > 0) {
            _wprintf_e(L"Run '%s --help' for more information\n", argv[0]);
        }
        Mem_free(argv);
        return 1;
    }

    if (flags[3].count > 0) {
        _wprintf(HELP_MESSAGE, argv[0]);
        Mem_free(argv);
        return 0;
    }

    bool number = flags[0].count > 0;
    bool show_ends = flags[1].count > 0;
    bool number_nonblank = flags[2].count > 0;

    if (argc < 2) {
        HANDLE in = GetStdHandle(STD_INPUT_HANDLE);
        if (GetFileType(in) == FILE_TYPE_CHAR) {
            read_console_chunked(in, number, show_ends, number_nonblank);
        } else {
            read_file_chunked(in, number, show_ends, number_nonblank);
        }

        Mem_free(argv);
        return 0;
    }

    for (uint32_t ix = 1; ix < argc; ++ix) {
        HANDLE in;
        if (argv[ix][0] == L'-' && argv[ix][1] == L'\0') {
            in = GetStdHandle(STD_INPUT_HANDLE);
            if (GetFileType(in) == FILE_TYPE_CHAR) {
                read_console_chunked(in, number, show_ends, number_nonblank);
            } else {
                read_file_chunked(in, number, show_ends, number_nonblank);
            }
        } else {
            in = CreateFileW(argv[ix], GENERIC_READ,
                             FILE_SHARE_READ, NULL,
                             OPEN_EXISTING, 0, NULL);
            if (in == INVALID_HANDLE_VALUE) {
                DWORD err = GetLastError();
                if (err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND ||
                    err == ERROR_BAD_ARGUMENTS) {
                    _wprintf_e(L"Failed opening '%s', No such file or directory\n", argv[ix]);
                } else if (err == ERROR_ACCESS_DENIED) {
                    if (is_directory(argv[ix])) {
                        _wprintf_e(L"Cannot read '%s', Is a directory\n", argv[ix]);
                    } else {
                        _wprintf_e(L"Failed opening '%s', Permissing denied\n", argv[ix]);
                    }
                } else {
                    wchar_t buf[512];
                    DWORD res = FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | 
                                               FORMAT_MESSAGE_IGNORE_INSERTS,
                                               NULL, GetLastError(), 0, buf, 512, NULL);
                    if (res == 0) {
                        _wprintf_e(L"Failed opening file '%s', Unkown error\n", argv[ix], buf);
                    } else {
                        _wprintf_e(L"Failed opening file '%s', %s", argv[ix], buf);
                    }
                }
            } else {
                FILE_BASIC_INFO info;
                if (GetFileInformationByHandleEx(in, FileBasicInfo, &info, sizeof(info))) {
                    if (info.FileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                        _wprintf_e(L"Cannot read '%s', Is a directory\n", argv[ix]);
                        CloseHandle(in);
                        continue;
                    }
                }
                read_file_chunked(in, number, show_ends, number_nonblank);
                CloseHandle(in);
            }
        }
    }
    Mem_free(argv);
    return 0;
}
