#ifndef REGEX_H_00
#define REGEX_H_00
#include <stdint.h>
#include <stdbool.h>

typedef struct EdgeNFA EdgeNFA;
typedef struct NodeNFA NodeNFA;
typedef struct NodeDFA NodeDFA;

typedef struct NFA {
    uint32_t node_count;
    uint32_t node_cap;
    uint32_t edge_count;
    uint32_t edge_cap;
    NodeNFA *nodes;
    EdgeNFA *edges;
} NFA;

typedef struct Regex {
    char* chars;
    NFA nfa;
    NodeDFA* dfa;
    uint32_t dfa_nodes;
    uint32_t minlen;
} Regex;

typedef enum RegexResult {
    REGEX_ERROR = -1,
    REGEX_NO_MATCH = 0,
    REGEX_MATCH = 1
} RegexResult;

typedef struct RegexMatch {
    uint64_t ix;
    uint64_t size;
} RegexMatch;

typedef struct RegexAllCtx {
    uint64_t start;
    Regex* regex;
    const uint8_t* str;
    uint64_t len;

    // Fields used for case insensitive match
    uint64_t buffer_len;
    uint64_t buffer_cap;
    uint8_t* buffer;
} RegexAllCtx;

Regex* Regex_compile(const char* pattern);

Regex* Regex_compile_with(const char* pattern, bool casefold);

void Regex_free(Regex* regex);

RegexResult Regex_fullmatch(Regex* regex, const char* str, uint64_t len);

RegexResult Regex_anymatch(Regex* regex, const char* str, uint64_t len);

void Regex_allmatch_init(Regex* regex, const char* str, uint64_t len, RegexAllCtx* ctx);
// Set ctx->buffer to Mem_alloc:d buffer before calling, and set ctx->buffer_cap.
void Regex_allmatch_init_nocase(Regex* regex, const char* str, uint64_t len, RegexAllCtx* ctx);

RegexResult Regex_allmatch(RegexAllCtx* ctx, const char** match, uint64_t* len);

RegexResult Regex_allmatch_nocase(RegexAllCtx* ctx, const char** match, uint64_t* len);

#endif
