#ifndef PARSER_H_00
#define PARSER_H_00
#include <stdint.h>
#include <stdbool.h>
#include <arena.h>

#define pinned

typedef struct Expression Expression;
typedef struct Statement Statement;
typedef struct FunctionDef FunctionDef;

enum ExpressionType {
    EXPRESSION_VARIABLE,
    EXPRESSION_BINOP, 
    EXPRESSION_UNOP, 
    EXPRESSION_LITERAL_INT,
    EXPRESSION_LITERAL_UINT,
    EXPRESSION_LITERAL_FLOAT,
    EXPRESSION_LITERAL_STRING,
    EXPRESSION_CALL,
    EXPRESSION_ARRAY_INDEX,
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
#define TYPE_ID_BUILTIN_COUNT 4
// Index into name_table
typedef uint64_t name_id;
#define NAME_ID_INVALID ((name_id) -1)
#define NAME_ID_FN 0
#define NAME_ID_STRUCT 1
#define NAME_ID_IF 2
#define NAME_ID_WHILE 3

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

struct Expression pinned {
    enum ExpressionType type;
    LineInfo line;
    union {
        VariableExpr variable;
        BinaryOperation binop;
        UnaryOperation unop;
        LiteralExpr literal;
        CallExpr call;
        ArrayIndexExpr array_index;
    };
};

struct Statement pinned {
    enum {
        STATEMENT_ASSIGN, STATEMENT_EXPRESSION,
        STATEMENT_IF, STATEMENT_WHILE, STATEMET_INVALID
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
    };
    LineInfo line;
};

typedef struct FunctionDef pinned {
    const char* name;
    type_id return_type;
    uint64_t arg_count;
    VariableExpr* args;
    LineInfo line;
} FunctionDef;

typedef struct StructDef pinned {
    const char *name;
    uint64_t field_count;
    type_id* fields;
    LineInfo line;
} StructDef;

enum TypeDefKind {
    TYPEDEF_UINT64, TYPEDEF_INT64, TYPEDEF_FLOAT64,
    TYPEDEF_CSTRING, TYPEDEF_STRUCT,
    TYPEDEF_FUNCTION
};

typedef struct TypeDef pinned {
    enum TypeDefKind kind;
    union {
        StructDef struct_; // Only valid if kind is TYPEDEF_STRUCT
        FunctionDef* func; // TYPEDEF_FUNCTION
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
    PARSE_ERROR_INVALID_LITERAL = 7,
    PARSE_ERROR_REDEFINITION = 8,
    PARSE_ERROR_BAD_ARRAY_SIZE = 9,
    PARSE_ERROR_INTERNAL = 10
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

#endif
