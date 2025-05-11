#include "arena.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define ALLIGN_TO(i, size) if (((i) % (size)) != 0) { \
    (i) = (i) + ((size) - ((i) % (size)));            \
}

bool Arena_create(Arena* arena, uint64_t max_size,
        void (*alloc_failure)(void*), void* failure_ctx){
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    arena->reserve_granularity = info.dwAllocationGranularity;
    ALLIGN_TO(max_size, arena->reserve_granularity);
    uint8_t* ptr = VirtualAlloc(NULL, max_size, MEM_RESERVE, PAGE_READWRITE);
    if (ptr == NULL) {
        arena->base = NULL;
        return false;
    }
    arena->base = ptr;
    arena->offset = 0;
    arena->reserved_size = max_size;
    arena->alloc_failure = alloc_failure;
    arena->failure_ctx = failure_ctx;
    return true;
}

void* Arena_alloc(Arena* arena, size_t alloc_size, size_t allignment) {
    uint64_t offset = arena->offset;
    ALLIGN_TO(offset, allignment);
    if (offset + alloc_size > arena->reserved_size) {
        if (arena->alloc_failure != NULL) {
            arena->alloc_failure(arena->failure_ctx);
        }
        return NULL;
    }
    if (alloc_size == 0) {
        return arena->base + offset;
    }
    void* ptr = VirtualAlloc(arena->base + offset, alloc_size, MEM_COMMIT,
                             PAGE_READWRITE);
    if (ptr == NULL) {
        if (arena->alloc_failure != NULL) {
            arena->alloc_failure(arena->failure_ctx);
        }
        return NULL;
    }
    arena->offset = offset + alloc_size;
    return arena->base + offset;
}

void Arena_release(Arena* arena) {
    arena->offset = 0;
}

void Arena_free(Arena* arena) {
    VirtualFree(arena->base, arena->reserved_size, MEM_RELEASE);
    arena->base = NULL;
    arena->reserved_size = 0;
    arena->offset = 0;
}
