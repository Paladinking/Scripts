#include "tokenizer.h"
#include "tables.h"
#include "mem.h"
#include <printf.h>


void out_of_memory(void* ctx) {
    Parser* parser = ctx;
    fatal_error(parser, PARSE_ERROR_OUTOFMEMORY, LINE_INFO_NONE);
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

void fatal_error_cb(Parser* parser, enum ErrorKind error, LineInfo info,
                    const char* file, uint64_t line) {
    uint64_t row, col;
    if (info.start == LINE_INFO_NONE.start || info.end == LINE_INFO_NONE.end) {
        row = 0;
        col = 0;
    } else {
        parser_row_col(parser, info.start, &row, &col);
    }
    switch (error) {
        case PARSE_ERROR_OUTOFMEMORY:
            LOG_USER_CRITICAL("Fatal error: out of memory");
            break;
        case PARSE_ERROR_EOF:
            LOG_USER_CRITICAL("Fatal error: unexpecetd end of file at row %llu, collum %llu", row, col);
            break;
        case PARSE_ERROR_INTERNAL:
            LOG_USER_CRITICAL("Fatal error: internal error at %s line %llu", file, line);
            break;
        case PARSE_ERROR_NONE:
            break;
        case ASM_ERROR_MISSING_ENCODING:
            LOG_USER_CRITICAL("Fatal error: Missing asm encoding at %s line %llu", file, line);
            break;
        case LINKER_ERROR_DUPLICATE_SYMBOL:
            LOG_USER_CRITICAL("Fatal error: Duplicate symbol\n");
            break;
        default:
            LOG_USER_CRITICAL("Fatal error: UNKOWN");
            break;
    }
    Log_Shutdown();
    ExitProcess(1);
}

void error_cb(Parser* parser, enum ErrorKind error, LineInfo line,
              const char* file, uint64_t iline) {
    Error* e = Arena_alloc_type(&parser->arena, Error);
    e->pos = line;
    e->kind = error;
    e->next = NULL;
    e->file = file;
    e->internal_line = iline;
    if (parser->first_error == NULL) {
        parser->first_error = e;
        parser->last_error = e;
    } else {
        parser->last_error->next = e;
        parser->last_error = e;
    }
}

void Buffer_create(ByteBuffer* buf) {
    buf->data = Mem_alloc(4096);
    if (buf->data == NULL) {
        out_of_memory(NULL);
    }
    buf->size = 0;
    buf->capacity = 4096;
}

void Buffer_append(ByteBuffer* buf, uint8_t b) {
    RESERVE(buf->data, buf->size + 1, buf->capacity);
    buf->data[buf->size] = b;
    ++buf->size;
}

void Buffer_extend(ByteBuffer* buf, const uint8_t* data, uint32_t len) {
    RESERVE(buf->data, buf->size + len, buf->capacity);
    memcpy(buf->data + buf->size, data, len);
    buf->size += len;
}

uint8_t* Buffer_reserve_space(ByteBuffer* buf, uint32_t len) {
    RESERVE(buf->data, buf->size + len, buf->capacity);
    uint8_t* ptr = buf->data + buf->size;
    buf->size += len;

    return ptr;
}

void Buffer_free(ByteBuffer* buf) {
    Mem_free(buf->data);
    buf->data = NULL;
    buf->size = 0;
    buf->capacity = 0;
}

