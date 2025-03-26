#ifndef MEM_H_00
#define MEM_H_00

#ifdef MEM_DEBUG
#include <stddef.h>
#define Mem_alloc(size) Mem_alloc_dbg((size), __LINE__, __FILE__)
#define Mem_free(ptr) Mem_free_dbg((ptr), __LINE__, __FILE__)
#define Mem_realloc(ptr, size) Mem_realloc_dbg((ptr), (size), __LINE__, __FILE__)
#define Mem_debug(msg, ...) Mem_debug_dbg(__LINE__, __FILE__, (msg), __VA_ARGS__)

void* Mem_alloc_dbg(size_t size, int lineno, const char* file);
void Mem_free_dbg(void* ptr, int lineno, const char* file);
void* Mem_realloc_dbg(void* ptr, size_t size, int lineno, const char* file);
void Mem_debug_dbg(int lineno, const char* file, const char* fmt, ...);
unsigned long long Mem_count();
#else
#include <windows.h>
#define Mem_alloc(size) HeapAlloc(GetProcessHeap(), 0, (size))
#define Mem_free(ptr) HeapFree(GetProcessHeap(), 0, (ptr))
#define Mem_realloc(ptr, size) HeapReAlloc(GetProcessHeap(), 0, (ptr), (size))
#define Mem_debug(msg, ...)
#define Mem_count() (0)
#endif


#endif // MEM_H_00
