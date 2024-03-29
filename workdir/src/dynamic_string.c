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


bool DynamicWStringAppend(DynamicWString *s, const wchar_t c) {
    if (s->capacity == s->length + 1) {
        HANDLE heap = GetProcessHeap();
        unsigned capacity = s->capacity * 2;
        wchar_t *buffer = HeapReAlloc(heap, 0, s->buffer, capacity * sizeof(wchar_t));
        if (buffer == NULL) {
            return false;
        }
        s->buffer = buffer;
        s->capacity = capacity;
    }
    s->buffer[s->length] = c;
    s->length += 1;
    s->buffer[s->length] = L'\0';
    return true;
}

bool DynamicWStringExtend(DynamicWString *s, const wchar_t *c_str) {
    unsigned index = 0;
    while (c_str[index] != L'\0') {
        if (!DynamicWStringAppend(s, c_str[index])) {
            return false;
        }
        ++index;
    }
    return true;
}

bool DynamicWStringInsert(DynamicWString* s, unsigned ix, const char c) {
    if (!DynamicWStringAppend(s, c)) {
        return false;
    }
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

