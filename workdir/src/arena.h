#ifndef ARENA_H_00
#define ARENA_H_00
#include <stdint.h>
#include <stdbool.h>

typedef struct Arena {
    uint8_t* base;
    uint64_t offset;
    uint64_t reserved_size;
    uint64_t reserve_granularity;

    void *failure_ctx;
    void (*alloc_failure)(void*);
} Arena;


bool Arena_create(Arena* arena, size_t max_size,
                  void (*alloc_failure)(void*), void* failure_ctx);

void Arena_release(Arena* arena);

#define Arena_alloc_type(a, t) Arena_alloc(a, sizeof(t), __alignof(t))

#define Arena_alloc_count(a, t, c) Arena_alloc(a, sizeof(t) * (c), __alignof(t))

void* Arena_alloc(Arena* arena, size_t alloc_size, size_t allignment);

// Frees all memory
void Arena_free(Arena* arena);

#endif
