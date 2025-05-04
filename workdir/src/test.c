#define WIN32_LEAN_AND_MEAN
#define _NO_CRT_STDIO_INLINE
#include <windows.h>
#include "args.h"
#include "regex.h"
#include "glob.h"
#include "printf.h"

#define BUFFER_SIZE 4096

static inline char* last_newline(char* ptr, uint64_t len) {
    while (len > 0) {
        --len;
        if (ptr[len] == '\n' || ptr[len] == '\r') {
            return ptr + len;
        }
    }
    return NULL;
}

bool readlines(wchar_t* filename) {
    HANDLE file = CreateFileW(filename, GENERIC_READ, FILE_SHARE_READ, NULL,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | 
                              FILE_FLAG_OVERLAPPED, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }
    String b1, b2;
    if (!String_create_capacity(&b1, BUFFER_SIZE + 50)) {
        CloseHandle(file);
        return false;
    }
    if (!String_create_capacity(&b2, BUFFER_SIZE + 50)) {
        String_free(&b1);
        CloseHandle(file);
        return false;
    }
    bool status = false;
    DWORD read = 0;
    OVERLAPPED o;
    o.Offset = 0;
    o.OffsetHigh = 0;
    o.hEvent = CreateEventW(NULL, FALSE, FALSE, NULL);
    if (o.hEvent == NULL) {
        CloseHandle(file);
        String_free(&b1);
        String_free(&b2);
        return false;
    }
    if (!ReadFile(file, b1.buffer, BUFFER_SIZE, &read, &o) &&
        GetLastError() != ERROR_IO_PENDING) {
        goto end;
    }
    if (!GetOverlappedResultEx(file, &o, &read, INFINITE, FALSE)) {
        if (GetLastError() == ERROR_HANDLE_EOF) {
            status = true;
        }
        goto end;
    }
    b1.length = read;
    while (1) {
        char* newline = last_newline(b1.buffer, b1.length);
    }


end:
    CloseHandle(file);
    String_free(&b1);
    String_free(&b2);
    CloseHandle(o.hEvent);
    return status;
}

int main() {
    String s;
    String_create_capacity(&s, 2 * BUFFER_SIZE + 50);
    HANDLE file = CreateFileW(L"words3.txt", GENERIC_READ,
                              FILE_SHARE_READ, NULL,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL |
                              FILE_FLAG_OVERLAPPED, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        _printf("Failed opening file\n");
        return 1;
    }
    OVERLAPPED o;
    DWORD read = 0;
    o.Offset = 0;
    o.OffsetHigh = 0;
    o.hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (!o.hEvent) {
        _printf("Failed creating event\n");
        return 1;
    }
    if (!ReadFile(file, s.buffer, BUFFER_SIZE, &read, &o)) {
        if (GetLastError() != ERROR_IO_PENDING) {
            CloseHandle(o.hEvent);
            return 1;
        }
    } 
    if (!GetOverlappedResultEx(file, &o, &read, INFINITE, FALSE)) {
        CloseHandle(o.hEvent);
        return 1;
    }
    s.length = read;

    while (1) {
        if (s.capacity < s.length + BUFFER_SIZE) {
            if (!String_reserve(&s, s.length + BUFFER_SIZE)) {
                _printf("Out of memory\n");
                return 1;
            }
            
        }
    }

    //Regex* reg = Regex_compile("point");
    Regex* reg = Regex_compile("[0-9]\\{1,\\}.point*");
    RegexAllCtx allctx;
    LARGE_INTEGER start, end, freq, regtime;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&start);
    regtime.QuadPart = 0;

    uint64_t match_count;
    Regex_allmatch_init(reg, s.buffer, s.length, &allctx);
    const char* match;
    uint64_t len;
    while (Regex_allmatch_dfa(&allctx, &match, &len) == REGEX_MATCH) {
        _printf("[%*.*s]\n", len, len, match);
    }

    QueryPerformanceCounter(&end);
    _printf("Total time: %llu\n", (end.QuadPart - start.QuadPart) * 1000 / freq.QuadPart);

    QueryPerformanceCounter(&start);

    uint64_t ix = 0;
    uint64_t count = 0;
    struct RegexMatchCtx ctx;
    ctx.visited = NULL;
    ctx.to_visit = NULL;
    while (ix < s.length) {
        LARGE_INTEGER l1, l2;
        //QueryPerformanceCounter(&l1);
        char * start = s.buffer + ix;
        char* line_end = strchr(start, '\n');
        char* line_end2 = NULL; // strchr(start, '\r');
        uint64_t next_ix;
        if (line_end == NULL || line_end2 > line_end) {
            if (line_end2 == NULL) {
                next_ix = s.length;
            } else {
                next_ix = line_end2 - s.buffer + 1;
            }
        } else {
            next_ix = line_end - s.buffer + 1;
        }
        if (line_end2 != NULL && line_end > line_end2) {
            line_end = line_end2;
        }
        if (line_end == NULL) {
            line_end = s.buffer + s.length;
        }
        uint64_t len = line_end - start;
        QueryPerformanceCounter(&l1);
        if (Regex_anymatch_dfa(reg, start, len) == REGEX_MATCH) {
            outputa(start, next_ix - ix);
        }
        QueryPerformanceCounter(&l2);
        regtime.QuadPart += l2.QuadPart - l1.QuadPart;
        
        //++count;
        ix = next_ix;
        //if (count % 1000 == 0) {
        //    _printf("Passed: %llu\n", (ts.QuadPart) * 1000 / freq.QuadPart);
        //    ts.QuadPart = 0;
        //}
    }

    QueryPerformanceCounter(&end);
    _printf("Total time: %llu\n", (end.QuadPart - start.QuadPart) * 1000 / freq.QuadPart);
    _printf("Regex time: %llu\n", (regtime.QuadPart) * 1000 / freq.QuadPart);

    return 0;
/*
    String s;

    WString buf;
    WString_create_capacity(&buf, 4096 * 2);
    String_create(&s);

    _printf("Enter regex: ");
    DWORD read;
    ReadConsoleW(GetStdHandle(STD_INPUT_HANDLE), buf.buffer, 4096 * 2, &read, NULL);
    String_from_utf16_bytes(&s, buf.buffer, read);
    while (s.length > 0 && (s.buffer[s.length - 1] == '\n' || s.buffer[s.length - 1] == '\r')) {
        s.length -= 1;
        s.buffer[s.length] = '\0';
    }
    Regex* e = Regex_compile(s.buffer);
    if (e == NULL) {
        _printf("Failed parsing regex\n");
        return 1;
    }
    _printf("Compiled regex: '%s'\n", s.buffer);

    struct RegexMatchCtx ctx;
    ctx.visited = NULL;
    ctx.to_visit = NULL;
    while (1) {
        ReadConsoleW(GetStdHandle(STD_INPUT_HANDLE), buf.buffer, 4096, &read, NULL);
        String_from_utf16_bytes(&s, buf.buffer, read);
        while (s.length > 0 && (s.buffer[s.length - 1] == '\n' || s.buffer[s.length - 1] == '\r')) {
            s.length -= 1;
            s.buffer[s.length] = '\0';
        }
        if (strcmp(s.buffer, "exit") == 0) {
            break;
        }
        LARGE_INTEGER i1, i2;
        LARGE_INTEGER freq;
        QueryPerformanceFrequency(&freq);
        QueryPerformanceCounter(&i1);
        RegexResult res;
        for(uint32_t i = 0; i < 1000000; ++i) {
            res = Regex_fullmatch(e, s.buffer, s.length);
        }
        QueryPerformanceCounter(&i2);
        _printf("Full NFA %d, Total time: %llu\n", res, (i2.QuadPart - i1.QuadPart) * 1000 / freq.QuadPart);

        QueryPerformanceCounter(&i1);
        for(uint32_t i = 0; i < 1000000; ++i) {
            res = Regex_fullmatch_dfa(e, s.buffer, s.length);
        }
        QueryPerformanceCounter(&i2);
        _printf("Full DFA %d, time: %llu\n", res, (i2.QuadPart - i1.QuadPart) * 1000 / freq.QuadPart);

        QueryPerformanceCounter(&i1);
        for(uint32_t i = 0; i < 1000000; ++i) {
            res = Regex_anymatch(e, s.buffer, s.length, &ctx);
        }
        QueryPerformanceCounter(&i2);
        _printf("Any NFA %d, time: %llu\n", res, (i2.QuadPart - i1.QuadPart) * 1000 / freq.QuadPart);

        QueryPerformanceCounter(&i1);
        for(uint32_t i = 0; i < 1000000; ++i) {
            res = Regex_anymatch_dfa(e, s.buffer, s.length);
        }
        QueryPerformanceCounter(&i2);
        _printf("Any DFA %d, time: %llu\n", res, (i2.QuadPart - i1.QuadPart) * 1000 / freq.QuadPart);

        //_printf("'%s' nfa match: %u\n", s.buffer, Regex_fullmatch_dfa(e, s.buffer, s.length));
        //_printf("'%s' anymatch: %u\n", s.buffer, Regex_anymatch(e, s.buffer, s.length));
    }*/
    return 0;
}
