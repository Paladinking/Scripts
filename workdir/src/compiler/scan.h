#include <stdint.h>
#include <stdbool.h>
#include "parser.h"

enum TokenType {
    TOKEN_LITERAL, TOKEN_END, TOKEN_ERROR, TOKEN_IDENTIFIER, TOKEN_STATEMENTS
};
typedef struct Token {
    enum TokenType id;
    union {
        const char* literal;
        int64_t error;
        StrWithLength identifier;
        int64_t statements;
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

bool parse(void* ctx);
