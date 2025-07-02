#include "mem.h"

#ifdef MEM_DEBUG
#include "glob.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdarg.h>
#include <stdint.h>

static uint64_t alloc_count = 0;

bool append_file(const char* str, const wchar_t* filename) {
    HANDLE file = CreateFileW(filename, FILE_APPEND_DATA,
                              0, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    unsigned len = strlen(str);
    DWORD w;
    unsigned written = 0;
    while (written < len) {
        if (!WriteFile(file, str + written, len - written, &w, NULL)) {
            CloseHandle(file);
            return false;
        }
        written += w;
    }
    CloseHandle(file);
    return true;
}


extern int _snprintf(char *buffer, size_t count, const char *format, ...);
extern int _vsnprintf(char *buffer, size_t count, const char *format,
                      va_list argptr);

void* Mem_alloc_dbg(size_t size, int lineno, const char* file) {
    void* ptr = HeapAlloc(GetProcessHeap(), 0, size);
    if (ptr != NULL) {
        alloc_count += 1;
    }
#if MEM_DEBUG > 1

    char buf[1024];
    _snprintf(buf, 1024, "Alloc %p (%llu): %s:%d\n", ptr, alloc_count, file, lineno);

    append_file(buf, L"mem_debug.txt");
#endif
    return ptr;
}

void Mem_free_dbg(void* ptr, int lineno, const char* file) {
    if (ptr != NULL) {
        alloc_count -= 1;
    }

#if MEM_DEBUG > 1
    char buf[1024];
    _snprintf(buf, 1024, "Free %p (%llu): %s:%d\n", ptr, alloc_count, file, lineno);

    append_file(buf, L"mem_debug.txt");
#endif
    HeapFree(GetProcessHeap(), 0, ptr);
}

void* Mem_realloc_dbg(void* ptr, size_t size, int lineno, const char* file) {
    void *new_ptr = HeapReAlloc(GetProcessHeap(), 0, ptr, size);
#if MEM_DEBUG > 1
    char buf[1024];
    _snprintf(buf, 1024, "Realloc %p, %p (%llu): %s:%d\n", ptr, new_ptr, alloc_count, file, lineno);

    append_file(buf, L"mem_debug.txt");
#endif
    return new_ptr;
}


uint64_t Mem_count() {
    return alloc_count;
}

void Mem_debug_dbg(int lineno, const char* file, const char* fmt, ...) {
    char fmt_buf[1024];
    va_list args;
    va_start(args, fmt);

    _vsnprintf(fmt_buf, 1024, fmt, args);
    fmt_buf[1023] = '\0';
    va_end(args);

    char buf[1024];
    
    _snprintf(buf, 1024, "Debug %s: %s:%d\n", fmt_buf, file, lineno);

    append_file(buf, L"mem_debug.txt");
}

void* Mem_xalloc_dbg(size_t size, int lineno, const char* file) {
    void* res = Mem_alloc_dbg(size, lineno, file);
    if (res == NULL) {
        ExitProcess(ERROR_OUTOFMEMORY);
    }
    return res;

}

void* Mem_xrealloc_dbg(void* ptr, size_t size, int lineno, const char* file) {
    void* res = Mem_realloc_dbg(ptr, size, lineno, file);
    if (res == NULL) {
        ExitProcess(ERROR_OUTOFMEMORY);
    }
    return res;
}

#endif

/*void* Mem_xalloc(size_t size) {
    void* ptr = Mem_alloc(size);
    if (ptr == NULL) {
        ExitProcess(ERROR_OUTOFMEMORY);
    }
    return ptr;
}

void* Mem_xrealloc(void* ptr, size_t size) {
    void* p = Mem_realloc(ptr, size);
    if (p == NULL) {
        ExitProcess(ERROR_OUTOFMEMORY);
    }
    return p;
}*/
