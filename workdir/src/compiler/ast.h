#ifndef COMPILER_AST_H_00
#define COMPILER_AST_H_00

#include "tables.h"
#include "utils.h"

enum ExpressionKind {
    EXPRESSION_VARIABLE,
    EXPRESSION_BINOP, 
    EXPRESSION_UNOP, 
    EXPRESSION_LITERAL_INT,
    EXPRESSION_LITERAL_UINT,
    EXPRESSION_LITERAL_FLOAT,
    EXPRESSION_STRING,
    EXPRESSION_LITERAL_BOOL,
    EXPRESSION_CALL,
    EXPRESSION_ARRAY_INDEX,
    EXPRESSION_CAST,
    EXPRESSION_INVALID
};

enum BinaryOperator {
    BINOP_DIV, BINOP_MUL, BINOP_MOD, // 0
    BINOP_SUB, BINOP_ADD, // 1
    BINOP_BIT_LSHIFT, BINOP_BIT_RSHIFT, // 2
    BINOP_CMP_LE, BINOP_CMP_GE, BINOP_CMP_G, BINOP_CMP_L, // 3
    BINOP_CMP_EQ, BINOP_CMP_NEQ, // 4
    BINOP_BIT_AND, // 5
    BINOP_BIT_XOR, // 6
    BINOP_BIT_OR, // 7
    BINOP_BOOL_AND, // 8
    BINOP_BOOL_OR, // 9
};

enum UnaryOperator {
    UNOP_BITNOT, UNOP_BOOLNOT, UNOP_NEGATIVE, UNOP_POSITIVE, UNOP_PAREN,
    UNOP_ADDROF, UNOP_DEREF
};

typedef struct Expression Expression;
typedef struct Statement Statement;

typedef struct Arg {
    name_id name;
    LineInfo line;
} Arg;

typedef struct ArgList {
    Arg arg;
    struct ArgList* next;
} ArgList;

typedef struct ArraySizes {
    uint64_t size;
    struct ArraySizes* next;
} ArraySizes;

typedef struct VariableExpr {
    name_id ix;
} VariableExpr;

typedef struct BinaryOperation {
    enum BinaryOperator op;
    Expression* lhs;
    Expression* rhs;
} BinaryOperation;

typedef struct UnaryOperation {
    enum UnaryOperator op;
    Expression* expr;
} UnaryOperation;

typedef struct ExpressionList {
    Expression* expr;
    struct ExpressionList* next;
} ExpressionList;

typedef struct CallExpr {
    Expression* function;
    uint64_t arg_count;
    Expression** args;
} CallExpr;

typedef struct ArrayIndexExpr {
    Expression* array;
    Expression* index;
    // set for expressions like &array[2]
    // In that case only the address is needed
    bool get_addr;
} ArrayIndexExpr;

typedef struct LiteralExpr {
    union {
        uint64_t uint; // Also used by bool
        int64_t iint;
        double float64;
    };
} LiteralExpr;

typedef struct StringExpr {
    StringLiteral str;
} StringExpr;

typedef struct CastExpr {
    type_id type;
    Expression* e;
} CastExpr;

struct Expression {
    enum ExpressionKind kind;
    union {
        type_id type; // set by typechecker
        var_id var; // set by quads
    };
    LineInfo line;
    union {
        VariableExpr variable;
        BinaryOperation binop;
        UnaryOperation unop;
        LiteralExpr literal;
        StringExpr string;
        CallExpr call;
        ArrayIndexExpr array_index;
        CastExpr cast;
    };
};

enum StatementKind {
    STATEMENT_ASSIGN, STATEMENT_EXPRESSION,
    STATEMENT_IF, STATEMENT_WHILE, STATEMENT_RETURN,
    STATEMET_INVALID
};

struct Statement {
    enum StatementKind type;
    union {
        struct {
            Expression* lhs;
            Expression* rhs;
        } assignment;
        Expression* expression;
        struct {
            Expression* condition;
            uint64_t statement_count;
            Statement** statements;
            Statement* else_branch;
        } if_;
        struct {
            Expression* condition;
            uint64_t statement_count;
            Statement** statements;
        } while_;
        struct {
            Expression* return_value;
        } return_;
    };
    LineInfo line;
};

typedef struct StatementList {
    Statement* statement;
    struct StatementList* next;
} StatementList; 

typedef struct Statements {
    Statement** statements;
    uint64_t count;
} Statements;

typedef struct CallArg {
    name_id name;
    type_id type;
    LineInfo line;
} CallArg;

struct Quad;

typedef struct FunctionDef {
    name_id name;
    type_id return_type;
    uint64_t arg_count;
    CallArg* args;
    uint64_t statement_count;
    Statement** statements;
    LineInfo line;

    // First quad of function
    struct Quad* quad_start;
    // Last quad of function (inclusive)
    struct Quad* quad_end;

    VarList vars;
} FunctionDef;

#endif
