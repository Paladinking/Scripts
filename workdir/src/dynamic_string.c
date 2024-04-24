#include "dynamic_string.h"
#include <windows.h>

bool DynamicStringAppend(DynamicString *s, const char c) {
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

bool DynamicStringExtend(DynamicString *s, const char *c_str) {
    unsigned index = 0;
    while (c_str[index] != '\0') {
        if (!DynamicStringAppend(s, c_str[index])) {
            return false;
        }
        ++index;
    }
    return true;
}

bool DynamicStringInsert(DynamicString* s, unsigned ix, const char c) {
    if (!DynamicStringAppend(s, c)) {
        return false;
    }
    memmove(s->buffer + ix + 1, s->buffer + ix, s->length - ix - 1);
    s->buffer[ix] = c;
    return true;
}

void DynamicStringPop(DynamicString *s, unsigned int count) {
    if (count > s->length) {
        s->length = 0;
        s->buffer[0] = '\0';
    } else {
        s->length = s->length - count;
        s->buffer[s->length] = '\0';
    }
}

void DynamicStringRemove(DynamicString* s, unsigned ix, unsigned count) {
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

void DynamicStringClear(DynamicString *s) {
    s->length = 0;
    s->buffer[0] = '\0';
}

bool DynamicStringCreate(DynamicString *s) {
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

void DynamicStringFree(DynamicString *s) {
    HANDLE heap = GetProcessHeap();
    HeapFree(heap, 0, s->buffer);
    s->capacity = 0;
    s->length = 0;
    s->buffer = NULL;
}

bool DynamicStringCopy(DynamicString* dest, DynamicString* source) {
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

bool DynamicWStringAppend(DynamicWString *s, const wchar_t c) {
    if (!DynamicWStringReserve(s, s->length + 1)) {
        return false;
    }
    s->buffer[s->length] = c;
    s->length += 1;
    s->buffer[s->length] = L'\0';
    return true;
}

bool DynamicWStringExtend(DynamicWString *s, const wchar_t *c_str) {
    size_t len = wcslen(c_str);
    if (!DynamicWStringReserve(s, s->length + len)) {
        return false;
    }
    memcpy(s->buffer + s->length, c_str, (len + 1) * sizeof(wchar_t));
    s->length += len;
    return true;
}

bool DynamicWStringInsert(DynamicWString* s, unsigned ix, const wchar_t c) {
    if (!DynamicWStringReserve(s, s->length + 1)) {
        return false;
    }
    s->length += 1;
    memmove(s->buffer + ix + 1, s->buffer + ix, (s->length - ix - 1) * sizeof(wchar_t));
    s->buffer[ix] = c;
    return true;
}

void DynamicWStringPop(DynamicWString *s, unsigned int count) {
    if (count > s->length) {
        s->length = 0;
        s->buffer[0] = L'\0';
    } else {
        s->length = s->length - count;
        s->buffer[s->length] = L'\0';
    }
}

void DynamicWStringRemove(DynamicWString* s, unsigned ix, unsigned count) {
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

void DynamicWStringClear(DynamicWString *s) {
    s->length = 0;
    s->buffer[0] = L'\0';
}

bool DynamicWStringCreate(DynamicWString *s) {
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

void DynamicWStringFree(DynamicWString *s) {
    HANDLE heap = GetProcessHeap();
    HeapFree(heap, 0, s->buffer);
    s->capacity = 0;
    s->length = 0;
    s->buffer = NULL;
}

bool DynamicWStringCopy(DynamicWString* dest, DynamicWString* source) {
    HANDLE heap = GetProcessHeap();
    dest->length = source->length;
    dest->capacity = 0;
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

bool DynamicWStringAppendCount(DynamicWString* s, const wchar_t* buf, unsigned count) {
    if (!DynamicWStringReserve(s, s->length + count)) {
        return FALSE;
    }
    memcpy(s->buffer + s->length, buf, count * sizeof(wchar_t));
    s->length += count;
    s->buffer[s->length] = L'\0';
    return TRUE;
}


bool DynamicWStringReserve(DynamicWString* s, size_t count) {
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

