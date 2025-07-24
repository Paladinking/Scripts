#include <stdint.h>
#include <stdbool.h>
#include "parser.h"

enum TokenType {
    TOKEN_LITERAL, TOKEN_END, TOKEN_ERROR, TOKEN_KFN, TOKEN_KSTRUCT, TOKEN_KIF, TOKEN_KWHILE, TOKEN_KELSE, TOKEN_KRETURN, TOKEN_KTRUE, TOKEN_KFALSE, TOKEN_TYPEID, TOKEN_IDENTIFIER, TOKEN_INTEGER, TOKEN_REAL, TOKEN_STRING
};
typedef struct Token {
    enum TokenType id;
    union {
        const char* literal;
        int64_t error;
        name_id kFn;
        name_id kStruct;
        name_id kIf;
        name_id kWhile;
        name_id kElse;
        name_id kReturn;
        name_id kTrue;
        name_id kFalse;
        type_id typeid;
        StrWithLength identifier;
        uint64_t integer;
        double real;
        StrWithLength string;
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
