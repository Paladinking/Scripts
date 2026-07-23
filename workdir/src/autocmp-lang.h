#ifndef AUTOCMP_LANG_H
#define AUTOCMP_LANG_H
#include <stdint.h>
#include <stdbool.h>
#include "autocmp.h"

enum TokenType {
    TOKEN_UNKOWN,
    TOKEN_END,
    TOKEN_ERROR,
    TOKEN_STRING,
    TOKEN_NEWLINE,
    TOKEN_LITERAL_OPENPAREN,
    TOKEN_LITERAL_CLOSEPAREN,
    TOKEN_LITERAL_PIPE,
    TOKEN_LITERAL_GT,
    TOKEN_LITERAL_LT
};
typedef struct Token {
    enum TokenType id;
    union {
        const char* literal;
        char* string;
    };
} Token;

typedef struct SyntaxError {
    Token token;
    uint64_t start;
    uint64_t end;
    const Token* expected;
    uint64_t expected_count;
} SyntaxError;

Token peek_token(void* ctx, uint64_t* start, uint64_t* end);

void consume_token(void* ctx, uint64_t* start, uint64_t* end);

void parser_error(void* ctx, SyntaxError e);

Token literal_token(const char* s, uint64_t len, uint64_t* ix);

bool parse(void* ctx);

ParenList* OnParenList(void* ctx, uint64_t start, uint64_t end, char*);

Command* OnCommand(void* ctx, uint64_t start, uint64_t end, char*);

DynamicMatch* OnDynamic(void* ctx, uint64_t start, uint64_t end, char*, char*, char*, char*);

Command* OnListCommand(void* ctx, uint64_t start, uint64_t end, ParenList*);

ParenList* OnAddParenList(void* ctx, uint64_t start, uint64_t end, ParenList*, char*);

Line OnLine(void* ctx, uint64_t start, uint64_t end, Command*);

Line OnLineWithLoop(void* ctx, uint64_t start, uint64_t end, Command*, Command*);

void OnProgramLine(void* ctx, uint64_t start, uint64_t end, Line);

void OnProgramLineWithPrefix(void* ctx, uint64_t start, uint64_t end, int32_t, Line);

void OnProgramDynamic(void* ctx, uint64_t start, uint64_t end, DynamicMatch*);
#endif
