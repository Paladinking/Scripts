#include "args.h"
#include "glob.h"
#include "perm.h"
#include "printf.h"
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
#define OPTION_LIST_DETAILS 512
#define OPTION_LIST_COMMA 1024

#define OPTION_HUMAN_SIZES 2048
#define OPTION_ALLOCATION_SIZE 4096

#define OPTION_HELP 8192

#define OPTION_IF(opt, cond, flag)                                             \
    if (cond) {                                                                \
        opt |= flag;                                                           \
    }

#define COLOR_FMT(s, m, e, col, link, dir, exe)                                \
    ((!col) ? (s m e)                                                          \
            : ((link) ? (s L"\x1b[1;36m" m L"\x1b[0m" e)                       \
                      : ((dir) ? (s L"\x1b[1;34m" m L"\x1b[0m" e)              \
                               : ((exe) ? (s L"\x1b[1;32m" m L"\x1b[0m" e)     \
                                        : (s m e)))))

typedef struct FileObj {
    WString path;
    WString owner;
    WString group;
    uint64_t size;
    uint64_t disk_size;
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

wchar_t **format_file_times(WString *buffer, FileObj *obj, uint32_t count,
                            uint32_t opt, uint32_t *width) {
    wchar_t **buf = Mem_alloc(count * sizeof(wchar_t *));
    SYSTEMTIME st, now_st;
    GetSystemTime(&st);
    FILETIME now_ft;
    int64_t now = 0;
    if (SystemTimeToFileTime(&st, &now_ft)) {
        now = now_ft.dwHighDateTime;
        now = now_ft.dwLowDateTime | (now << 32);
    }

    uint64_t half_year = (uint64_t)10 * 1000 * 1000 * 60 * 60 * 24 * 182;
    const wchar_t *months[] = {L"",    L"Jan", L"Feb", L"Mar", L"Apr",
                               L"May", L"Jun", L"Jul", L"Aug", L"Sep",
                               L"Oct", L"Mov", L"Dec"};

    for (uint32_t ix = 0; ix < count; ++ix) {
        buf[ix] = (void *)(uintptr_t)buffer->length;
        int64_t stamp = obj[ix].stamp.dwHighDateTime;
        stamp = (obj[ix].stamp.dwLowDateTime) | (stamp << 32);
        if (!FileTimeToSystemTime(&obj[ix].stamp, &st)) {
            memset(&st, 0, sizeof(st));
            st.wMonth = 1;
            st.wDay = 1;
        }
        if (now - stamp > half_year) {
            WString_format_append(buffer, L"%s %2u  %4u", months[st.wMonth],
                                  st.wDay, st.wYear);
        } else {
            WString_format_append(buffer, L"%s %2u %02u:%02u", months[st.wMonth],
                                  st.wDay, st.wHour, st.wMinute);
        }
        WString_append(buffer, L'\0');
    }
    for (uint32_t ix = 0; ix < count; ++ix) {
        buf[ix] = (uintptr_t)(buf[ix]) + buffer->buffer;
    }
    *width = 12;
    return buf;
}

void format_file_size(WString *buffer, uint64_t size, uint32_t opt) {
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
            WString_format_append(buffer, L"%llu.%llu%c", size, rem * 10 / 1024,
                                  teirs[teir]);
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

wchar_t **format_disk_sizes(WString *buffer, FileObj *obj, uint32_t count,
                            uint32_t opt, uint32_t *width) {
    wchar_t **buf = Mem_alloc(count * sizeof(wchar_t *));
    uint32_t last_len = buffer->length;
    *width = 0;
    for (uint32_t ix = 0; ix < count; ++ix) {
        buf[ix] = (void *)(uintptr_t)buffer->length;
        uint64_t size = obj[ix].disk_size;
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

wchar_t **format_file_sizes(WString *buffer, FileObj *obj, uint32_t count,
                            uint32_t opt, uint32_t *width) {
    wchar_t **buf = Mem_alloc(count * sizeof(wchar_t *));
    uint32_t last_len = buffer->length;
    *width = 0;
    for (uint32_t ix = 0; ix < count; ++ix) {
        buf[ix] = (void *)(uintptr_t)buffer->length;
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

uint32_t find_widths(uint32_t cols, uint32_t *path_widths, uint32_t count,
                     uint32_t *widths) {
    for (uint32_t ix = 0; ix < cols; ++ix) {
        widths[ix] = 0;
    }
    if (count == 0) {
        return 0;
    }

    uint32_t rows = count / cols + (count % cols != 0);
    uint32_t w = 0;
    for (uint32_t i = 0; i < cols; ++i) {
        uint32_t max_w = 0;
        uint32_t max_i = 0;
        for (uint32_t j = i * rows; j < ((i + 1) * rows) && j < count; ++j) {
            uint32_t rw = path_widths[j];
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

uint32_t *find_dims(uint32_t max_w, uint32_t *str_widths, uint32_t count,
                    uint32_t *cols, uint32_t *rows, bool sizes) {
    *cols = 1;
    uint32_t w1[512];
    uint32_t w2[512];
    memset(w1, 0, sizeof(w1));
    uint32_t *buf = w1;
    while (1) {
        *cols += 1;
        if (*cols == 512) {
            break;
        }
        buf = buf == w1 ? w2 : w1;
        uint32_t w = find_widths(*cols, str_widths, count, buf);
        if (w > max_w) {
            *cols -= 1;
            buf = buf == w1 ? w2 : w1;
            break;
        }
    }
    *rows = count / *cols + (count % *cols != 0);
    uint32_t *res;
    if (sizes) {
        res = Mem_alloc(2 * (*cols) * sizeof(uint32_t));
        for (uint32_t col = 0; col < (*cols); ++col) {
            uint32_t w = 0;
            for (uint32_t row = 0; row < (*rows); ++row) {
                uint32_t ix = row + col * (*rows);
                if (ix >= count) {
                    break;
                }
                if (str_widths[ix + count] > w) {
                    w = str_widths[ix + count];
                }
            }
            res[col + (*cols)] = w;
        }
    } else {
        res = Mem_alloc(*cols * sizeof(uint32_t));
    }
    memcpy(res, buf, *cols * sizeof(uint32_t));
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

void get_any_perms(const wchar_t *path, bool is_dir, WString_noinit *owner,
                   WString_noinit *group, uint32_t *mask) {
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

void get_detailed_info(FileObj *obj, const wchar_t *path, WString *fake_owner,
                       WString *fake_group, uint32_t fake_flags, uint32_t opt) {

    if (opt & OPTION_LIST_DETAILS && opt & OPTION_FAKE_PERMS) {
        WString_copy(&obj->owner, fake_owner);
        WString_copy(&obj->group, fake_group);
        obj->access = fake_flags;
    } else if (opt & OPTION_LIST_DETAILS) {
        WString_create(&obj->owner);
        WString_create(&obj->group);
    } else if (!(opt & OPTION_ALLOCATION_SIZE)) {
        return;
    }

    DWORD flags = FILE_FLAG_OPEN_REPARSE_POINT |
                  (obj->is_dir ? FILE_FLAG_BACKUP_SEMANTICS : 0);
    HANDLE f =
        CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                    NULL, OPEN_EXISTING, flags, NULL);

    if (f == INVALID_HANDLE_VALUE) {
        if (opt & OPTION_LIST_DETAILS) {
            obj->size = 0;
            obj->stamp.dwHighDateTime = 0;
            obj->stamp.dwLowDateTime = 0;
            if (!(opt & OPTION_FAKE_PERMS)) {
                obj->access = 0;
                WString_extend(&obj->owner, L"unkown");
                WString_extend(&obj->group, L"unkown");
            }
        }
        if (opt & OPTION_ALLOCATION_SIZE) {
            obj->disk_size = 0;
        }
        return;
    }
    if (opt & OPTION_LIST_DETAILS) {
        if (!(opt & OPTION_FAKE_PERMS)) {
            if (!get_perms_h(f, &obj->access, &obj->owner, &obj->group)) {
                obj->access = 0;
            }
            if (obj->owner.length == 0) {
                WString_extend(&obj->owner, L"unkown");
            }
            if (obj->group.length == 0) {
                WString_extend(&obj->group, L"unkown");
            }
        }
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
    }
    if (opt & OPTION_ALLOCATION_SIZE) {
        FILE_STANDARD_INFO i;
        if (GetFileInformationByHandleEx(f, FileStandardInfo, &i, sizeof(i))) {
            obj->disk_size = i.AllocationSize.QuadPart;
            if (!(opt & OPTION_HUMAN_SIZES)) {
                obj->disk_size /= 1024;
            }
        } else {
            obj->disk_size = 0;
        }
    }
    CloseHandle(f);
    return;
}

#define MAX_NO_THREADS 16

typedef struct ThreadParams {
    FileObj *files;
    uint32_t file_count;
    uint32_t opt;
    WString *fake_owner;
    WString *fake_group;
    uint32_t fake_flags;
    uint32_t no_threads;
} ThreadParams;

typedef struct ThreadInfo {
    ThreadParams *params;
    uint32_t id;
    HANDLE thread_handle;
} ThreadInfo;

DWORD get_detailed_info_entry(void *param) {
    ThreadInfo *info = param;
    ThreadParams *p = info->params;

    uint32_t per_thread = p->file_count / p->no_threads;
    uint32_t start = per_thread * info->id;
    uint32_t end = per_thread * (info->id + 1);
    if (info->id == p->no_threads - 1) {
        end = p->file_count;
    }

    FileObj *files = p->files;

    for (uint32_t ix = start; ix < end; ++ix) {
        uint64_t filename_offset = files[ix].size;
        get_detailed_info(&files[ix], files[ix].path.buffer, p->fake_owner,
                          p->fake_group, p->fake_flags, p->opt);
        WString_remove(&files[ix].path, 0, filename_offset);
    }

    return 0;
}

FileObj *collect_dir(const wchar_t *str, unsigned *count, uint32_t opt,
                     WString *err) {
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
    if (opt & OPTION_FAKE_PERMS && opt & OPTION_LIST_DETAILS) {
        get_any_perms(str, true, &owner, &group, &mask);
    }

    while (WalkDir_next(&ctx, &path)) {
        uint64_t filename_offset = path->path.length - path->name_len;
        if (path->path.buffer[filename_offset] == L'.' &&
            !(opt & OPTION_SHOW_HIDDEN)) {
            continue;
        }
        if (file_count == file_capacity) {
            file_capacity *= 2;
            files = Mem_realloc(files, file_capacity * sizeof(FileObj));
        }
        FileObj *file = files + file_count;
        ++file_count;
        WString_copy(&file->path, &path->path);
        file->size = filename_offset;
        file->is_link = path->is_link;
        file->is_dir = path->is_dir;
    }

    int32_t ix;

    if (opt & (OPTION_LIST_DETAILS | OPTION_ALLOCATION_SIZE)) {
        ThreadParams params;
        params.files = files;
        params.file_count = file_count;
        params.opt = opt;
        params.fake_owner = &owner;
        params.fake_group = &group;
        params.fake_flags = mask;
        params.no_threads = file_count / 4;
        if (params.no_threads > MAX_NO_THREADS) {
            params.no_threads = MAX_NO_THREADS;
        }

        ThreadInfo info[MAX_NO_THREADS];
        info[0].params = &params;
        info[0].id = 0;
        for (uint32_t i = 1; i < params.no_threads; ++i) {
            info[i].params = &params;
            info[i].id = i;
            info[i].thread_handle =
                CreateThread(NULL, 0, get_detailed_info_entry, &info[i], 0, 0);
        }
        get_detailed_info_entry(&info[0]);
        for (uint32_t i = 1; i < params.no_threads; ++i) {
            if (info[i].thread_handle == NULL) {
                get_detailed_info_entry(&info[i]);
            } else {
                WaitForSingleObject(info[i].thread_handle, INFINITE);
            }
        }
    } else {
        for (ix = 0; ix < file_count; ix++) {
            uint64_t filename_offset = files[ix].size;
            WString_remove(&files[ix].path, 0, filename_offset);
        }
    }

    if (opt & OPTION_FAKE_PERMS && opt & OPTION_LIST_DETAILS) {
        WString_free(&group);
        WString_free(&owner);
    }

    *count = file_count;
    return files;
}

void print_target_w(FileObj *files, unsigned file_count, unsigned width,
                    uint32_t opt) {
    bool color = opt & OPTION_USE_COLOR;
    unsigned cols, rows;

    uint32_t count = file_count;

    WString disk_size;
    uint32_t disk_w;
    wchar_t **disk_sizes;
    if (opt & OPTION_ALLOCATION_SIZE) {
        WString_create(&disk_size);
        disk_sizes =
            format_disk_sizes(&disk_size, files, file_count, opt, &disk_w);
        count *= 2;
    }

    uint32_t *str_widths = Mem_alloc(count * sizeof(uint32_t));
    for (uint32_t ix = 0; ix < file_count; ++ix) {
        str_widths[ix] = str_width(&files[ix].path);
        if (opt & OPTION_ALLOCATION_SIZE) {
            str_widths[ix + file_count] = wcslen(disk_sizes[ix]);
            str_widths[ix] += str_widths[ix + file_count] + 1;
        }
    }

    uint32_t *widths = find_dims(width, str_widths, file_count, &cols, &rows,
                                 opt & OPTION_ALLOCATION_SIZE);
    for (unsigned row = 0; row < rows; ++row) {
        for (unsigned col = 0; col < cols; ++col) {
            unsigned ix = row + col * rows;
            if (ix >= file_count) {
                break;
            }
            FileObj *path = files + ix;
            uint32_t w = widths[col];
            if (opt & OPTION_ALLOCATION_SIZE) {
                uint32_t size_w = widths[col + cols];
                w -= size_w + 1;
                _wprintf(L"%*s ", size_w, disk_sizes[ix]);
            }
            const wchar_t *fmt =
                COLOR_FMT(L"", L"%-*s", L"", color, path->is_link, path->is_dir,
                          is_exe(&path->path));
            _wprintf(fmt, w, path->path.buffer);
        }
        _wprintf(L"\n");
    }

    if (opt & OPTION_ALLOCATION_SIZE) {
        Mem_free(disk_sizes);
        WString_free(&disk_size);
    }
    Mem_free(widths);
    Mem_free(str_widths);
}

void print_target_c(FileObj *files, unsigned filecount, uint32_t max_w,
                    uint32_t opt) {
    bool color = opt & OPTION_USE_COLOR;
    WString disk_size;
    uint32_t disk_w;
    wchar_t **disk_sizes;
    if (opt & OPTION_ALLOCATION_SIZE) {
        WString_create(&disk_size);
        disk_sizes =
            format_disk_sizes(&disk_size, files, filecount, opt, &disk_w);
    }

    for (unsigned i = 0; i < filecount - 1; ++i) {
        FileObj *path = files + i;
        if (opt & OPTION_ALLOCATION_SIZE) {
            _wprintf(L"%s ", disk_sizes[i]);
        }
        const wchar_t *fmt = COLOR_FMT(L"", L"%s", L"", color, path->is_link,
                                       path->is_dir, is_exe(&path->path));
        _wprintf(fmt, path->path.buffer);
        if (i < filecount - 1) {
            _wprintf(L", ");
        }
    }
    _wprintf(L"\n");
    if (opt & OPTION_ALLOCATION_SIZE) {
        Mem_free(disk_sizes);
        WString_free(&disk_size);
    }
}

void print_target_l(FileObj *files, unsigned filecount, uint32_t opt) {
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
    WString size_buf, time_buf, disk_buf;
    WString_create(&size_buf);
    WString_create(&time_buf);
    uint32_t size_w = 0, time_w = 0, disk_w = 0;
    wchar_t **disk_sizes;
    if (opt & OPTION_ALLOCATION_SIZE) {
        WString_create(&disk_buf);
        disk_sizes =
            format_disk_sizes(&disk_buf, files, filecount, opt, &disk_w);
    }
    wchar_t **sizes =
        format_file_sizes(&size_buf, files, filecount, opt, &size_w);
    wchar_t **times =
        format_file_times(&time_buf, files, filecount, opt, &time_w);

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

        if (opt & OPTION_ALLOCATION_SIZE) {
            _wprintf(L"%*s ", disk_w, disk_sizes[ix]);
        }
        const wchar_t *fmt = COLOR_FMT(
            L"%s 1 %-*s %-*s %*s %*s ", L"%s", L"\n", color, files[ix].is_link,
            files[ix].is_dir, is_exe(&files[ix].path));
        _wprintf(fmt, perms, owner_w, files[ix].owner.buffer, group_w,
                 files[ix].group.buffer, size_w, sizes[ix], time_w, times[ix],
                 files[ix].path.buffer);
    }

    Mem_free(sizes);
    Mem_free(times);
    WString_free(&size_buf);
    WString_free(&time_buf);
    if (opt & OPTION_ALLOCATION_SIZE) {
        Mem_free(disk_sizes);
        WString_free(&disk_buf);
    }
}

void print_target(FileObj *files, unsigned file_count, unsigned w,
                  uint32_t opt) {

    if (opt & OPTION_LIST_COMMA) {
        print_target_c(files, file_count, w, opt);
    } else if (opt & OPTION_LIST_DETAILS) {
        print_target_l(files, file_count, opt);
    } else {
        print_target_w(files, file_count, w, opt);
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

    if (opt & (OPTION_ALLOCATION_SIZE | OPTION_LIST_DETAILS)) {
        uint64_t total = 0;
        for (uint32_t ix = 0; ix < file_count; ++ix) {
            total += (opt & OPTION_ALLOCATION_SIZE) ? files[ix].disk_size
                                                    : files[ix].size;
        }
        uint32_t old_len = err->length;
        format_file_size(err, total, opt);
        _wprintf(L"total %s\n", err->buffer + old_len);
        WString_pop(err, err->length - old_len);
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

const wchar_t* HELP_MESSAGE = L"Usage: %s [OPTION]... [FILE]...\n"
                              L"List information about the FILEs (the current directory by default)\n\n"
                              L"-a, --all               do not ignore entries starting with .\n"
                              L"-A, --almost-all        same as -a, but do not list . and ..\n"
                              L"-C                      list entires by collums\n"
                              L"    --color[=WHEN]      colorize the output; WHEN can be 'always' (default\n"
                              L"                         if omitted, 'auto' or 'never'\n"
                              L"-d, --directory         treat directories as files, do not list their content\n"
                              L"    --format=WORD       use format commas -m, long -l, single-collum -1,\n"
                              L"                         verbose -l or vertical -C\n"
                              L"-h, --human-readable    format sizes in a human-readable format\n"
                              L"    --help              display this help message and exit\n"
                              L"-l                      use a detaild long listing format\n"
                              L"-m                      list entries comma separated\n"
                              L"    --real-perms        with -l, query each file for permissions, group and\n"
                              L"                         owner, instead of using folder values for all files.\n"
                              L"-s, --size              Also show file allocation sizes\n"
                              L"-w, --width=COLS        specify the maximum width in collums. 0 means no limit\n"
                              L"-1                      list all entries in one collumn\n";

uint32_t parse_options(wchar_t **argv, int *argc, unsigned *width,
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

    const wchar_t *yes_options[] = {L"always", L"yes", L"force"};
    const wchar_t *no_options[] = {L"never", L"no", L"none"};
    const wchar_t *auto_options[] = {L"auto", L"tty", L"if-tty"};
    EnumValue color_options[] = {
        {yes_options, 3}, {no_options, 3}, {auto_options, 3}};
    FlagValue color = {FLAG_OPTONAL_VALUE | FLAG_ENUM, color_options, 3};
    FlagValue width_arg = {FLAG_REQUIRED_VALUE | FLAG_UINT};
    const wchar_t *across[] = {L"across", L"horizontal"},
                  *commas[] = {L"commas"}, *list[] = {L"long", L"verbose"},
                  *single[] = {L"single-column"}, *vertical[] = {L"vertical"};
    EnumValue foramt_options[] = {
        {across, 2}, {commas, 1}, {list, 2}, {single, 1}, {vertical, 1}};
    FlagValue format_arg = {FLAG_REQUIRED_VALUE | FLAG_ENUM, foramt_options, 5};

    FlagInfo flags[] = {
        {L'a', L"all", NULL},            // 0
        {L'A', L"almost-all", NULL},     // 1
        {L'\0', NULL, NULL},             // 2 (escape)
        {L'd', L"directory", NULL},      // 3
        {L'\0', L"color", &color},       // 4
        {L'l', NULL, NULL},              // 5
        {L'x', NULL, NULL},              // 6
        {L'm', NULL, NULL},              // 7
        {L'C', NULL, NULL},              // 8
        {L'1', NULL, NULL},              // 9
        {L'\0', L"format", &format_arg}, // 10
        {L'w', L"width", &width_arg},    // 11
        {L'\0', L"real-perms"},          // 12
        {L'h', L"human-readable"},       // 13
        {L's', L"size"},                 // 14
        {L'\0', L"help"}                 // 15
    };
    ErrorInfo errors;
    if (!find_flags(argv, argc, flags, 16, &errors)) {
        wchar_t* err_msg = format_error(&errors, flags, 16);
        _wprintf_h(err, L"%s\n", err_msg); 
        _wprintf_h(err, L"Run '%s --help' for more information\n", argv[0]);
        return OPTION_INVALID;
    }
    if (flags[15].count > 0) {
        _wprintf(HELP_MESSAGE, argv[0]);
        return OPTION_HELP;
    }
    uint32_t opt = OPTION_VALID;
    bool show_hidden = flags[0].count > 0 || flags[1].count > 0;
    bool show_curdir = flags[0].count > 0 && flags[0].ord > flags[1].ord;
    bool dir_as_file = flags[3].count > 0;
    bool escape_bytes = flags[2].count > 0;
    bool has_color = has_console;
    bool fake_perms = flags[12].count == 0;
    bool human_readable = flags[13].count > 0;
    bool disk_size = flags[14].count > 0;
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
        if (*width == 0) {
            *width = 0xffffffff;
        }
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
    OPTION_IF(opt, disk_size, OPTION_ALLOCATION_SIZE);

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
    if (!(opt & OPTION_VALID)) {
        if (opt == OPTION_HELP) {
            status = 0;
        }
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
                file_targets =
                    Mem_realloc(file_targets, file_capacity * sizeof(FileObj));
            }
            FileObj *file = file_targets + file_count;

            file->is_dir = attrs & FILE_ATTRIBUTE_DIRECTORY;
            file->is_link = attrs & FILE_ATTRIBUTE_REPARSE_POINT;
            WString_create(&file->path);
            WString_extend(&file->path, targets[ix]);
            get_detailed_info(file, file->path.buffer, NULL, NULL, 0,
                              opt & ~(OPTION_FAKE_PERMS));
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

    ExitProcess(status);
}
