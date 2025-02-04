#include "args.h"
#include "glob.h"
#include "printf.h"
#include "unicode_width.h"
#include <stdbool.h>

unsigned str_width(WString* str) {
    unsigned w = 0;
    for (unsigned ix = 0; ix < str->length; ++ix) {
        wchar_t c1 = str->buffer[ix];
        if (IS_HIGH_SURROGATE(c1)) {
            ++ix;
            if (ix < str->length && IS_LOW_SURROGATE(str->buffer[ix])) {
                w += UNICODE_PAIR_WIDTH(c1, str->buffer[ix]);
            } else {
                // Invalid unicode...
                continue;
            }
        } else {
            w += UNICODE_WIDTH(c1);
        }
    }
    return w;
}


unsigned find_widths(unsigned cols, unsigned* path_widths, unsigned count, unsigned* widths) {
    for (unsigned ix = 0; ix < cols; ++ix) {
        widths[ix] = 0;
    }
    if (count == 0) {
        return 0;
    }

    unsigned rows = count / cols + (count % cols != 0);
    unsigned w = 0;
    for (unsigned i = 0; i < cols; ++i) {
        unsigned max_w = 0;
        unsigned max_i = 0;
        for (unsigned j = i * rows; j < ((i + 1) * rows) && j < count; ++j) {
            unsigned rw = path_widths[j];
            if (rw > max_w) {
                max_w = rw;
                max_i = j;
            }
        }
        widths[i] = max_w + 2;
        w += max_w + 2;
    }
    widths[count - 1] -= 1;
    return w - 1;
}

unsigned* find_dims(unsigned max_w, Path* paths, unsigned count, unsigned* cols, unsigned* rows) {
    *cols = 1;
    unsigned* str_widths = Mem_alloc(count * sizeof(unsigned));
    for (unsigned i = 0; i < count; ++i) {
        str_widths[i] = str_width(&paths[i].path);
    }
    unsigned w1[512];
    unsigned w2[512];
    memset(w1, 0, sizeof(w1));
    unsigned *buf = w1;
    while (1) {
        *cols += 1;
        if (*cols == 512) {
            break;
        }
        buf = buf == w1 ? w2 : w1;
        unsigned w = find_widths(*cols, str_widths, count, buf);
        if (w > max_w) {
            *cols -= 1;
            buf = buf == w1 ? w2 : w1;
            break;
        }
    }
    unsigned* res = Mem_alloc(*cols * sizeof(unsigned));
    memcpy(res, buf, *cols * sizeof(unsigned));

    Mem_free(str_widths);
    *rows = count / *cols + (count % *cols != 0);
    return res;
}

bool is_exe(WString* s) {
    if (s->length < 5) {
        return false;
    }
    return _wcsicmp(s->buffer + s->length - 4, L".exe") == 0 ||
           _wcsicmp(s->buffer + s->length - 4, L".dll") == 0;
}


int main() {
    HANDLE err = GetStdHandle(STD_ERROR_HANDLE);
    HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    bool has_ansi = false;
    DWORD old_flags;

    if (GetFileType(out) == FILE_TYPE_CHAR) {
        if (GetConsoleMode(out, &old_flags)) {
            if (SetConsoleMode(out, old_flags | ENABLE_VIRTUAL_TERMINAL_PROCESSING)) {
                has_ansi = true;
            }
        }
    }


    wchar_t* args = GetCommandLineW();
    int argc;
    int status = 1;
    wchar_t** argv = parse_command_line(args, &argc);

    FlagInfo flags[] = {
        {L'a', L"all", 0},
        {L'A', L"almost-all", 0},
        {L'b', L"escape", 0},
        {L'd', L"directory", 0},
        {L'\0', NULL, 0}
    };
    Path* files = NULL;
    unsigned file_count = 0;

    if (find_flags(argv, &argc, flags, 3) > 0) {
        _wprintf_h(err, L"Unkown option '%s'\n", flags[3].long_name);
        goto end;
    }

    wchar_t* target_dir;
    if (argc <= 1) {
        target_dir = L".";
    } else {
        target_dir = argv[1];
    }

    files = Mem_alloc(10 * sizeof(Path));
    unsigned file_capacity = 10;

    WalkCtx ctx;
    WalkDir_begin(&ctx, target_dir);
    Path* path;
    while (WalkDir_next(&ctx, &path)) {
        if (file_count == file_capacity) {
            file_capacity *= 2;
            files = Mem_realloc(files, file_capacity * sizeof(Path));
        }
        WString_copy(&files[file_count].path, &path->path);
        files[file_count].is_link = path->is_link;
        files[file_count].is_dir = path->is_dir;
        ++file_count;
    }

    unsigned w = 80;
    CONSOLE_SCREEN_BUFFER_INFO cinfo;
    if (has_ansi && GetConsoleScreenBufferInfo(out, &cinfo)) {
        w = cinfo.dwSize.X;
    } else if (!has_ansi) {
        w = 1;
    }

    unsigned cols, rows;
    unsigned* widths = find_dims(w, files, file_count, &cols, &rows);

    for (unsigned row = 0; row < rows; ++row) {
            for (unsigned col = 0; col < cols; ++col) {
            unsigned ix = row + col * rows;
            if (ix >= file_count) {
                break;
            }
            Path* path = files + ix;
            if (!has_ansi) {
                _wprintf(L"%-*s", widths[col], path->path.buffer);
            } else if (path->is_link) {
                _wprintf(L"\x1b[1;36m%-*s\x1b[0m", widths[col], path->path.buffer);
            } else if (path->is_dir) {
                _wprintf(L"\x1b[1;34m%-*s\x1b[0m", widths[col], path->path.buffer);
            } else if (is_exe(&path->path)) {
                _wprintf(L"\x1b[1;32m%-*s\x1b[0m", widths[col], path->path.buffer);
            } else {
                _wprintf(L"%-*s", widths[col], path->path.buffer);
            }
        }
        _wprintf(L"\n");
    }

    Mem_free(widths);
end:
    if (has_ansi) {
        SetConsoleMode(out, old_flags);
    }
    if (files != NULL) {
        for (unsigned i = 0; i < file_count; ++i) {
            WString_free(&files[i].path);
        }
        Mem_free(files);
    }
    Mem_free(argv);

    return status;
}
