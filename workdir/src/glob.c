#include "glob.h"

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

bool WalkDir_begin(WalkCtx* ctx, const wchar_t* dir) {
    WString* s = &ctx->p.path;
    WString_create(s);
    WString_extend(s, dir);

    for (unsigned ix = 0; dir[ix] != L'\0'; ++ix) {
        if (dir[ix] == L'/') {
            s->buffer[ix] = L'\\';
        }
        if (dir[ix] == L'?' || dir[ix] == L'*') {
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
            if (GetLastError() != ERROR_FILE_NOT_FOUND) {
                return -1;
            }
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
