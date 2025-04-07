#ifndef REGEX_H_00
#define REGEX_H_00
#include <stdint.h>
#include <stdbool.h>

typedef struct EdgeNFA EdgeNFA;
typedef struct NodeNFA NodeNFA;


typedef struct NFA {
    uint64_t node_count;
    NodeNFA *nodes;
    EdgeNFA *edges;
} NFA;

typedef struct Regex {
    char* chars;
    NFA nfa;
} Regex;

typedef enum RegexResult {
    REGEX_ERROR = -1,
    REGEX_NO_MATCH = 0,
    REGEX_MATCH = 1
} RegexResult;

Regex* Regex_compile(const char* pattern);

void Regex_free(Regex* regex);

RegexResult Regex_fullmatch(Regex* regex, const char* str, uint64_t len);

#endif
