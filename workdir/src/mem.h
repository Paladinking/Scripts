#ifndef MEM_H_00
#define MEM_H_00

#include <windows.h>
#ifdef MEM_DEBUG
#include <stddef.h>

#define Mem_free(ptr) Mem_free_dbg((ptr), __LINE__, __FILE__)

#ifdef MEM_ABORT_ERROR
#define Mem_alloc(size) Mem_xalloc_dbg((size), __LINE__, __FILE__)
#define Mem_realloc(ptr, size) Mem_xrealloc_dbg((ptr), (size), __LINE__, __FILE__)
#else
#define Mem_alloc(size) Mem_alloc_dbg((size), __LINE__, __FILE__)
#define Mem_realloc(ptr, size) Mem_realloc_dbg((ptr), (size), __LINE__, __FILE__)
#endif

#define Mem_debug(...) Mem_debug_dbg(__LINE__, __FILE__, __VA_ARGS__)

#define Mem_salloc(size) Mem_alloc_dbg((size), __LINE__, __FILE__)
#define Mem_srealloc(ptr, size) Mem_realloc_dbg((ptr), (size), __LINE__, __FILE__)
#define Mem_xalloc(size) Mem_xalloc_dbg((size), __LINE__, __FILE__, __VA_ARGS__)
#define Mem_xrealloc(ptr, size) Mem_xrealloc_dbg((ptr), (size), __LINE__, __FILE__, __VA_ARGS__)

void* Mem_alloc_dbg(size_t size, int lineno, const char* file);
void Mem_free_dbg(void* ptr, int lineno, const char* file);
void* Mem_realloc_dbg(void* ptr, size_t size, int lineno, const char* file);
void* Mem_xalloc_dbg(size_t size, int lineno, const char* file);
void* Mem_xrealloc_dbg(void* ptr, size_t size, int lineno, const char* file);
void Mem_debug_dbg(int lineno, const char* file, const char* fmt, ...);
unsigned long long Mem_count();
#else

#define Mem_free(ptr) HeapFree(GetProcessHeap(), 0, (ptr))

#ifdef MEM_ABORT_ERROR
#define Mem_alloc(size) HeapAlloc(GetProcessHeap(), HEAP_GENERATE_EXCEPTIONS, (size))
#define Mem_realloc(ptr, size) HeapReAlloc(GetProcessHeap(), HEAP_GENERATE_EXCEPTIONS, (ptr), (size))
#else
#define Mem_alloc(size) HeapAlloc(GetProcessHeap(), 0, (size))
#define Mem_realloc(ptr, size) HeapReAlloc(GetProcessHeap(), 0, (ptr), (size))
#endif

#define Mem_debug(...)
#define Mem_count() (0)

#define Mem_salloc(size) HeapAlloc(GetProcessHeap(), 0, (size))
#define Mem_srealloc(ptr, size) HeapReAlloc(GetProcessHeap(), 0, (ptr), (size))

#define Mem_xalloc(size) HeapAlloc(GetProcessHeap(), HEAP_GENERATE_EXCEPTIONS, (size))
#define Mem_xrealloc(ptr, size) HeapReAlloc(GetProcessHeap(), HEAP_GENERATE_EXCEPTIONS, (ptr), (size))
#endif

#define MEM_ALIGNMENT MEMORY_ALLOCATION_ALIGNMENT

#endif // MEM_H_00
