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

bool WalkDir_begin(WalkCtx* ctx, const wchar_t* dir) {
    WString* s = &ctx->p.path;
    WString_create(s);

    if (dir[0] == L'/' && ((dir[1] >= 'a' && dir[1] <= 'z') || (dir[1] >= 'A' && dir[1] <= 'Z')) && (dir[2] == L'\0' || dir[2] == L'/' || dir[2] == L'\\')) {
        WString_append(s, dir[1]);
        WString_append(s, L':');
        dir += 2;
    }

    WString_extend(s, dir);

    for (unsigned ix = 0; ix < s->length; ++ix) {
        if (s->buffer[ix] == L'/') {
            s->buffer[ix] = L'\\';
        }
        if (s->buffer[ix] == L'?' || s->buffer[ix] == L'*') {
            ctx->handle = INVALID_HANDLE_VALUE;
            WString_free(s);
            return false;
        }
    }
    if (s->buffer[s->length - 1] != L'\\') {
        WString_append(s, L'\\');
    }
    WString_append(s, L'*');

    WIN32_FIND_DATAW data;
    ctx->handle = FindFirstFileW(s->buffer, &data);
    if (ctx->handle == INVALID_HANDLE_VALUE) {
        WString_free(s);
        return GetLastError() != ERROR_FILE_NOT_FOUND;
    }
    WString_clear(s);
    WString_extend(s, data.cFileName);
    ctx->p.is_dir = data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY;
    ctx->first = true;
    return true;
}

int WalkDir_next(WalkCtx* ctx, Path** path) {
    if (ctx->handle == INVALID_HANDLE_VALUE) {
        return 0;
    }
    WString* p = &ctx->p.path;
    if (ctx->first) {
        ctx->first = false;
        if (!(p->length == 1 && p->buffer[0] == L'.'|| 
            (p->length == 2 && p->buffer[1] == L'.' &&
             p->buffer[2] == L'.')
            )) {
            *path = &ctx->p;
            return 1;
        }
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

        WString_clear(&ctx->p.path);
        WString_extend(&ctx->p.path, data.cFileName);
        ctx->p.is_dir = data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY;
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
