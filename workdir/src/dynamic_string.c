#define _NO_CRT_STDIO_INLINE
#include <windows.h>
#ifndef DYNAMIC_STRING_NO_FMT
#include <stdarg.h>

extern int _vscwprintf(const wchar_t *format, va_list argptr);
extern int _vsnwprintf(wchar_t *buffer, size_t count, const wchar_t *format,
                       va_list argptr);

#endif
#include "dynamic_string.h"
#include "mem.h"

bool String_append(String *s, const char c) {
    if (!String_reserve(s, s->length + 1)) {
        return false;
    }
    s->buffer[s->length] = c;
    s->length += 1;
    s->buffer[s->length] = L'\0';
    return true;
}

bool String_extend(String *s, const char *c_str) {
    size_t len = strlen(c_str);
    if (!String_reserve(s, s->length + len)) {
        return false;
    }
    memcpy(s->buffer + s->length, c_str, (len + 1));
    s->length += len;
    return true;
}

bool String_insert(String* s, string_size_t ix, const char c) {
    if (!String_reserve(s, s->length + 1)) {
        return false;
    }
    s->length += 1;
    memmove(s->buffer + ix + 1, s->buffer + ix, (s->length - ix - 1));
    s->buffer[ix] = c;
    return true;
}

bool String_insert_count(String* s, string_size_t ix, const char* buf, string_size_t count) {
    if (!String_reserve(s, s->length + count)) {
        return false;
    }
    s->length += count;
    memmove(s->buffer + ix + count, s->buffer + ix, (s->length - ix - count) + 1);
    memmove(s->buffer + ix, buf, count);

    return true;
}

void String_pop(String *s, string_size_t count) {
    if (count > s->length) {
        s->length = 0;
        s->buffer[0] = L'\0';
    } else {
        s->length = s->length - count;
        s->buffer[s->length] = L'\0';
    }
}

void String_remove(String* s, string_size_t ix, string_size_t count) {
    if (ix < 0 || ix >= s->length) {
        return;
    }
    if (ix + count > s->length) {
        count = s->length - ix;
    }
    memmove(s->buffer + ix, s->buffer + ix + count, (s->length - ix - count));
    s->length = s->length - count;
    s->buffer[s->length] = L'\0';
}

void String_clear(String *s) {
    s->length = 0;
    s->buffer[0] = L'\0';
}

bool String_create(String *s) {
    s->length = 0;
    s->buffer = Mem_alloc(4);
    if (s->buffer == NULL) {
        s->capacity = 0;
        return false;
    }
    s->capacity = 4;
    s->buffer[0] = '\0';
    return true;
}

bool String_create_capacity(String_noinit* s, string_size_t cap) {
    if (cap > 0x7fffffff) {
        return false;
    }
    s->length = 0;
    s->capacity = 4;
    while (s->capacity < cap) {
        s->capacity *= 2;
    }
    s->buffer = Mem_alloc(s->capacity);
    if (s->buffer == NULL) {
        s->capacity = 0;
        return false;
    }
    s->buffer[0] = '\0';
    return true;
}

void String_replaceall(String* s, char from, char to) {
    for (string_size_t i = 0; i < s->length; ++i) {
        if (s->buffer[i] == from) {
            s->buffer[i] = to;
        }
    }
}

string_size_t String_count(const String* s, char c) {
    string_size_t count = 0;
    for (string_size_t i = 0; i < s->length; ++i) {
        if (s->buffer[i] == c) {
            ++count;
        }
    }
    return count;
}

bool String_startswith(const String* s, const char* str) {
    string_size_t ix = 0;
    while (str[ix]) {
        if (ix >= s->length) {
            return false;
        }
        if (s->buffer[ix] != str[ix]) {
            return false;
        }
        ++ix;
    }
    return true;
}

bool String_equals(const String* s, const String* str) {
    if (s->length != str->length) {
        return false;
    }
    return memcmp(s->buffer, str->buffer, s->length) == 0;
}


bool String_equals_str(const String* s, const char* str) {
    uint64_t len = strlen(str);
    if (s->length != len) {
        return false;
    }
    return memcmp(s->buffer, str, s->length) == 0;
}

void String_free(String *s) {
    Mem_free(s->buffer);
    s->capacity = 0;
    s->length = 0;
    s->buffer = NULL;
}

bool String_copy(String* dest, String* source) {
    dest->length = source->length;
    dest->capacity = 4;
    while (dest->capacity <= source->length) {
        dest->capacity = dest->capacity * 2;
    }
    dest->buffer = Mem_alloc(dest->capacity);
    if (dest->buffer == NULL) {
        dest->capacity = 0;
        dest->length = 0;
        return false;
    }
    memcpy(dest->buffer, source->buffer, (dest->length + 1));
    return true;
}

bool String_append_count(String* s, const char* buf, string_size_t count) {
    if (!String_reserve(s, s->length + count)) {
        return FALSE;
    }
    memcpy(s->buffer + s->length, buf, count);
    s->length += count;
    s->buffer[s->length] = L'\0';
    return TRUE;
}


bool String_reserve(String* s, size_t count) {
    if (s->capacity <= count) {
        if (count > 0x7fffffff) {
            return false;
        }
        if (s->capacity == 0) {
            return false;
        }
        size_t new_cap = s->capacity * 2;
        while (new_cap <= count) {
            new_cap *= 2;
        }
        char* buf = Mem_realloc(s->buffer, new_cap);
        if (buf == NULL) {
            return false;
        }
        s->buffer = buf;
        s->capacity = new_cap;
    }
    return true;
}

bool String_from_utf16_bytes(String *dest, const wchar_t *s, size_t count) {
    UINT code_point = 65001;
    DWORD size = WideCharToMultiByte(code_point, 0, s, count, NULL, 0, NULL, NULL);
    if (!String_reserve(dest, size)) {
        return false;
    }
    size = WideCharToMultiByte(code_point, 0, s, count, dest->buffer, size, NULL, NULL);
    if (size == 0) {
        return false;
    }
    dest->length = size;
    dest->buffer[size] = L'\0';

    return true;
}

bool String_from_utf16_str(String *dest, const wchar_t* s) {
    size_t len = wcslen(s);
    return String_from_utf16_bytes(dest, s, len);
}


// Wide

bool WString_append(WString *s, const wchar_t c) {
    if (!WString_reserve(s, s->length + 1)) {
        return false;
    }
    s->buffer[s->length] = c;
    s->length += 1;
    s->buffer[s->length] = L'\0';
    return true;
}

bool WString_extend(WString *s, const wchar_t *c_str) {
    size_t len = wcslen(c_str);
    if (!WString_reserve(s, s->length + len)) {
        return false;
    }
    memcpy(s->buffer + s->length, c_str, (len + 1) * sizeof(wchar_t));
    s->length += len;
    return true;
}

bool WString_insert(WString* s, string_size_t ix, const wchar_t c) {
    if (!WString_reserve(s, s->length + 1)) {
        return false;
    }
    s->length += 1;
    memmove(s->buffer + ix + 1, s->buffer + ix, (s->length - ix - 1) * sizeof(wchar_t));
    s->buffer[ix] = c;
    return true;
}

bool WString_insert_count(WString* s, string_size_t ix, const wchar_t* buf, string_size_t count) {
    if (!WString_reserve(s, s->length + count)) {
        return false;
    }
    s->length += count;
    memmove(s->buffer + ix + count, s->buffer + ix, (s->length - ix - count + 1) * sizeof(wchar_t));
    memmove(s->buffer + ix, buf, count * sizeof(wchar_t));

    return true;
}

void WString_pop(WString *s, string_size_t count) {
    if (count > s->length) {
        s->length = 0;
        s->buffer[0] = L'\0';
    } else {
        s->length = s->length - count;
        s->buffer[s->length] = L'\0';
    }
}

void WString_remove(WString* s, string_size_t ix, string_size_t count) {
    if (ix < 0 || ix >= s->length) {
        return;
    }
    if (ix + count > s->length) {
        count = s->length - ix;
    }
    memmove(s->buffer + ix, s->buffer + ix + count, (s->length - ix - count) * sizeof(wchar_t));
    s->length = s->length - count;
    s->buffer[s->length] = L'\0';
}

void WString_clear(WString *s) {
    s->length = 0;
    s->buffer[0] = L'\0';
}

bool WString_create(WString *s) {
    s->length = 0;
    s->buffer = Mem_alloc(4 * sizeof(wchar_t));
    if (s->buffer == NULL) {
        s->capacity = 0;
        return false;
    }
    s->capacity = 4;
    s->buffer[0] = '\0';
    return true;
}

bool WString_create_capacity(WString_noinit* s, string_size_t cap) {
    if (cap > 0x7fffffff) {
        return false;
    }
    s->length = 0;
    s->capacity = 4;
    while (s->capacity < cap) {
        s->capacity *= 2;
    }
    s->buffer = Mem_alloc(s->capacity * sizeof(wchar_t));
    if (s->buffer == NULL) {
        s->capacity = 0;
        return false;
    }
    s->buffer[0] = '\0';
    return true;
}

void WString_replaceall(WString* s, wchar_t from, wchar_t to) {
    for (string_size_t i = 0; i < s->length; ++i) {
        if (s->buffer[i] == from) {
            s->buffer[i] = to;
        }
    }
}

string_size_t WString_count(const WString* s, wchar_t c) {
    string_size_t count = 0;
    for (string_size_t i = 0; i < s->length; ++i) {
        if (s->buffer[i] == c) {
            ++count;
        }
    }
    return count;
}

bool WString_startswith(const WString* s, const wchar_t* str) {
    string_size_t ix = 0;
    while (str[ix]) {
        if (ix >= s->length) {
            return false;
        }
        if (s->buffer[ix] != str[ix]) {
            return false;
        }
        ++ix;
    }
    return true;
}

bool WString_equals(const WString* s, const WString* str) {
    if (s->length != str->length) {
        return false;
    }
    return memcmp(s->buffer, str->buffer, 
                  s->length * sizeof(wchar_t)) == 0;
}

bool WString_equals_str(const WString* s, const wchar_t* str) {
    uint64_t len = wcslen(str);
    if (s->length != len) {
        return false;
    }
    return memcmp(s->buffer, str, s->length * sizeof(wchar_t)) == 0;
}

void WString_free(WString *s) {
    Mem_free(s->buffer);
    s->capacity = 0;
    s->length = 0;
    s->buffer = NULL;
}

bool WString_copy(WString* dest, WString* source) {
    dest->length = source->length;
    dest->capacity = 4;
    while (dest->capacity <= source->length) {
        dest->capacity = dest->capacity * 2;
    }
    dest->buffer = Mem_alloc(dest->capacity * sizeof(wchar_t));
    if (dest->buffer == NULL) {
        dest->capacity = 0;
        dest->length = 0;
        return false;
    }
    memcpy(dest->buffer, source->buffer, (dest->length + 1) * sizeof(wchar_t));
    return true;
}

bool WString_append_count(WString* s, const wchar_t* buf, string_size_t count) {
    if (!WString_reserve(s, s->length + count)) {
        return FALSE;
    }
    memcpy(s->buffer + s->length, buf, count * sizeof(wchar_t));
    s->length += count;
    s->buffer[s->length] = L'\0';
    return TRUE;
}


bool WString_reserve(WString* s, size_t count) {
    if (s->capacity <= count) {
        if (count > 0x7fffffff) {
            return false;
        }
        if (s->capacity == 0) {
            return false;
        }
        size_t new_cap = s->capacity * 2;
        while (new_cap <= count) {
            new_cap *= 2;
        }
        wchar_t* buf = Mem_realloc(s->buffer, new_cap * sizeof(wchar_t));
        if (buf == NULL) {
            return false;
        }
        s->buffer = buf;
        s->capacity = new_cap;
    }
    return true;
}

bool WString_from_con_bytes(WString* dest, const char* s, size_t count, UINT code_point) {
    DWORD size = MultiByteToWideChar(code_point, 0, s, count, NULL, 0);
    if (!WString_reserve(dest, size)) {
        return false;
    }
    size = MultiByteToWideChar(code_point, 0, s, count, dest->buffer, size);
    if (size == 0) {
        return false;
    }
    dest->length = size;
    dest->buffer[size] = L'\0';

    return true;
}

bool WString_from_con_str(WString* dest, const char* s, UINT code_point) {
    size_t len = strlen(s);
    return WString_from_con_bytes(dest, s, len, code_point);
}

bool WString_from_utf8_bytes(WString* dest, const char* s, size_t count) {
    UINT code_point = 65001;
    DWORD size = MultiByteToWideChar(code_point, 0, s, count, NULL, 0);
    if (!WString_reserve(dest, size)) {
        return false;
    }
    size = MultiByteToWideChar(code_point, 0, s, count, dest->buffer, size);
    if (size == 0) {
        return false;
    }
    dest->length = size;
    dest->buffer[size] = L'\0';

    return true;
}

bool WString_from_utf8_str(WString* dest, const char* s) {
    size_t len = strlen(s);
    return WString_from_utf8_bytes(dest, s, len);
}


#ifndef DYNAMIC_STRING_NO_FMT

bool WString_format(WString* dest, const wchar_t* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int count = _vscwprintf(fmt, args);
    if (count == -1) {
        return false;
    }
    if (!WString_reserve(dest, count)) {
        return false;
    }
    _vsnwprintf(dest->buffer, count, fmt, args);
    dest->length = count;
    dest->buffer[dest->length] = L'\0';

    va_end(args);
    return true;
}

bool WString_format_append(WString* dest, const wchar_t* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int count = _vscwprintf(fmt, args);
    if (count == -1) {
        return false;
    }
    if (!WString_reserve(dest, dest->length + count)) {
        return false;
    }
    _vsnwprintf(dest->buffer + dest->length, count, fmt, args);
    dest->length = count + dest->length;
    dest->buffer[dest->length] = L'\0';

    va_end(args);
    return true;
}
#endif
