#include "glob.h"
#include "args.h"

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
    wchar_t* dir_sep = wcsrchr(buf, L'\\');
    if (dir_sep == NULL) {
        return false;
    }
    size_t len = dir_sep - buf;
    size_t name_len = wcslen(filename);
    if (filename[0] != '\\') {
        len += 1;
    }
    if (len + name_len + 1 >= size) {
        return false;
    }
    memcpy(buf + len, filename, (name_len + 1) * sizeof(wchar_t));
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

bool matches(const wchar_t* pattern, const wchar_t* str) {
    // Remove . and ..
    if (str[0] == L'.' && (str[1] == L'\0' || (str[1] == L'.' && str[2] == L'\0'))) {
        return false;
    }
    uint32_t p_ix = 0;
    uint32_t s_ix = 0;
    int64_t glob_ix = -1;
    while (pattern[p_ix] != L'\0') {
        wchar_t c = pattern[p_ix];
        if (c == L'*') {
            if (str[s_ix] == L'\0') {
                while (pattern[p_ix] != L'\0') {
                    if (pattern[p_ix] != L'*') {
                        return false;
                    }
                    ++p_ix;
                }
                return true;
            } else if (str[s_ix] == L'.' && s_ix == 0) {
                return false;
            } else if (pattern[p_ix + 1] == L'?' || towlower(str[s_ix]) == towlower(pattern[p_ix + 1])) {
                glob_ix = p_ix;
                p_ix += 2;
                ++s_ix;
            } else {
                ++s_ix;
            }
            continue;
        } else if (c == L'?') {
            if (str[s_ix] == L'.' && s_ix == 0) {
                return false;
            }
            ++s_ix;
            ++p_ix;
        } else {
            if (towlower(str[s_ix]) != towlower(c)) {
                if (glob_ix < 0) {
                    return false;
                }
                p_ix = glob_ix;
                continue;
            }
            ++s_ix;
            ++p_ix;
        }
    }
    return str[s_ix] == L'\0';
}


bool Glob_begin(const wchar_t* pattern, GlobCtx* ctx) {
    ctx->handle = INVALID_HANDLE_VALUE;
    ctx->stack_size = 0;
    ctx->stack = NULL;
    wchar_t* s = Mem_alloc((wcslen(pattern) + 1) * sizeof(wchar_t));
    if (s == NULL) {
        return false;
    }

    uint32_t offset = 0;

    if (pattern[0] == L'/' && ((pattern[1] >= 'a' && pattern[1] <= 'z') || 
        (pattern[1] >= 'A' && pattern[1] <= 'Z')) && 
        (pattern[2] == L'\0' || pattern[2] == L'/' || pattern[2] == L'\\')) {
        s[0] = pattern[1];
        s[1] = L':';
        offset = 2;
    }

    bool has_glob = false;
    for (; pattern[offset] != L'\0'; ++offset) {
        if (pattern[offset] == L'/') {
            s[offset] = L'\\';
        } else {
            s[offset] = pattern[offset];
        }
        if (s[offset] == L'*' || s[offset] == L'?') {
            has_glob = true;
        }
    }
    s[offset] = L'\0';
    if (!has_glob) {
        goto fail;
    }
    if (!WString_create_capacity(&ctx->p.path, offset)) {
        goto fail;
    }

    ctx->stack_capacity = 4;
    ctx->stack = Mem_alloc(4 * sizeof(ctx->stack[0]));
    if (ctx->stack == NULL) {
        WString_free(&ctx->p.path);
        goto fail;
    }
    ctx->stack_size = 1;
    ctx->stack[0].pattern_offset = 0;
    ctx->stack[0].pattern = s;
    ctx->last_segment = 0;

    return true;
fail:
    Mem_free(s);
    return false;
}

bool Glob_next(GlobCtx* ctx, Path** path) {
    // Stack is NULL if init failed, or stack is emtied and freed.
    if (ctx->stack == NULL) {
        return false;
    }
    Path* p = &ctx->p;
    WIN32_FIND_DATAW data;

    WString_clear(&p->path);
    while (ctx->stack_size > 0) {
        struct _GlobCtxNode n = ctx->stack[ctx->stack_size - 1];
        if (ctx->handle != INVALID_HANDLE_VALUE) {
            while (FindNextFileW(ctx->handle, &data)) {
                if (!matches(n.pattern + ctx->last_segment, data.cFileName)) {
                    continue;
                }
                if (!WString_append_count(&p->path, n.pattern, ctx->last_segment)) {
                    goto fail;
                }
                if (!WString_extend(&p->path, data.cFileName)) {
                    goto fail;
                }
                p->is_dir = data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY;
                p->is_link = data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT;
                *path = p;
                return true;
            }
            --ctx->stack_size;
            Mem_free(n.pattern);
            ctx->handle = INVALID_HANDLE_VALUE;
            continue;
        }

        bool has_glob = false;
        ctx->last_segment = n.pattern_offset;
        for (uint32_t ix = n.pattern_offset; true ; ++ix) {
            if (!has_glob && n.pattern[ix] == L'\\') {
                ctx->last_segment = ix + 1;
            } else if (n.pattern[ix] == L'*' || n.pattern[ix] == L'?') {
                has_glob = true;
            } else if (has_glob && n.pattern[ix] == L'\\') {
                uint32_t rem_len = wcslen(n.pattern + ix);
                n.pattern[ix] = L'\0';
                HANDLE h = FindFirstFileW(n.pattern, &data);
                --ctx->stack_size;
                if (h == INVALID_HANDLE_VALUE) {
                    Mem_free(n.pattern);
                    break;
                }
                do {
                    if (!(data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                        continue;
                    }
                    if (!matches(n.pattern + n.pattern_offset, data.cFileName)) {
                        continue;
                    }
                    if (ctx->stack_size == ctx->stack_capacity) {
                        uint32_t cap = 2 * ctx->stack_size;
                        struct _GlobCtxNode* stack = 
                            Mem_realloc(ctx->stack, 
                                    cap * sizeof(struct _GlobCtxNode));
                        if (stack == NULL) {
                            Mem_free(n.pattern);
                            goto fail;
                        }
                        ctx->stack = stack;
                        ctx->stack_capacity = cap;
                    }
                    uint32_t s_len = wcslen(data.cFileName);
                    uint32_t tot_len = s_len + ctx->last_segment + rem_len + 1;
                    wchar_t* s = Mem_alloc(tot_len * sizeof(wchar_t));
                    if (s == NULL) {
                        Mem_free(n.pattern);
                        goto fail;
                    }
                    memcpy(s, n.pattern, ctx->last_segment * sizeof(wchar_t));
                    memcpy(s + ctx->last_segment, data.cFileName, s_len * sizeof(wchar_t));
                    memcpy(s + ctx->last_segment + s_len, n.pattern + ix, (rem_len + 1) * sizeof(wchar_t));
                    s[ctx->last_segment + s_len] = L'\\';
                    ctx->stack[ctx->stack_size].pattern = s;
                    ctx->stack[ctx->stack_size].pattern_offset = ctx->last_segment + s_len + 1;
                    ++ctx->stack_size;
                } while (FindNextFileW(h, &data));
                Mem_free(n.pattern);
                break;
            } else if (n.pattern[ix] == L'\0') {
                if (has_glob) {
                    HANDLE h = FindFirstFileW(n.pattern, &data);
                    if (h == INVALID_HANDLE_VALUE) {
                        --ctx->stack_size;
                        Mem_free(n.pattern);
                        break;
                    }
                    ctx->handle = h;
                    if (matches(n.pattern + ctx->last_segment, data.cFileName)) {
                        if (!WString_append_count(&p->path, n.pattern, ctx->last_segment)) {
                            goto fail;
                        }
                        if (!WString_extend(&p->path, data.cFileName)) {
                            goto fail;
                        }
                        p->is_dir = data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY;
                        p->is_link = data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT;
                        *path = p;
                        return true;
                    }
                } else if (GetFileAttributesW(n.pattern) != INVALID_FILE_ATTRIBUTES) {
                    if (!WString_extend(&p->path, n.pattern)) {
                        goto fail;
                    }
                    --ctx->stack_size;
                    Mem_free(n.pattern);
                    p->is_dir = data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY;
                    p->is_link = data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT;
                    *path = p;
                    return true;
                } else {
                    --ctx->stack_size;
                    Mem_free(n.pattern);
                }
                break;
            }
        }
    }

fail:
    if (ctx->handle != INVALID_HANDLE_VALUE) {
        FindClose(ctx->handle);
    }
    WString_free(&ctx->p.path);
    for (uint32_t ix = 0; ix < ctx->stack_size; ++ix) {
        Mem_free(ctx->stack[ix].pattern);
    }
    ctx->stack_size = 0;
    Mem_free(ctx->stack);
    ctx->stack = NULL;
    return false;
}

void Glob_abort(GlobCtx* ctx) {
    if (ctx->stack == NULL) {
        return;
    }
    if (ctx->handle != INVALID_HANDLE_VALUE) {
        FindClose(ctx->handle);
    }
    WString_free(&ctx->p.path);
    for (uint32_t ix = 0; ix < ctx->stack_size; ++ix) {
        Mem_free(ctx->stack[ix].pattern);
    }
    ctx->stack_size = 0;
    Mem_free(ctx->stack);
    ctx->stack = NULL;
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


wchar_t** glob_command_line_with(const wchar_t* args, int* argc, unsigned options) {
    *argc = 0;
    size_t ix = 0;
    size_t cap = 4;
    wchar_t** argv = Mem_alloc(4 * sizeof(wchar_t*));
    if (argv == NULL) {
        return NULL;
    }

    size_t len = 0;
    size_t size = 0;
    BOOL quoted;
    GlobCtx ctx;
    Path* path;
    while (1) {
        size_t offset = ix;
        if (!get_arg_len(args, &ix, &len, &quoted, options)) {
            break;
        }
        wchar_t* arg = Mem_alloc((len + 1) * sizeof(wchar_t));
        if (arg == NULL) {
            goto fail;
        }
        get_arg(args, &offset, arg, options);

        bool glob_match = false;
        if (!quoted) {
            Glob_begin(arg, &ctx);
            glob_match = Glob_next(&ctx, &path);
        }
        if (!glob_match) {
            if (cap == *argc) {
                wchar_t** a = Mem_realloc(argv, 2 * cap * sizeof(wchar_t*));
                if (a == NULL) {
                    Mem_free(arg);
                    goto fail;
                }
                cap *= 2;
                argv = a;
            }
            argv[*argc] = arg;
            ++(*argc);
            size += (len + 1) * sizeof(wchar_t);
            continue;
        }
        Mem_free(arg);
        do {
            if (cap == *argc) {
                wchar_t** a = Mem_realloc(argv, 2 * cap * sizeof(wchar_t*));
                if (a == NULL) {
                    Glob_abort(&ctx);
                    goto fail;
                }
                cap *= 2;
                argv = a;
            }
            wchar_t* arg = Mem_alloc((path->path.length + 1) * sizeof(wchar_t));
            if (arg == NULL) {
                Glob_abort(&ctx);
                goto fail;
            }
            memcpy(arg, path->path.buffer, (path->path.length + 1) * sizeof(wchar_t));
            argv[*argc] = arg;
            ++(*argc);
            size += (path->path.length + 1) * sizeof(wchar_t);
        } while (Glob_next(&ctx, &path));
    }

    size += (*argc) * sizeof(wchar_t*);
    unsigned char* dest = Mem_realloc(argv, size);
    if (dest == NULL) {
        goto fail;
    }
    wchar_t* buffer = (wchar_t*)(dest + (*argc * sizeof(wchar_t*)));
    argv = (wchar_t**)dest;
    for (uint32_t ix = 0; ix < *argc; ++ix) {
        size_t len = wcslen(argv[ix]);
        memcpy(buffer, argv[ix], (len + 1) * sizeof(wchar_t));
        Mem_free(argv[ix]);
        argv[ix] = buffer;
        buffer += len + 1;
    }

    return argv;
fail:
    for (size_t ix = 0; ix < *argc; ++ix) {
        Mem_free(argv[ix]);
    }
    Mem_free(argv);
    return NULL;
}

wchar_t** glob_command_line(const wchar_t* args, int* argc) {
    return glob_command_line_with(args, argc, ARG_OPTION_STD);
}
