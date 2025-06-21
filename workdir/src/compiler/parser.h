#ifndef PARSER_H_00
#define PARSER_H_00
#include <stdint.h>
#include <stdbool.h>
#include <arena.h>
#include "log.h"

#ifdef _MSC_VER
#include <printf.h>
#define assert(e) if (!(e)) { _wprintf_e(L"Assertion failed: '%s' at %S:%u", L#e, __FILE__, __LINE__); ExitProcess(1); }
#else
#include <assert.h>
#endif

#define pinned

typedef struct Expression Expression;
typedef struct Statement Statement;
typedef struct FunctionDef FunctionDef;

enum ExpressionKind {
    EXPRESSION_VARIABLE,
    EXPRESSION_BINOP, 
    EXPRESSION_UNOP, 
    EXPRESSION_LITERAL_INT,
    EXPRESSION_LITERAL_UINT,
    EXPRESSION_LITERAL_FLOAT,
    EXPRESSION_LITERAL_STRING,
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
    UNOP_BITNOT, UNOP_BOOLNOT, UNOP_NEGATVIE, UNOP_POSITIVE, UNOP_PAREN
};

// Index into type_table
typedef uint64_t type_id;
#define TYPE_ID_INVALID ((type_id) -1)
#define TYPE_ID_UINT64 0
#define TYPE_ID_INT64 1
#define TYPE_ID_FLOAT64 2
#define TYPE_ID_CSTRING 3
#define TYPE_ID_NONE 4
#define TYPE_ID_BOOL 5
#define TYPE_ID_BUILTIN_COUNT 6
// Index into name_table
typedef uint64_t name_id;
#define NAME_ID_INVALID ((name_id) -1)
#define NAME_ID_FN 0
#define NAME_ID_STRUCT 1
#define NAME_ID_IF 2
#define NAME_ID_WHILE 3
#define NAME_ID_ELSE 4
#define NAME_ID_RETURN 5

// Index into var_table (part of quads)
typedef uint64_t var_id;
#define VAR_ID_INVALID ((var_id) -1)

// Index into hash table
typedef uint32_t hash_id;

typedef struct LineInfo {
    bool real;
    uint64_t start;
    uint64_t end;
} LineInfo;

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

typedef struct CallExpr {
    Expression* function;
    uint64_t arg_count;
    Expression** args;
} CallExpr;

typedef struct ArrayIndexExpr {
    Expression* array;
    Expression* index;
} ArrayIndexExpr;

typedef struct LiteralExpr {
    union {
        uint64_t uint;
        int64_t iint;
        double float64;
        struct {
            uint8_t* bytes;
            uint64_t len;
        } string;
    };
} LiteralExpr;

typedef struct CastExpr {
    type_id type;
    Expression* e;
} CastExpr;

struct Expression pinned {
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
        CallExpr call;
        ArrayIndexExpr array_index;
        CastExpr cast;
    };
};

struct Statement pinned {
    enum {
        STATEMENT_ASSIGN, STATEMENT_EXPRESSION,
        STATEMENT_IF, STATEMENT_WHILE, STATEMENT_RETURN,
        STATEMET_INVALID
    } type;
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

typedef struct CallArg pinned {
    name_id name;
    type_id type;
    LineInfo line;
} CallArg;

typedef struct FunctionDef pinned {
    name_id name;
    type_id return_type;
    uint64_t arg_count;
    CallArg* args;
    uint64_t statement_count;
    Statement** statements;
    LineInfo line;
} FunctionDef;

typedef struct StructDef pinned {
    name_id name;
    uint64_t field_count;
    type_id* fields;
    LineInfo line;
} StructDef;

enum TypeDefKind {
    TYPEDEF_UINT64, TYPEDEF_INT64, TYPEDEF_FLOAT64,
    TYPEDEF_CSTRING, TYPEDEF_NONE, TYPEDEF_BOOL,
    TYPEDEF_STRUCT,
    TYPEDEF_FUNCTION
};

typedef struct TypeDef pinned {
    enum TypeDefKind kind;
    union {
        StructDef struct_; // Only valid if kind is TYPEDEF_STRUCT
        FunctionDef* func; // Only valid if kind is TYPEDEF_FUNCTION
        name_id name;      // Valid otherwise
    };
} TypeDef;


typedef struct TypeData {
    enum {
        TYPE_NORMAL, TYPE_ARRAY
    } kind;
    TypeDef* type_def;
    type_id parent;
    type_id array_type;
} TypeData;

typedef struct TypeTable {
    uint64_t size;
    uint64_t capacity;
    TypeData* data;
} TypeTable;

typedef struct FunctionTable {
    uint64_t size;
    uint64_t capacity;
    FunctionDef** data;
} FunctionTable;

// Linked list of array sizes
typedef struct ArraySize {
    uint64_t size;
    struct ArraySize* next;
} ArraySize;

enum NameKind {
    NAME_VARIABLE,
    NAME_FUNCTION,
    NAME_TYPE,
    NAME_KEYWORD
};

typedef struct NameData {
    // NameData entry with same hash
    name_id parent;

    hash_id hash_link;

    uint32_t name_len;
    const uint8_t* name; // Ptr with ownership (arena?)
 
    enum NameKind kind;
    union {
        // Used when kind == NAME_FUNCTION
        FunctionDef* func_def;
        // Used when kind == NAME_TYPE
        TypeDef* type_def;
        // Used when kind == NAME_VARIABLE and type is array
        ArraySize* array_size;
        // Used when kind == NAME_VARIABLE after var_table is created.
        var_id variable;
    };
    // For NAME_KEYWORD: TYPE_ID_INVALID
    // For NAME_VARIABLE: Type of variable
    // For NAME_TYPE: type_id of type itself (non-array type)
    // For NAME_FUNCTION: unique type of the function
    type_id type;
} NameData;

#define NAME_TABLE_HASH_ENTRIES 1024

typedef struct NameTable {
    uint64_t size;
    uint64_t capacity;
    NameData* data;

    name_id hash_map[NAME_TABLE_HASH_ENTRIES];
} NameTable;

enum ParseErrorKind {
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
    TYPE_ERROR_WRONG_ARG_COUNT = 14
};

typedef struct ParseError {
    enum ParseErrorKind kind;
    LineInfo pos;
    struct ParseError* next;
    const char* file;
    uint64_t internal_line;
} ParseError;

typedef struct Parser {
    FunctionTable function_table;
    TypeTable type_table;
    NameTable name_table;

    uint64_t scope_count;
    uint64_t scope_capacity;
    name_id* scope_stack; // id of first name not to be freed on scope end
    
    const uint8_t* indata;
    uint64_t input_size;

    uint64_t pos;

    Arena arena;

    ParseError* first_error;
    ParseError* last_error;
} Parser;

bool Parser_create(Parser* parser);

Expression* parse_expression(Parser* parser);

Statement** parse_statements(Parser* parser, uint64_t* count);

bool parse_program(Parser* parser);

// TODO: move?

void find_row_col(const Parser* parser, uint64_t pos, uint64_t* row, uint64_t* col);

type_id type_of(Parser* parser, name_id name);

type_id array_of(Parser* parser, type_id id);

void fatal_error_cb(Parser* parser, enum ParseErrorKind error, LineInfo info,
                    const char* file, uint64_t line);

void error_cb(Parser* parser, enum ParseErrorKind error, LineInfo line,
        const char* file, uint64_t iline);

#define fatal_error(p, e, l) fatal_error_cb(p, e, l, __FILE__, __LINE__)
#define add_error(p, e, l) do { LOG_INFO("Adding error %d at %llu", e, p->pos); error_cb(p, e, l, __FILE__, __LINE__); } while(0);

#endif
