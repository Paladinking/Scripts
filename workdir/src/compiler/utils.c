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
        case PARSE_ERROR_NONE:
            _wprintf_e(L"Fatal error: UNKOWN\n");
            break;
        case PARSE_ERROR_OUTOFMEMORY:
            _wprintf_e(L"Fatal error: out of memory\n");
            break;
        case PARSE_ERROR_EOF:
            _wprintf_e(L"Fatal error: unexpecetd end of file at row %llu, collum %llu\n", row, col);
            break;
        case PARSE_ERROR_INTERNAL:
            _wprintf_e(L"Fatal error: internal error at %S line %llu\n", file, line);
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

