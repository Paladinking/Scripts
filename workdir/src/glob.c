#include "glob.h"

bool read_utf16_file(WString_noinit* str, const wchar_t* filename) {
    HANDLE file = CreateFileW(filename, GENERIC_READ,
                              FILE_SHARE_READ, NULL,
                              OPEN_EXISTING, 0, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }
    LARGE_INTEGER size;
    if (!GetFileSizeEx(file, &size) || size.QuadPart >= 0xffffffff) {
        CloseHandle(file);
        return false;
    }
    while (size.QuadPart % sizeof(wchar_t) != 0) {
        --size.QuadPart;
    }

    if (!WString_create_capacity(str, (size.QuadPart / sizeof(wchar_t)) + 1)) {
        CloseHandle(file);
        return false;
    }
    unsigned char* buf = (unsigned char*)str->buffer;

    DWORD read = 0;
    while (read < size.QuadPart) {
        DWORD r;
        if (!ReadFile(file, buf + read, size.QuadPart - read, &r, NULL)) {
            if (GetLastError() != ERROR_HANDLE_EOF) {
                CloseHandle(file);
                return false;
            }
            break;
        }
        read += r;
    }
    str->buffer = (wchar_t*) buf;
    str->length = read / sizeof(wchar_t);
    str->buffer[str->length] = '\0';
    CloseHandle(file);
    return true;

}

bool read_text_file(String_noinit* str, const wchar_t* filename) {
    HANDLE file = CreateFileW(filename, GENERIC_READ,
                              FILE_SHARE_READ, NULL,
                              OPEN_EXISTING, 0, NULL);

    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }
    LARGE_INTEGER size;
    if (!GetFileSizeEx(file, &size) || size.QuadPart >= 0xffffffff) {
        CloseHandle(file);
        return false;
    }

    if (!String_create_capacity(str, size.QuadPart + 1)) {
        CloseHandle(file);
        return false;
    }

    DWORD read = 0;
    while (read < size.QuadPart) {
        DWORD r;
        if (!ReadFile(file, str->buffer + read, size.QuadPart - read, &r, NULL)) {
            if (GetLastError() != ERROR_HANDLE_EOF) {
                CloseHandle(file);
                return false;
            }
            break;
        }
        read += r;
    }
    str->length = read;
    str->buffer[str->length] = '\0';
    CloseHandle(file);
    return true;
}

bool get_workdir(WString* str) {
    WString_clear(str);
    DWORD res = GetCurrentDirectoryW(str->capacity, str->buffer);
    if (res == 0) {
        return false;
    }
    if (res < str->capacity) {
        str->length = res;
        return true;
    }
    WString_reserve(str, res);
    res = GetCurrentDirectoryW(str->capacity, str->buffer);
    if (res == 0 || res >= str->capacity) {
        return false;
    }
    str->length = res;
    return true;
}

bool is_file(const wchar_t* str) {
    return GetFileAttributesW(str) != INVALID_FILE_ATTRIBUTES;
}

bool find_file_relative(wchar_t* buf, size_t size, const wchar_t *filename, bool exists) {
    HMODULE mod;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            (wchar_t*)find_file_relative, &mod)) {
        return false;
    }

    DWORD res = GetModuleFileNameW(mod, buf, size);
    if (res == 0 || res >= size) {
        return false;
    }
    const wchar_t* ext = wcsrchr(filename, L'.');
    size_t name_len;
    if (ext == NULL) {
        name_len = wcslen(filename);
    } else {
        name_len = ext - filename;
    }
    wchar_t name_buf[512];
    if (name_len > 512) {
        return 0;
    }
    memcpy(name_buf, filename, name_len * sizeof(wchar_t));
    name_buf[name_len] = L'\0';
    wchar_t drive[10], dir[1024];
    if (_wsplitpath_s(buf, drive, 10, dir, 1024, NULL, 0, NULL, 0) != 0) {
        return false;
    }
    if (_wmakepath_s(buf, size, drive, dir, name_buf, ext) != 0) {
        return false;
    }
    if (!exists) {
        return true;
    }
    DWORD attr = GetFileAttributesW(buf);
    return attr != INVALID_FILE_ATTRIBUTES &&
           (attr & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

bool to_windows_path(const wchar_t* path, WString* s) {
    if (path[0] == L'/' && ((path[1] >= 'a' && path[1] <= 'z') || (path[1] >= 'A' && path[1] <= 'Z')) && (path[2] == L'\0' || path[2] == L'/' || path[2] == L'\\')) {
        WString_append(s, path[1]);
        WString_append(s, L':');
        path += 2;
    }
    WString_extend(s, path);
    for (unsigned ix = 0; ix < s->length; ++ix) {
        if (s->buffer[ix] == L'/') {
            s->buffer[ix] = L'\\';
        }
        if (s->buffer[ix] == L'?' || s->buffer[ix] == L'*') {
            return false;
        }
    }
    return true;
}

DWORD get_file_attrs(const wchar_t* path) {
    WString s;
    WString_create(&s);

    if (!to_windows_path(path, &s)) {
        WString_free(&s);
        SetLastError(ERROR_PATH_NOT_FOUND);
        return INVALID_FILE_ATTRIBUTES;
    }
    DWORD attrs = GetFileAttributesW(s.buffer);
    WString_free(&s);
    return attrs;
}

bool WalkDir_begin(WalkCtx* ctx, const wchar_t* dir, bool absolute_path) {
    WString* s = &ctx->p.path;
    ctx->absolute_path = absolute_path;
    WString_create(s);

    if (!to_windows_path(dir, s)) {
        ctx->handle = INVALID_HANDLE_VALUE;
        WString_free(s);
        return false;
    }

    if (s->buffer[s->length - 1] != L'\\') {
        WString_append(s, L'\\');
    }
    WString_append(s, L'*');

    WIN32_FIND_DATAW data;
    ctx->handle = FindFirstFileW(s->buffer, &data);
    if (ctx->handle == INVALID_HANDLE_VALUE) {
        WString_free(s);
        return false;
    }
    if (!ctx->absolute_path) {
        WString_clear(s);
    } else {
        WString_pop(s, 1); // Remove '*'
    }
    ctx->p.name_len = wcslen(data.cFileName); 
    WString_append_count(s, data.cFileName, ctx->p.name_len);
    ctx->p.is_dir = data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY;
    ctx->p.is_link = data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT;
    if (data.cFileName[0] == L'.' && (data.cFileName[1] == L'\0' || (
         data.cFileName[1] == L'.' && data.cFileName[2] == L'\0'))) {
        ctx->first = false;
    } else {
        ctx->first = true;
    }
    return true;
}

int WalkDir_next(WalkCtx* ctx, Path** path) {
    if (ctx->handle == INVALID_HANDLE_VALUE) {
        return 0;
    }
    WString* p = &ctx->p.path;
    if (ctx->first) {
        ctx->first = false;
        *path = &ctx->p;
        return 1;
    }
    while (1) {
        WIN32_FIND_DATAW data;
        if (!FindNextFileW(ctx->handle, &data)) {
            WString_free(&ctx->p.path);
            FindClose(ctx->handle);
            ctx->handle = INVALID_HANDLE_VALUE;
            *path = NULL;
            return 0;
        }
        if (data.cFileName[0] == L'.' && (data.cFileName[1] == L'\0' || (
             data.cFileName[1] == L'.' && data.cFileName[2] == L'\0'))) {
            continue;
        }
        if (ctx->absolute_path) {
            WString_pop(&ctx->p.path, ctx->p.name_len);
        } else {
            WString_clear(&ctx->p.path);
        }

        ctx->p.name_len = wcslen(data.cFileName);
        WString_append_count(&ctx->p.path, data.cFileName, ctx->p.name_len);
        ctx->p.is_dir = data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY;
        ctx->p.is_link = data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT;
        *path = &ctx->p;
        return 1;
    }
}

void WalkDir_abort(WalkCtx* ctx) {
    if (ctx->handle != INVALID_HANDLE_VALUE) {
        WString_free(&ctx->p.path);
        FindClose(ctx->handle);
        ctx->handle = INVALID_HANDLE_VALUE;
    }
}
