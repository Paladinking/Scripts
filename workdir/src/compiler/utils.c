#include "parser.h"
#include "mem.h"


void out_of_memory(void* ctx) {
    Parser* parser = ctx;
    LineInfo i;
    i.real = false;
    fatal_error(parser, PARSE_ERROR_OUTOFMEMORY, i);
}

void reserve(void** ptr, uint64_t size, size_t elem_size, uint64_t* cap) {
    if (*cap >= size) {
        return;
    }
    while (size > *cap) {
        (*cap) *= 2;
    }
    void* n = Mem_realloc(*ptr, *cap * elem_size);
    if (n == NULL) {
        out_of_memory(NULL);
    }
    *ptr = n;
}
