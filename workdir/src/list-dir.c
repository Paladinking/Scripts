#include "args.h"
#include "glob.h"
#include "printf.h"
#include "perm.h"
#include "unicode_width.h"
#include <stdbool.h>
#include <stdint.h>


#define OPTION_INVALID 0
#define OPTION_VALID 1
#define OPTION_SHOW_CURDIR 2
#define OPTION_SHOW_HIDDEN 4
#define OPTION_USE_COLOR 8
#define OPTION_DIR_AS_FILE 16
#define OPTION_ESCAPE_BYTES 32 // TODO
#define OPTION_FAKE_PERMS 64

#define OPTION_LIST_COLS 128
#define OPTION_LIST_ROWS 256 // TODO
#define OPTION_LIST_DETAILS 512 // TODO
#define OPTION_LIST_COMMA 1024

#define OPTION_HUMAN_SIZES 2048

#define OPTION_IF(opt, cond, flag) if (cond) { opt |= flag; _wprintf(L## #flag L"\n"); }


#define COLOR_FMT(s, m, e, col, link, dir, exe) ((!col) ? (s m e) : ((link) ? (s L"\x1b[1;36m" m L"\x1b[0m" e) : ((dir) ? (s L"\x1b[1;34m" m L"\x1b[0m" e) : ((exe) ? (s L"\x1b[1;32m" m L"\x1b[0m" e) : (s m e)))))

typedef struct FileObj {
    WString path;
    WString owner;
    WString group;
    uint64_t size;
    FILETIME stamp;
    uint32_t access;

    bool is_dir;
    bool is_link;
    bool is_exe;

} FileObj;

unsigned str_width(WString *str) {
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

wchar_t** format_file_times(WString* buffer, FileObj* obj, uint32_t count, uint32_t opt, uint32_t* width) {
    wchar_t** buf = Mem_alloc(count * sizeof(wchar_t*));
    SYSTEMTIME st, now_st;
    GetSystemTime(&st);
    FILETIME now_ft;
    int64_t now = 0;
    if (SystemTimeToFileTime(&st, &now_ft)) {
        now = now_ft.dwHighDateTime;
        now = now_ft.dwLowDateTime | (now << 32);
    }

    uint64_t half_year = (uint64_t)10 * 1000 * 1000 * 60 * 60 * 24 * 182;
    const wchar_t* months[] = {L"", L"Jan", L"Feb", L"Mar", L"Apr", L"May",
            L"Jun", L"Jul", L"Aug", L"Sep", L"Oct", L"Mov", L"Dec"};

    for (uint32_t ix = 0; ix < count; ++ix) {
        buf[ix] = (void*)(uintptr_t)buffer->length;
        int64_t stamp = obj[ix].stamp.dwHighDateTime;
        stamp = (obj[ix].stamp.dwLowDateTime) | (stamp << 32);
        if (!FileTimeToSystemTime(&obj[ix].stamp, &st)) {
            memset(&st, 0, sizeof(st));
            st.wMonth = 1;
            st.wDay = 1;
        }
        if (now - stamp > half_year) {
            WString_format_append(buffer, L"%s %2u  %4u", months[st.wMonth], st.wDay, st.wYear);
        } else {
            WString_format_append(buffer, L"%s %2u %2u:%2u", months[st.wMonth], st.wDay, st.wHour, st.wMinute);
        }
        WString_append(buffer, L'\0');
    }
    for (uint32_t ix = 0; ix < count; ++ix) {
        buf[ix] = (uintptr_t)(buf[ix]) + buffer->buffer;
    }
    *width = 12;
    return buf;
}

void format_file_size(WString* buffer, uint64_t size, uint32_t opt) {
    if (opt & OPTION_HUMAN_SIZES && size >= 1024) {
        uint64_t rem = size;
        uint32_t teir = 0;
        while (size > 1024 && teir < 4) {
            rem = size % 1024;
            size = size / 1024;
            ++teir;
        }
        const wchar_t teirs[] = {L'\0', L'K', L'M', L'G', L'T', L'P'};
        if (size < 10) {
            WString_format_append(buffer, L"%llu.%llu%c", size, rem * 10 / 1024, teirs[teir]);
        } else {
            if (rem > 0) {
                ++size;
            }
            WString_format_append(buffer, L"%llu%c", size, teirs[teir]);
        }
    } else {
        WString_format_append(buffer, L"%llu", size);
    }
} 

wchar_t** format_file_sizes(WString* buffer, FileObj* obj, uint32_t count, uint32_t opt, uint32_t* width) {
    wchar_t** buf = Mem_alloc(count * sizeof(wchar_t*));
    uint32_t last_len = buffer->length;
    *width = 0;
    for (uint32_t ix = 0; ix < count; ++ix) {
        buf[ix] = (void*)(uintptr_t)buffer->length;
        uint64_t size = obj[ix].is_dir ? 0 : obj[ix].size;
        format_file_size(buffer, size, opt);
        uint32_t w = buffer->length - last_len;
        if (w > *width) {
            *width = w;
        }
        WString_append(buffer, L'\0');
        last_len = buffer->length;
    }
    for (uint32_t ix = 0; ix < count; ++ix) {
        buf[ix] = (uintptr_t)(buf[ix]) + buffer->buffer;
    }

    return buf;
}

unsigned find_widths(unsigned cols, unsigned *path_widths, unsigned count,
                     unsigned *widths) {
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
    widths[cols - 1] -= 1;
    return w - 1;
}

unsigned *find_dims(unsigned max_w, FileObj *paths, unsigned count, unsigned *cols,
                    unsigned *rows) {
    *cols = 1;
    unsigned *str_widths = Mem_alloc(count * sizeof(unsigned));
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
    unsigned *res = Mem_alloc(*cols * sizeof(unsigned));
    memcpy(res, buf, *cols * sizeof(unsigned));

    Mem_free(str_widths);
    *rows = count / *cols + (count % *cols != 0);
    return res;
}

bool is_exe(WString *s) {
    if (s->length < 5) {
        return false;
    }
    return _wcsicmp(s->buffer + s->length - 4, L".exe") == 0 ||
           _wcsicmp(s->buffer + s->length - 4, L".dll") == 0;
}

wchar_t *default_target = L".";

void get_any_perms(const wchar_t* path, bool is_dir, WString_noinit* owner, WString_noinit* group, uint32_t* mask) {
    WString_create(owner);
    WString_create(group);
    if (!get_perms(path, is_dir, mask, owner, group)) {
        *mask = 0;
    }
    if (owner->length == 0) {
        WString_extend(owner, L"unkown");
    }
    if (group->length == 0) {
        WString_extend(group, L"unkown");
    }
}

void get_detailed_info(FileObj* obj, const wchar_t* path, WString* fake_owner, WString* fake_group, uint32_t fake_flags, uint32_t opt) {
    if (opt & OPTION_FAKE_PERMS) {
        WString_copy(&obj->owner, fake_owner);
        WString_copy(&obj->group, fake_group);
        obj->access = fake_flags;
        DWORD flags = FILE_FLAG_OPEN_REPARSE_POINT | (obj->is_dir ? FILE_FLAG_BACKUP_SEMANTICS : 0);
        HANDLE f = CreateFileW(path, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, flags, NULL);
        if (f == INVALID_HANDLE_VALUE) {
            obj->size = 0;
            obj->stamp.dwHighDateTime = 0;
            obj->stamp.dwLowDateTime = 0;
        } else {
            if (!GetFileTime(f, NULL, NULL, &obj->stamp)) {
                obj->stamp.dwHighDateTime = 0;
                obj->stamp.dwLowDateTime = 0;
            }
            LARGE_INTEGER q;
            if (GetFileSizeEx(f, &q)) {
                obj->size = q.QuadPart;
            } else {
                obj->size = 0;
            }
            CloseHandle(f);
        }
    } else {
        WString_create(&obj->owner);
        WString_create(&obj->group);
        get_perms_size_date(path, obj->is_dir, &obj->access, &obj->owner, &obj->group, &obj->size, &obj->stamp);
        if (obj->owner.length == 0) {
            WString_extend(&obj->owner, L"unkown");
        }
        if (obj->group.length == 0) {
            WString_extend(&obj->group, L"unkown");
        }
    }
}


FileObj *collect_dir(const wchar_t *str, unsigned *count, uint32_t opt, WString *err) {
    FileObj *files = Mem_alloc(10 * sizeof(FileObj));
    unsigned file_capacity = 10;
    unsigned file_count = 0;

    WalkCtx ctx;
    if (!WalkDir_begin(&ctx, str, true)) {
        DWORD cause = GetLastError();
        if (cause == ERROR_FILE_NOT_FOUND || cause == ERROR_BAD_ARGUMENTS ||
            cause == ERROR_PATH_NOT_FOUND) {
            WString_format_append(
                err, L"Cannot access '%s': No such file or directory\n", str);
        } else if (cause == ERROR_ACCESS_DENIED) {
            WString_format_append(
                err, L"Cannot open directory '%s': Permission denied\n", str);
        } else {
            wchar_t buf[256];
            FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM |
                               FORMAT_MESSAGE_IGNORE_INSERTS,
                           NULL, cause, 0, buf, 256, NULL);
            WString_format_append(err, L"Cannot open '%s': %s", str, buf);
        }
        Mem_free(files);
        *count = 0;
        return NULL;
    }
    if (opt & OPTION_SHOW_CURDIR) {
        WString_create(&files[0].path);
        WString_append(&files[0].path, L'.');
        WString_create(&files[1].path);
        WString_append_count(&files[1].path, L"..", 2);
        files[0].is_link = false;
        files[0].is_dir = true;
        files[1].is_link = false;
        files[1].is_dir = true;
        file_count = 2;
    }

    Path *path;
    
    WString group, owner;
    uint32_t mask;
    if (opt & OPTION_FAKE_PERMS) {
        get_any_perms(str, true, &owner, &group, &mask);
    }

    while (WalkDir_next(&ctx, &path)) {
        const wchar_t* filename = path->path.buffer + path->path.length - path->name_len;
        if (filename[0] == L'.' && !(opt & OPTION_SHOW_HIDDEN)) {
            continue;
        }
        if (file_count == file_capacity) {
            file_capacity *= 2;
            files = Mem_realloc(files, file_capacity * sizeof(FileObj));
        }
        FileObj* file = files + file_count;
        ++file_count;
        WString_create_capacity(&file->path, path->name_len);
        WString_append_count(&file->path, filename, path->name_len);
        file->is_link = path->is_link;
        file->is_dir = path->is_dir;
        if (opt & OPTION_LIST_DETAILS) {
            get_detailed_info(file, path->path.buffer, &owner, &group, mask, opt);
        }
    }
    if (opt & OPTION_FAKE_PERMS) {
        WString_free(&group);
        WString_free(&owner);
    }

    *count = file_count;
    return files;
}

void print_target_w(FileObj *files, unsigned file_count, unsigned width,
                    bool color) {
    unsigned cols, rows;
    unsigned *widths = find_dims(width, files, file_count, &cols, &rows);
    for (unsigned row = 0; row < rows; ++row) {
        for (unsigned col = 0; col < cols; ++col) {
            unsigned ix = row + col * rows;
            if (ix >= file_count) {
                break;
            }
            FileObj *path = files + ix;
            if (!color) {
                _wprintf(L"%-*s", widths[col], path->path.buffer);
            } else if (path->is_link) {
                _wprintf(L"\x1b[1;36m%-*s\x1b[0m", widths[col],
                         path->path.buffer);
            } else if (path->is_dir) {
                _wprintf(L"\x1b[1;34m%-*s\x1b[0m", widths[col],
                         path->path.buffer);
            } else if (is_exe(&path->path)) {
                _wprintf(L"\x1b[1;32m%-*s\x1b[0m", widths[col],
                         path->path.buffer);
            } else {
                _wprintf(L"%-*s", widths[col], path->path.buffer);
            }
        }
        _wprintf(L"\n");
    }

    Mem_free(widths);
}

void print_target_c(FileObj* files, unsigned filecount, bool color) {
    for (unsigned i = 0; i < filecount - 1; ++i) {
        FileObj *path = files + i;
        if (!color) {
            _wprintf(L"%s", path->path.buffer);
        } else if (path->is_link) {
            _wprintf(L"\x1b[1;36m%s\x1b[0m", path->path.buffer);
        } else if (path->is_dir) {
            _wprintf(L"\x1b[1;34m%s\x1b[0m", path->path.buffer);
        } else if (is_exe(&path->path)) {
            _wprintf(L"\x1b[1;32m%s\x1b[0m", path->path.buffer);
        } else {
            _wprintf(L"%s", path->path.buffer);
        }
        if (i < filecount - 1) {
            _wprintf(L", ");
        }
    }
    _wprintf(L"\n");
}

void print_target_l(FileObj* files, unsigned filecount, uint32_t opt) {
    bool color = opt & OPTION_USE_COLOR;
    wchar_t perms[11];
    perms[10] = L'\0';
    unsigned owner_w = 0;
    unsigned group_w = 0;
    for (unsigned ix = 0; ix < filecount; ++ix) {
        unsigned w = str_width(&files[ix].owner);
        if (w > owner_w) {
            owner_w = w;
        }
        w = str_width(&files[ix].group);
        if (w > group_w) {
            group_w = w;
        }
    }
    WString size_buf, time_buf;
    WString_create(&size_buf);
    WString_create(&time_buf);
    uint32_t size_w = 0, time_w = 0;
    wchar_t** sizes = format_file_sizes(&size_buf, files, filecount, opt, &size_w);
    wchar_t** times = format_file_times(&time_buf, files, filecount, opt, &time_w);

    for (unsigned ix = 0; ix < filecount; ++ix) {
        if (files[ix].is_dir) {
            perms[0] = L'd';
        } else if (files[ix].is_link) {
            perms[0] = L'l';
        } else {
            perms[0] = L'-';
        }
        perms[1] = (files[ix].access & OWNER_READ) ? L'r' : L'-';
        perms[2] = (files[ix].access & OWNER_WRITE) ? L'w' : L'-';
        perms[3] = (files[ix].access & OWNER_EXECUTE) ? L'x' : L'-';
        perms[4] = (files[ix].access & GROUP_READ) ? L'r' : L'-';
        perms[5] = (files[ix].access & OWNER_WRITE) ? L'w' : L'-';
        perms[6] = (files[ix].access & OWNER_EXECUTE) ? L'x' : L'-';
        perms[7] = (files[ix].access & USER_READ) ? L'r' : L'-';
        perms[8] = (files[ix].access & USER_WRITE) ? L'w' : L'-';
        perms[9] = (files[ix].access & USER_EXECUTE) ? L'x' : L'-';

        const wchar_t* fmt = COLOR_FMT(L"%s 1 %-*s %-*s %*s %*s ", L"%s", L"\n", color, files[ix].is_link, files[ix].is_dir, is_exe(&files[ix].path));
        _wprintf(fmt, perms, owner_w, files[ix].owner.buffer, group_w, files[ix].group.buffer, size_w, sizes[ix], time_w, times[ix], files[ix].path.buffer);
    }

    Mem_free(sizes);
    Mem_free(times);
    WString_free(&size_buf);
    WString_free(&time_buf);
}

void print_target(FileObj* files, unsigned file_count, unsigned w, uint32_t opt) {


    if (opt & OPTION_LIST_COMMA) {
        print_target_c(files, file_count, opt & OPTION_USE_COLOR);
    } else if (opt & OPTION_LIST_DETAILS) {
        print_target_l(files, file_count, opt);
    } else {
        print_target_w(files, file_count, w, opt & OPTION_USE_COLOR);
    }
}

bool list_target(const wchar_t *str, unsigned width, uint32_t opt,
                 bool print_name, WString *err) {

    unsigned file_count = 0;
    FileObj *files = collect_dir(str, &file_count, opt, err);
    if (!files) {
        return false;
    }

    if (print_name) {
        _wprintf(L"%s:\n", str);
    }

    print_target(files, file_count, width, opt);

    for (unsigned i = 0; i < file_count; ++i) {
        WString_free(&files[i].path);
        if (opt & OPTION_LIST_DETAILS) {
            WString_free(&files[i].owner);
            WString_free(&files[i].group);
        }
    }
    Mem_free(files);
    return true;
}

uint32_t parse_options(wchar_t** argv, int* argc, unsigned* width, 
                       bool has_console) {
    HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    HANDLE err = GetStdHandle(STD_OUTPUT_HANDLE);
    *width = 80;
    CONSOLE_SCREEN_BUFFER_INFO cinfo;
    if (has_console && GetConsoleScreenBufferInfo(out, &cinfo)) {
        *width = cinfo.dwSize.X;
    } else if (!has_console) {
        *width = 1;
    }

    const wchar_t* yes_options[] = {L"always", L"yes", L"force"};
    const wchar_t* no_options[] = {L"never", L"no", L"none"};
    const wchar_t* auto_options[] = {L"auto", L"tty", L"if-tty"}; 
    EnumValue color_options[] = {{ yes_options, 3 }, { no_options, 3}, 
                                 { auto_options, 3}};
    FlagValue color = {FLAG_OPTONAL_VALUE | FLAG_ENUM, color_options, 3};
    FlagValue width_arg = {FLAG_REQUIRED_VALUE | FLAG_UINT};
    const wchar_t *across[] = {L"across", L"horizontal"},
                  *commas[] = {L"commas"}, *list[] = {L"long", L"verbose"},
                  *single[] = {L"single-column"}, *vertical[] = {L"vertical"};
    EnumValue foramt_options[] = {{across, 2}, {commas, 1}, {list, 2}, 
                                  {single, 1}, {vertical, 1}};
    FlagValue format_arg = {FLAG_REQUIRED_VALUE | FLAG_ENUM, foramt_options, 5};

    FlagInfo flags[] = {{L'a', L"all", NULL},               // 0
                        {L'A', L"almost-all", NULL},        // 1
                        {L'b', L"escape", NULL},            // 2
                        {L'd', L"directory", NULL},         // 3
                        {L'\0', L"color", &color},          // 4
                        {L'l', NULL, NULL},                 // 5
                        {L'x', NULL, NULL},                 // 6
                        {L'm', NULL, NULL},                 // 7
                        {L'C', NULL, NULL},                 // 8
                        {L'1', NULL, NULL},                 // 9
                        {L'\0', L"format", &format_arg},    // 10
                        {L'w', L"width", &width_arg},       // 11
                        {L'\0', L"fake-perms"},             // 12
                        {L'h', L"human-readable"}           // 13
                        };
    ErrorInfo errors;
    if (!find_flags(argv, argc, flags, 14, &errors)) {
        _wprintf_h(err, L"%u -- %s\n", errors.type, errors.value);
        return OPTION_INVALID;
    }
    uint32_t opt = OPTION_VALID;
    bool show_hidden = flags[0].count > 0 || flags[1].count > 0;
    bool show_curdir = flags[0].count > 0 && flags[0].ord > flags[1].ord;
    bool dir_as_file = flags[3].count > 0;
    bool escape_bytes = flags[2].count > 0;
    bool has_color = has_console;
    bool fake_perms = flags[12].count > 0;
    bool human_readable = flags[13].count > 0;
    unsigned format = 0;
    unsigned m_ord = 0;
    for (unsigned ix = 5; ix < 11; ++ix) {
        if (flags[ix].ord > m_ord) {
            m_ord = flags[ix].ord;
            format = ix;
        }
    }
    if (flags[11].count > 0) {
        *width = width_arg.uint;
    }
    if (m_ord > 0) {
        if (format == 10) {
            if (format_arg.uint == 0) {
                format = 6;
            } else if (format_arg.uint == 1) {
                format = 7;
            } else if (format_arg.uint == 2) {
                format = 5;
            } else if (format_arg.uint == 3) {
                format = 9;
            } else {
                format = 8;
            }
        }
        if (format == 9) {
            format = 8;
            *width = 1;
        }
    }
    OPTION_IF(opt, format == 5, OPTION_LIST_DETAILS);
    OPTION_IF(opt, format == 6, OPTION_LIST_ROWS);
    OPTION_IF(opt, format == 7, OPTION_LIST_COMMA);
    OPTION_IF(opt, format == 8 || format == 0, OPTION_LIST_COLS);

    if (flags[4].count > 0) {
        if (!color.has_value || color.uint == 0) {
            has_color = true;
        } else if (color.uint == 1) {
            has_color = false;
        }
    }
    OPTION_IF(opt, has_color, OPTION_USE_COLOR);
    OPTION_IF(opt, show_hidden, OPTION_SHOW_HIDDEN);
    OPTION_IF(opt, show_curdir, OPTION_SHOW_CURDIR);
    OPTION_IF(opt, dir_as_file, OPTION_DIR_AS_FILE);
    OPTION_IF(opt, escape_bytes, OPTION_ESCAPE_BYTES);
    OPTION_IF(opt, fake_perms, OPTION_FAKE_PERMS);
    OPTION_IF(opt, human_readable, OPTION_HUMAN_SIZES);

    return opt;
}

int main() {
    HANDLE err = GetStdHandle(STD_ERROR_HANDLE);
    HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    bool has_console = false;
    DWORD old_flags;

    if (GetFileType(out) == FILE_TYPE_CHAR) {
        if (GetConsoleMode(out, &old_flags)) {
            if (SetConsoleMode(out, old_flags |
                                        ENABLE_VIRTUAL_TERMINAL_PROCESSING)) {
                has_console = true;
            }
        }
    }

    wchar_t *args = GetCommandLineW();
    int argc;
    int status = 1;
    wchar_t **argv = parse_command_line(args, &argc);

    unsigned w;
    uint32_t opt = parse_options(argv, &argc, &w, has_console);
    if (opt == OPTION_INVALID) {
        status = 1;
        goto end;
    }


    wchar_t *p = default_target;
    wchar_t **targets = argv + 1;
    unsigned target_count = argc - 1;
    if (argc <= 1) {
        target_count = 1;
        targets = &p;
    }
    WString errors;
    WString_create(&errors);

    FileObj *file_targets = Mem_alloc(1 * sizeof(FileObj));
    unsigned file_capacity = 1;
    unsigned file_count = 0;
    for (unsigned ix = 0; ix < target_count;) {
        DWORD attrs = get_file_attrs(targets[ix]);
        if (attrs == INVALID_FILE_ATTRIBUTES) {
            _wprintf_h(err, L"Cannot access '%s': No such file or directory\n",
                       targets[ix]);
        } else if (attrs & FILE_ATTRIBUTE_DIRECTORY && 
                   !(opt & OPTION_DIR_AS_FILE)) {
            ++ix;
            continue;
        } else {
            if (file_count == file_capacity) {
                file_capacity *= 2;
                file_targets = Mem_realloc(file_targets, file_capacity * sizeof(FileObj));
            }
            FileObj* file = file_targets + file_count;

            file->is_dir = attrs & FILE_ATTRIBUTE_DIRECTORY;
            file->is_link = attrs & FILE_ATTRIBUTE_REPARSE_POINT;
            WString_create(&file->path);
            WString_extend(&file->path, targets[ix]);
            if (opt & OPTION_LIST_DETAILS) {
                get_detailed_info(file, file->path.buffer, NULL, NULL, 0, opt & ~(OPTION_FAKE_PERMS));
            }
            ++file_count;
        }
        --target_count;
        for (unsigned j = ix; j < target_count; ++j) {
            targets[j] = targets[j + 1];
        }
    }


    if (target_count == 1 && file_count == 0) {
        list_target(targets[0], w, opt, false, &errors);
    } else {
        print_target(file_targets, file_count, w, opt);
        if (target_count > 0) {
            _wprintf(L"\n");
        }

        for (unsigned ix = 0; ix < target_count; ++ix) {
            if (!list_target(targets[ix], w, opt, true, &errors)) {
                continue;
            }
            if (ix < target_count - 1) {
                _wprintf(L"\n");
            }
        }
    }
    _wprintf_h(err, L"%s", errors.buffer);
    WString_free(&errors);

    for (unsigned ix = 0; ix < file_count; ++ix) {
        WString_free(&file_targets[ix].path);
        if (opt & OPTION_LIST_DETAILS) {
            WString_free(&file_targets[ix].owner);
            WString_free(&file_targets[ix].group);
        }
    }
    Mem_free(file_targets);

    status = 0;
end:
    if (has_console) {
        SetConsoleMode(out, old_flags);
    }
    Mem_free(argv);

    return status;
}
