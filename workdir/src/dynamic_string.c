#include "dynamic_string.h"
#include <windows.h>

bool String_append(String *s, const char c) {
    if (s->capacity == s->length + 1) {
        HANDLE heap = GetProcessHeap();
        unsigned capacity = s->capacity * 2;
        char *buffer = HeapReAlloc(heap, 0, s->buffer, capacity);
        if (buffer == NULL) {
            return false;
        }
        s->buffer = buffer;
        s->capacity = capacity;
    }
    s->buffer[s->length] = c;
    s->length += 1;
    s->buffer[s->length] = '\0';
    return true;
}

bool String_extend(String *s, const char *c_str) {
    unsigned index = 0;
    while (c_str[index] != '\0') {
        if (!String_append(s, c_str[index])) {
            return false;
        }
        ++index;
    }
    return true;
}

bool String_insert(String* s, unsigned ix, const char c) {
    if (!String_append(s, c)) {
        return false;
    }
    memmove(s->buffer + ix + 1, s->buffer + ix, s->length - ix - 1);
    s->buffer[ix] = c;
    return true;
}

void String_pop(String *s, unsigned int count) {
    if (count > s->length) {
        s->length = 0;
        s->buffer[0] = '\0';
    } else {
        s->length = s->length - count;
        s->buffer[s->length] = '\0';
    }
}

void String_remove(String* s, unsigned ix, unsigned count) {
    if (ix < 0 || ix >= s->length) {
        return;
    }
    if (ix + count > s->length) {
        count = s->length - ix;
    }
    memmove(s->buffer + ix, s->buffer + ix + count, s->length - ix - count);
    s->length = s->length - count;
    s->buffer[s->length] = '\0';
}

void String_clear(String *s) {
    s->length = 0;
    s->buffer[0] = '\0';
}

bool String_create(String *s) {
    HANDLE heap = GetProcessHeap();
    s->length = 0;
    s->buffer = HeapAlloc(heap, 0, 4);
    if (s->buffer == NULL) {
        s->capacity = 0;
        return false;
    }
    s->capacity = 4;
    s->buffer[0] = '\0';
    return true;
}

void String_free(String *s) {
    HANDLE heap = GetProcessHeap();
    HeapFree(heap, 0, s->buffer);
    s->capacity = 0;
    s->length = 0;
    s->buffer = NULL;
}

bool String_copy(String* dest, String* source) {
    HANDLE heap = GetProcessHeap();
    dest->length = source->length;
    dest->capacity = 0;
    while (dest->capacity <= source->length) {
        dest->capacity = dest->capacity * 2;
    }
    dest->buffer = HeapAlloc(heap, 0, dest->capacity);
    if (dest->buffer == NULL) {
        dest->capacity = 0;
        dest->length = 0;
        return false;
    }
    memcpy(dest->buffer, source->buffer, dest->length + 1);
    return true;
}

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

bool WString_insert(WString* s, unsigned ix, const wchar_t c) {
    if (!WString_reserve(s, s->length + 1)) {
        return false;
    }
    s->length += 1;
    memmove(s->buffer + ix + 1, s->buffer + ix, (s->length - ix - 1) * sizeof(wchar_t));
    s->buffer[ix] = c;
    return true;
}

bool WString_insert_count(WString* s, unsigned ix, const wchar_t* buf, unsigned count) {
    if (!WString_reserve(s, s->length + count)) {
        return false;
    }
    s->length += count;
    memmove(s->buffer + ix + count, s->buffer + ix, (s->length - ix - count) * sizeof(wchar_t));
    memmove(s->buffer + ix, buf, count);

    return true;
}

void WString_pop(WString *s, unsigned int count) {
    if (count > s->length) {
        s->length = 0;
        s->buffer[0] = L'\0';
    } else {
        s->length = s->length - count;
        s->buffer[s->length] = L'\0';
    }
}

void WString_remove(WString* s, unsigned ix, unsigned count) {
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
    HANDLE heap = GetProcessHeap();
    s->length = 0;
    s->buffer = HeapAlloc(heap, 0, 4 * sizeof(wchar_t));
    if (s->buffer == NULL) {
        s->capacity = 0;
        return false;
    }
    s->capacity = 4;
    s->buffer[0] = '\0';
    return true;
}

bool WString_create_capacity(WString_noinit* s, size_t cap) {
    HANDLE heap = GetProcessHeap();
    s->length = 0;
    s->capacity = 4;
    while (s->capacity < cap) {
        s->capacity *= 2;
    }
    s->buffer = HeapAlloc(heap, 0, s->capacity * sizeof(wchar_t));
    if (s->buffer == NULL) {
        s->capacity = 0;
        return false;
    }
    s->buffer[0] = '\0';
    return true;
}

void WString_free(WString *s) {
    HANDLE heap = GetProcessHeap();
    HeapFree(heap, 0, s->buffer);
    s->capacity = 0;
    s->length = 0;
    s->buffer = NULL;
}

bool WString_copy(WString* dest, WString* source) {
    HANDLE heap = GetProcessHeap();
    dest->length = source->length;
    dest->capacity = 4;
    while (dest->capacity <= source->length) {
        dest->capacity = dest->capacity * 2;
    }
    dest->buffer = HeapAlloc(heap, 0, dest->capacity * sizeof(wchar_t));
    if (dest->buffer == NULL) {
        dest->capacity = 0;
        dest->length = 0;
        return false;
    }
    memcpy(dest->buffer, source->buffer, (dest->length + 1) * sizeof(wchar_t));
    return true;
}

bool WString_append_count(WString* s, const wchar_t* buf, unsigned count) {
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
        size_t new_cap = s->capacity * 2;
        while (new_cap <= count) {
            new_cap *= 2;
        }
        wchar_t* buf = HeapReAlloc(GetProcessHeap(), 0, s->buffer, new_cap * sizeof(wchar_t));
        if (buf == NULL) {
            return false;
        }
        s->buffer = buf;
        s->capacity = new_cap;
    }
    return true;
}

bool WString_from_utf8_bytes(WString* dest, const char* s, size_t count) {
    UINT code_point = 65001;
    DWORD size = MultiByteToWideChar(code_point, 0, s, count, NULL, 0);
    if (!WString_reserve(dest, count)) {
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
