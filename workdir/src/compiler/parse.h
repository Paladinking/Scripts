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

Expression* OnTrue(void* ctx, uint64_t start, uint64_t end, name_id);

Expression* OnBinop1(void* ctx, uint64_t start, uint64_t end, Expression*, enum BinaryOperator, Expression*);

Expression* OnUnop1(void* ctx, uint64_t start, uint64_t end, enum UnaryOperator, Expression*);

Expression* OnCall(void* ctx, uint64_t start, uint64_t end, Expression*, ExpressionList*);

Expression* OnArrayIndex(void* ctx, uint64_t start, uint64_t end, Expression*, Expression*);

Expression* OnCond(void* ctx, uint64_t start, uint64_t end, Expression*);

Statement* OnWhile(void* ctx, uint64_t start, uint64_t end, name_id, Expression*, Statements);

Statement* OnIf(void* ctx, uint64_t start, uint64_t end, name_id, Expression*, Statements, Statement*);

void* OnElseHead(void* ctx, uint64_t start, uint64_t end, name_id);

Statements OnStatements(void* ctx, uint64_t start, uint64_t end, StatementList*);

FunctionDef* OnFunction(void* ctx, uint64_t start, uint64_t end, name_id, ArgList*, type_id, Statements);

name_id OnFnHead(void* ctx, uint64_t start, uint64_t end, name_id, StrWithLength);

ExpressionList* OnParamList(void* ctx, uint64_t start, uint64_t end, Expression*);

ArgList* OnArgList(void* ctx, uint64_t start, uint64_t end, type_id, StrWithLength);

ArgList* OnAddArgList(void* ctx, uint64_t start, uint64_t end, ArgList*, type_id, StrWithLength);

StatementList* OnStatementList(void* ctx, uint64_t start, uint64_t end, StatementList*, Statement*);

Statement* OnDecl(void* ctx, uint64_t start, uint64_t end, type_id, ArraySizes*, StrWithLength, Expression*);

Statement* OnAssign(void* ctx, uint64_t start, uint64_t end, Expression*, Expression*);

Statement* OnExprStatement(void* ctx, uint64_t start, uint64_t end, Expression*);

Statement* OnReturn(void* ctx, uint64_t start, uint64_t end, name_id, Expression*);

ArraySizes* OnArrayDecl(void* ctx, uint64_t start, uint64_t end, ArraySizes*, uint64_t);

Statement* OnElse(void* ctx, uint64_t start, uint64_t end, void*, Statements);

Expression* OnFalse(void* ctx, uint64_t start, uint64_t end, name_id);

Expression* OnIdentExpr(void* ctx, uint64_t start, uint64_t end, StrWithLength);

Expression* OnInt(void* ctx, uint64_t start, uint64_t end, uint64_t);

Expression* OnReal(void* ctx, uint64_t start, uint64_t end, double);

Expression* OnString(void* ctx, uint64_t start, uint64_t end, StrWithLength);

Expression* OnParen(void* ctx, uint64_t start, uint64_t end, Expression*);

Expression* OnBinop2(void* ctx, uint64_t start, uint64_t end, Expression*, enum BinaryOperator, Expression*);

Expression* OnUnop2(void* ctx, uint64_t start, uint64_t end, enum UnaryOperator, Expression*);

ExpressionList* OnAddParamList(void* ctx, uint64_t start, uint64_t end, ExpressionList*, Expression*);
