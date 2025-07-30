#ifndef COMPILER_UTILS_H_00
#define COMPILER_UTILS_H_00

#include <stdbool.h>
#include <stdint.h>
#include "log.h"

typedef struct LineInfo {
    uint64_t start;
    uint64_t end;
} LineInfo;

extern const LineInfo LINE_INFO_NONE;

#define PTR_SIZE 8

#ifdef _MSC_VER
#include <printf.h>
#define assert(e) if (!(e)) { _wprintf_e(L"Assertion failed: '%s' at %S:%u", L#e, __FILE__, __LINE__); ExitProcess(1); }
#else
#define assert(e) if (!(e)) { __debugbreak(); }
#endif

enum ErrorKind {
    PARSE_ERROR_NONE = 0,
    PARSE_ERROR_EOF = 1,
    PARSE_ERROR_OUTOFMEMORY = 2,
    PARSE_ERROR_INVALID_ESCAPE = 3,
    PARSE_ERROR_INVALID_CHAR = 4,
    PARSE_ERROR_RESERVED_NAME = 5,
    PARSE_ERROR_BAD_NAME = 6,
    PARSE_ERROR_BAD_TYPE = 7,
    PARSE_ERROR_INVALID_LITERAL = 8,
    PARSE_ERROR_REDEFINITION = 9,
    PARSE_ERROR_BAD_ARRAY_SIZE = 10,
    PARSE_ERROR_INTERNAL = 11,
    TYPE_ERROR_ILLEGAL_TYPE = 12,
    TYPE_ERROR_ILLEGAL_CAST = 13,
    TYPE_ERROR_WRONG_ARG_COUNT = 14,
    ASM_ERROR_MISSING_ENCODING = 15
};

typedef struct Error {
    enum ErrorKind kind;
    LineInfo pos;
    struct Error* next;
    const char* file;
    uint64_t internal_line;
} Error;

typedef struct Parser Parser;

void fatal_error_cb(Parser* parser, enum ErrorKind error, LineInfo info,
                    const char* file, uint64_t line);

void error_cb(Parser* parser, enum ErrorKind error, LineInfo line,
        const char* file, uint64_t iline);

#define fatal_error(p, e, l) fatal_error_cb(p, e, l, __FILE__, __LINE__)
#define add_error(p, e, l) do { LOG_INFO("Adding error %d at %llu", e, p->pos); error_cb(p, e, l, __FILE__, __LINE__); } while(0);

void out_of_memory(void* ctx);

void reserve(void** ptr, uint64_t size, size_t elem_size, uint64_t* cap);

#define RESERVE(ptr, size, cap) reserve((void**)(&(ptr)), (size), sizeof(*(ptr)), &(cap))

#define ALIGNED_TO(i, allignment) (((i) % (size) == 0) ? (i) : ((i) + ((size) - ((i) % (size)))))

#define ALIGN_TO(i, size) if (((i) % (size)) != 0) { \
    (i) = (i) + ((size) - ((i) % (size)));            \
}

#endif
