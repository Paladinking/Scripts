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
} Regex;

typedef enum RegexResult {
    REGEX_ERROR = -1,
    REGEX_NO_MATCH = 0,
    REGEX_MATCH = 1
} RegexResult;

struct Node {
    uint64_t str_ix;
    uint32_t node_ix;
};

struct RegexMatchCtx {
    struct Node* to_visit;
    uint64_t to_visit_cap;
    uint8_t* visited;
    uint64_t visited_cap;
};

extern uint64_t edge_time;

Regex* Regex_compile(const char* pattern);

void Regex_free(Regex* regex);

RegexResult Regex_fullmatch_dfa(Regex* regex, const char* str, uint64_t len);

RegexResult Regex_fullmatch(Regex* regex, const char* str, uint64_t len);

RegexResult Regex_anymatch_dfa(Regex* regex, const char* str, uint64_t len);

RegexResult Regex_anymatch(Regex* regex, const char* str, uint64_t len, struct RegexMatchCtx* ctx);

#endif
