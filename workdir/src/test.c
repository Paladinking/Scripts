#ifndef UNICODE
#define UNICODE
#endif

#include "args.h"
#include "glob.h"
#include "json.h"
#include "match_node.h"
#include "printf.h"
#include "subprocess.h"
#include "path_utils.h"
#include "cli.h"

bool add_node(NodeBuilder *builder, const char *key, HashMap *extr_map,
              MatchNode *node, WString *workbuf) {
    if (strcmp(key, "&FILE") == 0) {
        NodeBuilder_add_files(builder, true, node);
    } else if (strcmp(key, "&FILELIKE") == 0) {
        NodeBuilder_add_files(builder, false, node);
    } else if (strcmp(key, "&DEFAULT") == 0) {
        NodeBuilder_add_any(builder, node);
    } else if (key[0] == '&') {
        DynamicMatch *dyn = HashMap_Value(extr_map, key);
        if (dyn != NULL) {
            NodeBuilder_add_dynamic(builder, dyn, node);
        } else {
            _wprintf(L"Invalid extern \"%S\"\n", key);
            return false;
        }
    } else {
        if (!WString_from_utf8_str(workbuf, key)) {
            _wprintf(L"Invalid static \"%S\"\n", key);
            return false;
        }
        NodeBuilder_add_fixed(builder, workbuf->buffer, workbuf->length, node);
    }
    return true;
}

bool append_file(const char *str, const wchar_t *filename) {
    HANDLE file = CreateFileW(filename, FILE_APPEND_DATA, 0, NULL, OPEN_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL, NULL);
    unsigned len = strlen(str);
    DWORD w;
    unsigned written = 0;
    while (written < len) {
        if (!WriteFile(file, str + written, len - written, &w, NULL)) {
            CloseHandle(file);
            return false;
        }
        written += w;
    }
    CloseHandle(file);
    return true;
}

// replace all instances of $
void substitute_commands(wchar_t *str, DWORD *len, DWORD capacity) {
    wchar_t *pos = str;
    if (*len == 0) {
        return;
    }
    String outbuf;
    String_create(&outbuf);
    WString wbuf;
    WString_create(&wbuf);

    wchar_t last = str[*len - 1];
    wchar_t end_par = 0;
    if (last == L')' || last == L'}' || last == L']') {
        end_par = last;
    }
    str[*len - 1] = L'\0';
    while (pos < str + *len) {
        wchar_t *dlr = wcschr(pos, L'$');
        if (dlr == NULL) {
            break;
        }
        pos = dlr + 1;
        wchar_t par = *pos;
        if (par == L'(') {
            par = L')';
        } else if (par == L'{') {
            par = L'}';
        } else if (par == L'[') {
            par = L']';
        } else {
            continue;
        }
        wchar_t *close = wcschr(pos, par);
        bool at_end = false;
        if (close == NULL) {
            if (end_par == par) {
                at_end = true;
                close = str + *len - 1;
            } else {
                break;
            }
        }
        DWORD cmd_len = close - pos + 2;
        *close = L'\0';
        String_clear(&outbuf);
        WString_clear(&wbuf);
        unsigned long errorcode;
        if (!subprocess_run(pos + 1, &outbuf, 5000, &errorcode, SUBPROCESS_STDIN_DEVNULL)) {
            String_clear(&outbuf);
        }
        if (outbuf.length > 0 &&
            !WString_from_utf8_bytes(&wbuf, outbuf.buffer, outbuf.length)) {
            _wprintf(L"Failed decoding to utf-16\n");
            WString_clear(&wbuf);
        }
        if ((*len + wbuf.length - cmd_len < capacity)) {
            WString_replaceall(&wbuf, L'\n', ' ');
            WString_replaceall(&wbuf, L'\r', ' ');
            WString_replaceall(&wbuf, L'\t', ' ');
            DWORD start = (pos - 1) - str;
            DWORD old_end = 1 + close - str;
            DWORD rem = *len - old_end;
            DWORD new_end = start + wbuf.length;
            memmove(str + new_end, str + old_end, rem * sizeof(wchar_t));
            memcpy(str + start, wbuf.buffer, wbuf.length * sizeof(wchar_t));
            *len = *len - cmd_len + wbuf.length;
            if (at_end) {
                last = str[*len - 1];
                break;
            }
            close = str + new_end - 1;
        } else {
            if (at_end) {
                break;
            }
            *close = par;
        }
        pos = close + 1;
    }
    str[*len - 1] = last;
    String_free(&outbuf);
    WString_free(&wbuf);
}


WString pathext, pathbuf, progbuf, workdir;

const wchar_t* find_program(const wchar_t* cmd, WString* dest) {
    unsigned ix = 0;
    bool in_quote = false;
    WString prog;
    WString_clear(dest);
    WString_clear(&progbuf);
    while (1) {
        if (cmd[ix] == L'\0' || (cmd[ix] == L' ' && !in_quote)) {
            break;
        }
        if (cmd[ix] == L'"') {
            in_quote = !in_quote;
        } else {
            WString_append(&progbuf, cmd[ix]);
        }
        ++ix;
    }

    if (!get_envvar(L"PATHEXT", 0, &pathext) || pathext.length == 0) {
        WString_clear(&pathext);
        WString_extend(&pathext, L".EXE;.BAT");
    }
    wchar_t *ext = pathext.buffer;
    DWORD count = 0;
    do {
        wchar_t* sep = wcschr(ext, L';');
        if (sep == NULL) {
            sep = pathext.buffer + pathext.length;
        }
        *sep = L'\0';
        do {
            if (!WString_reserve(dest, count)) {
                return NULL;
            }
            count = SearchPathW(workdir.buffer,
                                progbuf.buffer, ext, dest->capacity,
                                dest->buffer, NULL);
        } while (count > dest->capacity);
        dest->length = count;
        dest->buffer[dest->length] = L'\0';
        if (count > 0) {
            return dest->buffer;
        }
        *sep = L';';
        ext = sep + 1;
    } while (ext < pathext.buffer + pathext.length);

    if (!get_envvar(L"PATH", 0, &pathbuf)) {
        return NULL;
    }
    ext = pathext.buffer;
    count = 0;
    do {
        wchar_t* sep = wcschr(ext, L';');
        if (sep == NULL) {
            sep = pathext.buffer + pathext.length;
        }
        *sep = L'\0';
        do {
            if (!WString_reserve(dest, count)) {
                return NULL;
            }
            count = SearchPathW(pathbuf.buffer,
                                progbuf.buffer, ext, dest->capacity,
                                dest->buffer, NULL);
        } while (count > dest->capacity);
        dest->length = count;
        dest->buffer[dest->length] = L'\0';
        if (count > 0) {
            return dest->buffer;
        }
        *sep = L';';
        ext = sep + 1;
    } while (ext < pathext.buffer + pathext.length);
    return NULL;
}

wchar_t* entries[] = {
    L"This", L"Is", L"A", L"long test"
};

int main() {
    wchar_t buf[1025];
    memcpy(buf, L"Hello world", 11 * sizeof(wchar_t));
    DWORD len = 11;

    HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    _wprintf(L"This custom prompt >");
    CONSOLE_SCREEN_BUFFER_INFO cinfo;
    GetConsoleScreenBufferInfo(out, &cinfo);
    CHAR_INFO charBuf[256];
    COORD bufSize;
    bufSize.X = 256;
    bufSize.Y = 1;
    COORD bufCorner;
    bufCorner.X = 0;
    bufCorner.Y = 0;
    SMALL_RECT area;
    area.Top = area.Bottom = cinfo.dwCursorPosition.Y;
    area.Left = 0;
    area.Right = cinfo.dwCursorPosition.X;

    ReadConsoleOutputW(out, charBuf, bufSize, bufCorner, &area);
    Cli_Search(buf, &len, 1024, entries, 4);
    buf[len] = L'\0';
    bufSize.X = cinfo.dwCursorPosition.X;
    WriteConsoleOutputW(out, charBuf, bufSize, bufCorner, &area);
    SetConsoleCursorPosition(out, cinfo.dwCursorPosition);
    _wprintf(L"Res: %s\n", buf);

    return 0;
}
