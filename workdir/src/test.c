#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "args.h"
#include "glob.h"
#include "printf.h"

const uint8_t utf8_len_table[] = {
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

const uint8_t utf8_valid_table[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 
    0xa0, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x9f, 0x80, 0x80, 0x90, 0x80, 0x80, 0x80, 0x8f, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static inline uint8_t utf8_len(uint8_t b) {
    return utf8_len_table[b];
}

bool valid_utf8(String* s) {
    uint32_t ix = 0;
    while (ix < s->length) {
        uint8_t b = s->buffer[ix];
        if (b <= 0x7F) {
            ++ix;
            continue;
        }
        uint8_t v = utf8_valid_table[b - 0x7F];
        if (v == 0) {
            return false;
        }
        uint8_t len = utf8_len(b);
        if (ix + len >= s->length) {
            return false;
        }
        for (uint32_t i = ix + 2; i < ix + len; ++i) {
            uint8_t b2 = s->buffer[i];
            if (b2 < 0x80 || b2 > 0xBF) {
                return false;
            }
        }
        uint8_t b2 = s->buffer[ix + 1];
        ix += len;
        if (v & 1) {
            if (b2 < 0x80 | b2 > v) {
                return false;
            }
        } else {
            if (b2 < v || b2 > 0xBF) {
                return false;
            }
        }
    }
    return true;
}

bool convert_chunk(String* s, WString* dest) {
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

bool read_file_chunked(HANDLE in) {
    HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD type = GetFileType(out);

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
        if (!convert_chunk(&s, &w)) {
            goto fail;
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
        WriteConsoleW(out, w.buffer, w.length, NULL, NULL);
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


int main() {
    wchar_t* args = GetCommandLineW();
    int argc;
    wchar_t** argv = parse_command_line(args, &argc);

    if (argc < 2) {
        read_file_chunked(GetStdHandle(STD_INPUT_HANDLE));

        Mem_free(argv);
        return 0;
    }

    for (uint32_t ix = 1; ix < argc; ++ix) {
        HANDLE in;
        if (argv[ix][0] == L'-' && argv[ix][1] == L'\0') {
            in = GetStdHandle(STD_INPUT_HANDLE);
            read_file_chunked(in);
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
                read_file_chunked(in);
                CloseHandle(in);
            }
        }
    }

    Mem_free(argv);
    return 0;
}
