#include "glob.h"
#include "args.h"


#ifdef NEXTLINE_FAST
#include <immintrin.h>
char* find_next_line_fast(LineCtx* ctx, uint64_t* len);
#define find_next_line find_next_line_fast
#else
char* find_next_line(LineCtx* ctx, uint64_t* len);
#endif

#define LINE_BUFFER_SIZE (32768)

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
    str->buffer[str->length] = L'\0';
    CloseHandle(file);
    return true;

}

HANDLE open_file_read(const ochar_t* filename) {
#ifdef NARROW_OCHAR
    WString name;
    if (!WString_create_capacity(&name, 512)) {
        return INVALID_HANDLE_VALUE;
    }
    if (!WString_from_utf8_str(&name, filename)) {
        WString_free(&name);
        return INVALID_HANDLE_VALUE;
    }
    wchar_t* fname = name.buffer;
#else
    const wchar_t* fname = filename;
#endif
    HANDLE file = CreateFileW(fname, GENERIC_READ, FILE_SHARE_READ, NULL,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
#ifdef NARROW_OCHAR
    WString_free(&name);
#endif
    return file;
}

HANDLE open_file_write(const ochar_t* filename) {
#ifdef NARROW_OCHAR
    WString name;
    if (!WString_create_capacity(&name, 512)) {
        return INVALID_HANDLE_VALUE;
    }
    if (!WString_from_utf8_str(&name, filename)) {
        WString_free(&name);
        return INVALID_HANDLE_VALUE;
    }
    wchar_t* fname = name.buffer;
#else
    const wchar_t* fname = filename;
#endif
    HANDLE file = CreateFileW(fname, GENERIC_WRITE,
                              0, NULL, CREATE_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL, NULL);
#ifdef NARROW_OCHAR
    WString_free(&name);
#endif
    return file;
}

bool write_file(const ochar_t* filename, const uint8_t* buf, uint64_t len) {
    HANDLE file = open_file_write(filename);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }
    uint64_t written = 0;
    DWORD w = 0;
    while (written < len) {
        DWORD to_write = 65535;
        if (len - written < 65535) {
            to_write = len - written;
        }
        if (!WriteFile(file, buf + written, to_write, &w, NULL)) {
            CloseHandle(file);
            return false;
        }
        written += w;
    }
    CloseHandle(file);
    return true;
}


bool read_text_file(String_noinit* str, const ochar_t* filename) {
#ifdef NARROW_OCHAR
    WString name;
    if (!WString_create_capacity(&name, 512)) {
        return false;
    }
    if (!WString_from_utf8_str(&name, filename)) {
        WString_free(&name);
        return false;
    }
    wchar_t* fname = name.buffer;
#else
    const wchar_t* fname = filename;
#endif
    HANDLE file = CreateFileW(fname, GENERIC_READ,
                              FILE_SHARE_READ, NULL,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
#ifdef NARROW_OCHAR
    WString_free(&name);
#endif

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

bool is_file(const ochar_t* str) {
#ifdef NARROW_OCHAR
    WString name;
    if (!WString_create_capacity(&name, 256)) {
        return false;
    }
    if (!WString_from_utf8_str(&name, str)) {
        WString_free(&name);
        return false;
    }
    bool res = GetFileAttributesW(name.buffer) != INVALID_FILE_ATTRIBUTES;
    WString_free(&name);
    return res;
#else
    return GetFileAttributesW(str) != INVALID_FILE_ATTRIBUTES;
#endif
}

bool is_directory(const wchar_t *str) {
    DWORD attr = GetFileAttributesW(str);
    if (attr == INVALID_FILE_ATTRIBUTES) {
        return false;
    }
    return attr & FILE_ATTRIBUTE_DIRECTORY;
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
        if (!WString_append(s, path[1]) || !WString_append(s, L':')) {
            SetLastError(ERROR_OUTOFMEMORY);
            return false;
        }
        path += 2;
    }
    if (!WString_extend(s, path)) {
        SetLastError(ERROR_OUTOFMEMORY);
        return false;
    }
    for (unsigned ix = 0; ix < s->length; ++ix) {
        if (s->buffer[ix] == L'/') {
            s->buffer[ix] = L'\\';
        }
        if (s->buffer[ix] == L'?' || s->buffer[ix] == L'*') {
            SetLastError(ERROR_PATH_NOT_FOUND);
            return false;
        }
    }
    return true;
}


bool make_absolute(const wchar_t* path, WString* dest) {
    WString win;
    if (!WString_create(&win)) {
        return false;
    }
    if (!to_windows_path(path, &win)) {
        return false;
    }
    DWORD size = GetFullPathNameW(win.buffer, dest->capacity, dest->buffer, NULL);
    if (size >= dest->capacity) {
        if (!WString_reserve(dest, size)) {
            WString_free(&win);
            return false;
        }
        size = GetFullPathNameW(win.buffer, dest->capacity, dest->buffer, NULL);
        if (size >= dest->capacity) {
            WString_free(&win);
            return false;
        }
    }
    dest->length = size;
    WString_free(&win);
    return true;
}


DWORD get_file_attrs(const wchar_t* path) {
    WString s;
    WString_create(&s);

    if (!to_windows_path(path, &s)) {
        WString_free(&s);
        return INVALID_FILE_ATTRIBUTES;
    }
    DWORD attrs = GetFileAttributesW(s.buffer);
    WString_free(&s);
    return attrs;
}

bool matches_glob(const wchar_t* pattern, const wchar_t* str) {
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


#ifdef NEXTLINE_FAST

char* find_next_line_fast(LineCtx* ctx, uint64_t* len) {
    char* start = ctx->line.buffer + ctx->str_offset;
    char* ptr = start;
    __m128i m1 = _mm_set1_epi8('\n');
    __m128i m2 = _mm_set1_epi8('\r');
    __m128i z = _mm_setzero_si128();
    uint32_t zm = 0;
    while (1) {
        if (zm) {
            ctx->binary = true;
        }
        __m128i m = _mm_loadu_si128((__m128i*)ptr);
        __m128i cmp1 = _mm_cmpeq_epi8(m, m1);
        __m128i cmp2 = _mm_cmpeq_epi8(m, m2);

        zm = _mm_movemask_epi8(_mm_cmpeq_epi8(m, z));

        uint32_t mask = _mm_movemask_epi8(_mm_or_si128(cmp1, cmp2));

        if (mask == 0) {
            ptr += 16;
            continue;
        }

        uint32_t ix = _tzcnt_u32(mask);
        if (zm) {
            zm = _tzcnt_u32(zm);
            if (zm < ix) {
                ctx->binary = true;
            }
        }

        ix += ptr - start;
        *len = ix;
        ix += ctx->str_offset;
        if (ctx->line.buffer[ix] == '\r') {
            if (ctx->line.buffer[ix + 1] == '\n') {
                ++ix;
            }
        }
        if (ix + 1 == ctx->line.length) {
            ctx->line.length = 0;
            ctx->str_offset = 0;
            ctx->ended_cr = ctx->line.buffer[ix] == '\r';
        } else {
            ctx->str_offset = ix + 1;
        }
        return start;
    }
}

#define find_next_line find_next_line_fast

#else

char* find_next_line(LineCtx* ctx, uint64_t* len) {
    char* start = ctx->line.buffer + ctx->str_offset;
    for (uint64_t ix = ctx->str_offset; true; ++ix) {
        if (ctx->line.buffer[ix] == '\0') {
            ctx->binary = true;
        }
        if (ctx->line.buffer[ix] == '\r') {
            *len = ix - ctx->str_offset;
            if (ctx->line.buffer[ix + 1] == '\n') {
                ++ix;
            }
            if (ix + 1 == ctx->line.length) {
                ctx->line.length = 0;
                ctx->str_offset = 0;
                ctx->ended_cr = ctx->line.buffer[ix] != '\n';
            } else {
                ctx->str_offset = ix + 1;
            }
            return start;
        }
        if (ctx->line.buffer[ix] == '\n') {
            *len = ix - ctx->str_offset;
            if (ix + 1 == ctx->line.length) {
                ctx->line.length = 0;
                ctx->str_offset = 0;
                ctx->ended_cr = false;
            } else {
                ctx->str_offset = ix + 1;
            }
            return start;
        }
    }
}
#endif


bool ConsoleLineIter_begin(LineCtx *ctx, HANDLE in) {
    ctx->file = in;
    ctx->eof = false;
    ctx->ended_cr = false;
    ctx->str_offset = 0;
    ctx->offset = 0;
    ctx->binary = false;
    if (!String_create(&ctx->line)) {
        ctx->file = INVALID_HANDLE_VALUE;
        SetLastError(ERROR_OUTOFMEMORY);
        return false;
    }
    if (!WString_create_capacity(&ctx->wbuffer, 4096)) {
        ctx->file = INVALID_HANDLE_VALUE;
        String_free(&ctx->line);
        SetLastError(ERROR_OUTOFMEMORY);
        return false;
    }
    return true;
}


char* ConsoleLineIter_next(LineCtx* ctx, uint64_t* len) {
    if (ctx->file == INVALID_HANDLE_VALUE) {
        if (ctx->eof) {
            String_free(&ctx->line);
            ctx->eof = false;
        }
        return NULL;
    }

    DWORD read;
    while (ctx->line.length == 0) {
        if (ctx->eof) {
            ctx->eof = false;
            goto eol;
        }
        if (!WString_reserve(&ctx->wbuffer, ctx->wbuffer.length + 4097)) {
            goto fail;
        }
        if (!ReadConsoleW(ctx->file, ctx->wbuffer.buffer + ctx->wbuffer.length,
                          4096, &read, NULL) || read == 0) {
            goto eol;
        }
        uint64_t start = ctx->wbuffer.length;
        ctx->wbuffer.length += read;
        
        for (uint64_t i = start; i < ctx->wbuffer.length; ++i) {
            if (ctx->wbuffer.buffer[i] == 0x1A) {
                ctx->eof = true;
                ctx->wbuffer.length = i;
                CloseHandle(ctx->file);
                break;
            }
        }
        uint64_t ix = ctx->wbuffer.length;
        uint64_t line_len = 0;
        do {
            --ix;
            if (ctx->wbuffer.buffer[ix] == L'\n' ||
                ctx->wbuffer.buffer[ix] == L'\r') {
                line_len = ix + 1;
                break;
            }

        } while (ix > start);

        if (line_len > 0) {
            ctx->str_offset = 0;
            if (ctx->ended_cr) {
                if (ctx->wbuffer.buffer[0] == L'\n') {
                    if (line_len == 1) {
                        continue;
                    } else {
                        ctx->str_offset = 1;
                    }
                }
                ctx->ended_cr = false;
            }
            if (!String_from_utf16_bytes(&ctx->line, ctx->wbuffer.buffer,
                                         line_len)) {
                goto fail;
            }
#ifdef NEXTLINE_FAST // Make space for vector registers
            if (!String_reserve(&ctx->line, ctx->line.length + 16)) {
                goto fail;
            }
#endif
            WString_remove(&ctx->wbuffer, 0, line_len);
        }
    }
    return find_next_line(ctx, len);
fail:
    ctx->file = INVALID_HANDLE_VALUE;
    String_free(&ctx->line);
    WString_free(&ctx->wbuffer);
    return NULL;
eol:
    ctx->file = INVALID_HANDLE_VALUE;
    if (ctx->wbuffer.length > 0) {
        if (String_from_utf16_bytes(&ctx->line, ctx->wbuffer.buffer,
                                     ctx->wbuffer.length)) {
            ctx->eof = true;
            ctx->binary = ctx->binary || memchr(ctx->line.buffer, ctx->line.length, 0) != NULL;
            WString_free(&ctx->wbuffer);
            *len = ctx->line.length;
            return ctx->line.buffer;
        }
    }
    String_free(&ctx->line);
    WString_free(&ctx->wbuffer);
    return NULL;
}

void ConsoleLineIter_abort(LineCtx *ctx) {
    if (ctx->file == INVALID_HANDLE_VALUE) {
        if (ctx->eof) {
            WString_free(&ctx->wbuffer);
            ctx->eof = false;
        }
        return;
    }
    String_free(&ctx->line);
    WString_free(&ctx->wbuffer);
    ctx->file = INVALID_HANDLE_VALUE;
}

bool SyncLineIter_begin(LineCtx* ctx, HANDLE file) {
    ctx->file = file;
    ctx->eof = false;
    ctx->ended_cr = false;
    ctx->str_offset = 0;
    ctx->offset = 0;
    ctx->binary = false;
    if (!String_create_capacity(&ctx->line, LINE_BUFFER_SIZE + 50)) {
        ctx->file = INVALID_HANDLE_VALUE;
        SetLastError(ERROR_OUTOFMEMORY);
        return false;
    }
    return true;
}

char* SyncLineIter_next(LineCtx* ctx, uint64_t* len) {
    if (ctx->file == INVALID_HANDLE_VALUE) {
        if (ctx->eof) {
            String_free(&ctx->line);
            ctx->eof = false;
        }
        return NULL;
    }

    while (ctx->offset == 0) {
        if (!String_reserve(&ctx->line, LINE_BUFFER_SIZE + ctx->line.length + 16)) {
            goto fail;
        }
        DWORD read;
        if (!ReadFile(ctx->file, ctx->line.buffer + ctx->line.length,
                     LINE_BUFFER_SIZE, &read, NULL)) {
            if (GetLastError() == ERROR_BROKEN_PIPE ||
                GetLastError() == ERROR_HANDLE_EOF) {
                goto eof;
            }
            goto fail;
        }
        if (read == 0) {
            if (GetFileType(ctx->file) == FILE_TYPE_PIPE) {
                continue;
            }
            goto eof;
        }
        uint64_t start = ctx->line.length;
        ctx->line.length += read;
        uint64_t ix = ctx->line.length;
        do {
            --ix;
            if (ctx->line.buffer[ix] == '\n' || ctx->line.buffer[ix] == '\r') {
                if (ctx->ended_cr) {
                    if (ctx->line.buffer[0] == '\n') {
                        if (ix == 0) {
                            break;
                        } else {
                            ctx->str_offset = 1;
                        }
                    }
                    ctx->ended_cr = false;
                }
                ctx->offset = ctx->line.length - ix;
                ctx->line.length = ix + 1;
                break;
            }
        } while (ix > start);
    }
    char* res = find_next_line(ctx, len);
    if (ctx->line.length == 0) {
        ctx->line.length = ctx->offset - 1;
        memmove(ctx->line.buffer, res + *len + 1, ctx->line.length);
        ctx->offset = 0;
    }
    return res;
fail:
    String_free(&ctx->line);
    ctx->file = INVALID_HANDLE_VALUE;
    return NULL;
eof:
    ctx->file = INVALID_HANDLE_VALUE;
    ctx->eof = true;
    *len = ctx->line.length;
    ctx->binary = ctx->binary || memchr(ctx->line.buffer, ctx->line.length, 0) != NULL;
    return ctx->line.buffer;
}

void SyncLineIter_abort(LineCtx* ctx) {
    if (ctx->file == INVALID_HANDLE_VALUE) {
        if (ctx->eof) {
            String_free(&ctx->line);
            ctx->eof = false;
        }
        return;
    }
    String_free(&ctx->line);
    ctx->file = INVALID_HANDLE_VALUE;
}

bool LineIter_begin(LineCtx* ctx, const ochar_t* filename) {
#ifdef NARROW_OCHAR
    WString s;
    if (!WString_create(&s)) {
        ctx->file = INVALID_HANDLE_VALUE;
        return false;
    }
    if (!WString_from_utf8_str(&s, filename)) {
        WString_free(&s);
        ctx->file = INVALID_HANDLE_VALUE;
        return false;
    }
    wchar_t* name = s.buffer;
#else
    const wchar_t* name = filename;
#endif
    ctx->file = CreateFileW(name, GENERIC_READ, FILE_SHARE_READ, NULL,
                            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL |
                            FILE_FLAG_OVERLAPPED, NULL);
#ifdef NARROW_OCHAR
    WString_free(&s);
#endif

    ctx->eof = false;
    ctx->ended_cr = false;
    ctx->binary = false;
    if (ctx->file == INVALID_HANDLE_VALUE) {
        return false;
    }
    // + 50 to because some space will be needed when read does not end with newline
    if (!String_create_capacity(&ctx->buffer, LINE_BUFFER_SIZE + 50)) {
        CloseHandle(ctx->file);
        ctx->file = INVALID_HANDLE_VALUE;
        SetLastError(ERROR_OUTOFMEMORY);
        return false;
    }
    if (!String_create_capacity(&ctx->line, LINE_BUFFER_SIZE + 50)) {
        String_free(&ctx->buffer);
        CloseHandle(ctx->file);
        ctx->file = INVALID_HANDLE_VALUE;
        SetLastError(ERROR_OUTOFMEMORY);
    }
    memset(&ctx->o, 0, sizeof(OVERLAPPED));
    ctx->o.hEvent = CreateEventW(NULL, FALSE, FALSE, NULL);
    if (ctx->o.hEvent == NULL) {
        DWORD err = GetLastError();
        String_free(&ctx->line);
        String_free(&ctx->buffer);
        CloseHandle(ctx->file);
        ctx->file = INVALID_HANDLE_VALUE;
        SetLastError(err);
        return false;
    }
    ctx->offset = 0;

    if (!ReadFile(ctx->file, ctx->buffer.buffer,
                  LINE_BUFFER_SIZE, NULL, &ctx->o)) {
        DWORD err = GetLastError();
        if (err != ERROR_IO_PENDING) {
            CloseHandle(ctx->o.hEvent);
            String_free(&ctx->line);
            String_free(&ctx->buffer);
            CloseHandle(ctx->file);
            ctx->file = INVALID_HANDLE_VALUE;
            SetLastError(err);
            return false;
        }
    }
    ctx->str_offset = 0;
    return true;
}


// Yeild lines from file.
// Uses overlapped I/O.
char* LineIter_next(LineCtx* ctx, uint64_t* len) {
    if (ctx->file == INVALID_HANDLE_VALUE) {
        if (ctx->eof) {
            String_free(&ctx->buffer);
            String_free(&ctx->line);
            ctx->eof = false;
        }
        // Already aborted or failed init
        return NULL;
    }

    while (ctx->line.length == 0) {
        DWORD read;
        if (!GetOverlappedResult(ctx->file, &ctx->o, &read, TRUE)) {
            if (GetLastError() == ERROR_HANDLE_EOF) {
                goto eof;
            }
            goto fail;
        }
        if (read == 0) {
            continue;
        }
        int64_t start = ctx->buffer.length;
        ctx->offset += read;
        ctx->o.Offset = (uint32_t)ctx->offset;
        ctx->o.OffsetHigh = (ctx->offset >> 32);

        uint64_t line_len = 0;
        ctx->buffer.length += read;
        int64_t ix = ctx->buffer.length;
        do {
            --ix;
            if (ctx->buffer.buffer[ix] == '\n' ||
                ctx->buffer.buffer[ix] == '\r') {
                line_len = ix + 1;
                break;
            }
        } while (ix > start);
    found:

        if (line_len > 0) {
            String tmp = ctx->line;
            ctx->line = ctx->buffer;
            ctx->buffer = tmp;

            // Append all characters after last '\r' or '\n' to buffer
            String_clear(&ctx->buffer);
            if (!String_append_count(&ctx->buffer, ctx->line.buffer + line_len,
                                ctx->line.length - line_len)) {
                goto fail;
            }
            ctx->str_offset = 0;
            if (ctx->ended_cr) {
                // Handle the case were last read ended with '\r',
                // and this read starts with '\n'. That is one CRLF newline.
                if (ctx->line.buffer[0] == '\n') {
                    if (line_len == 1) {
                        // The '\n' was the only '\n' or '\r' in this read,
                        // need another iteration
                        --line_len;
                    } else {
                        // Skip first byte
                        ctx->str_offset = 1;
                    }
                }
                ctx->ended_cr = false;
            }
            ctx->line.length = line_len;
            ctx->line.buffer[line_len] = '\0';
        }
        if (!String_reserve(&ctx->buffer,
                ctx->buffer.length + LINE_BUFFER_SIZE + 33)) {
            goto fail;
        }
        if (!ReadFile(ctx->file, ctx->buffer.buffer + ctx->buffer.length,
                    LINE_BUFFER_SIZE, NULL, &ctx->o)) {
            DWORD err = GetLastError();
            if (err == ERROR_HANDLE_EOF) {
                goto eof;
            } else if (err != ERROR_IO_PENDING) {
                goto fail;
            }
        }
    }
    return find_next_line(ctx, len);
fail:
    CloseHandle(ctx->file);
    CloseHandle(ctx->o.hEvent);
    String_free(&ctx->line);
    String_free(&ctx->buffer);
    ctx->file = INVALID_HANDLE_VALUE;
    return NULL;
eof:
    CloseHandle(ctx->file);
    CloseHandle(ctx->o.hEvent);
    ctx->file = INVALID_HANDLE_VALUE;
    if (ctx->buffer.length > 0) {
        ctx->eof = true;
        *len = ctx->buffer.length;
        ctx->binary = ctx->binary ||
         memchr(ctx->buffer.buffer, ctx->buffer.length, 0) != NULL;
        return ctx->buffer.buffer;
    }
    String_free(&ctx->buffer);
    String_free(&ctx->line);
    return NULL;
}


void LineIter_abort(LineCtx* ctx) {
    if (ctx->file == INVALID_HANDLE_VALUE) {
        if (ctx->eof) {
            String_free(&ctx->buffer);
            String_free(&ctx->line);
            ctx->eof = false;
        }
        return;
    }
    // Stop I/O and wait for it to stop
    if (CancelIo(ctx->file)) {
        DWORD d;
        GetOverlappedResult(ctx->file, &ctx->o, &d, TRUE);
    }
    CloseHandle(ctx->file);
    CloseHandle(ctx->o.hEvent);
    String_free(&ctx->line);
    String_free(&ctx->buffer);
    ctx->file = INVALID_HANDLE_VALUE;
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

    for (; pattern[offset] != L'\0'; ++offset) {
        if (pattern[offset] == L'/') {
            s[offset] = L'\\';
        } else {
            s[offset] = pattern[offset];
        }
    }
    s[offset] = L'\0';
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
                if (!matches_glob(n.pattern + ctx->last_segment, data.cFileName)) {
                    continue;
                }
                if (!WString_append_count(&p->path, n.pattern, ctx->last_segment)) {
                    goto fail;
                }
                if (!WString_extend(&p->path, data.cFileName)) {
                    goto fail;
                }
                p->attrs = data.dwFileAttributes;
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
                    if (!matches_glob(n.pattern + ctx->last_segment, data.cFileName)) {
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
                    if (matches_glob(n.pattern + ctx->last_segment, data.cFileName)) {
                        if (!WString_append_count(&p->path, n.pattern, ctx->last_segment)) {
                            goto fail;
                        }
                        if (!WString_extend(&p->path, data.cFileName)) {
                            goto fail;
                        }
                        p->attrs = data.dwFileAttributes;
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
                    p->attrs = data.dwFileAttributes;
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
    if (!WString_create(s)) {
        SetLastError(ERROR_OUTOFMEMORY);
    }

    if (!to_windows_path(dir, s)) {
        DWORD err = GetLastError();
        ctx->handle = INVALID_HANDLE_VALUE;
        WString_free(s);
        SetLastError(err);
        return false;
    }

    if (s->buffer[s->length - 1] != L'\\') {
        if (!WString_append(s, L'\\')) {
            WString_free(s);
            SetLastError(ERROR_OUTOFMEMORY);
            ctx->handle = INVALID_HANDLE_VALUE;
            return false;
        }
    }
    if (!WString_append(s, L'*')) {
        WString_free(s);
        SetLastError(ERROR_OUTOFMEMORY);
        ctx->handle = INVALID_HANDLE_VALUE;
        return false;
    }

    WIN32_FIND_DATAW data;
    ctx->handle = FindFirstFileW(s->buffer, &data);
    if (ctx->handle == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        WString_free(s);
        SetLastError(err);
        return false;
    }
    if (!ctx->absolute_path) {
        WString_clear(s);
    } else {
        WString_pop(s, 1); // Remove '*'
    }
    ctx->p.name_len = wcslen(data.cFileName); 
    if (!WString_append_count(s, data.cFileName, ctx->p.name_len)) {
        FindClose(ctx->handle);
        WString_free(s);
        SetLastError(ERROR_OUTOFMEMORY);
        ctx->handle = INVALID_HANDLE_VALUE;
        return false;
    }
    ctx->p.attrs = data.dwFileAttributes;
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
            DWORD err = GetLastError();
            WString_free(&ctx->p.path);
            FindClose(ctx->handle);
            ctx->handle = INVALID_HANDLE_VALUE;
            *path = NULL;
            SetLastError(err);
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
        if (!WString_append_count(&ctx->p.path, data.cFileName, ctx->p.name_len)) {
            WalkDir_abort(ctx);
            SetLastError(ERROR_OUTOFMEMORY);
            return 0;
        }
        ctx->p.attrs = data.dwFileAttributes;
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

#ifndef NARROW_OCHAR

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
    bool quoted;
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

#endif
