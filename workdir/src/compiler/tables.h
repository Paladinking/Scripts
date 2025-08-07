#ifndef COMPILER_TABLES_H_00
#define COMPILER_TABLES_H_00

#include "utils.h"
#include <arena.h>

// Index into type_table
typedef uint64_t type_id;
#define TYPE_ID_INVALID ((type_id) -1)

// DO NOT REORDER THESE
#define TYPE_ID_UINT64 0
#define TYPE_ID_UINT32 1
#define TYPE_ID_UINT16 2
#define TYPE_ID_UINT8 3

#define TYPE_ID_UNSIGNED_COUNT 4

#define TYPE_ID_INT64 4
#define TYPE_ID_INT32 5
#define TYPE_ID_INT16 6
#define TYPE_ID_INT8 7

#define TYPE_ID_INTEGER_COUNT 8

#define TYPE_ID_FLOAT64 8
#define TYPE_ID_FLOAT32 9

#define TYPE_ID_NUMBER_COUNT 10

#define TYPE_ID_NONE 10
#define TYPE_ID_BOOL 11
#define TYPE_ID_NULL 12
#define TYPE_ID_BUILTIN_COUNT 13

// Index into var_table (part of quads)
typedef uint64_t var_id;
#define VAR_ID_INVALID ((var_id) -1)
typedef var_id label_id;
#define LABEL_ID_INVALID ((label_id) -1)
typedef uint64_t quad_id;
#define QUAD_ID_INVALID ((quad_id) -1)
// TODO: Delete
// func_id is just used as a boolean to tell if variables are global or not
typedef uint64_t func_id;
#define FUNC_ID_GLOBAL ((func_id) - 1)
#define FUNC_ID_NONE ((func_id) - 2)

// Index into name_table
typedef uint64_t name_id;
#define NAME_ID_INVALID ((name_id) -1)
#define NAME_ID_FN 0
#define NAME_ID_STRUCT 1
#define NAME_ID_IF 2
#define NAME_ID_WHILE 3
#define NAME_ID_ELSE 4
#define NAME_ID_RETURN 5
#define NAME_ID_TRUE 6
#define NAME_ID_FALSE 7
#define NAME_ID_EXTERN 8
#define NAME_ID_NULL 9
#define NAME_ID_BUILTIN_COUNT 10

// Index into hash table
typedef uint32_t hash_id;

enum NameKind {
    NAME_VARIABLE,
    NAME_FUNCTION,
    NAME_TYPE,
    NAME_KEYWORD
};

typedef struct FunctionDef FunctionDef;
typedef struct TypeDef TypeDef;

typedef struct NameData {
    // NameData entry with same hash
    name_id parent;

    hash_id hash_link;

    uint32_t name_len;
    const uint8_t* name; // Ptr with ownership (arena?)
 
    enum NameKind kind;
    bool has_var;
    union {
        // Used when kind == NAME_FUNCTION
        FunctionDef* func_def;
        // Used when kind == NAME_TYPE
        TypeDef* type_def;
        // Used when kind == NAME_VARIABLE before var_table is created.
        func_id function;
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

    uint64_t scope_count;
    uint64_t scope_capacity;
    name_id* scope_stack; // id of first name not to be freed on scope end
} NameTable;

enum VarKind {
    VAR_FUNCTION, // Function reference, not allocated
    VAR_TEMP, // Unnamed variable, function allocated
    VAR_LOCAL, // Named local variable, function allocated
    VAR_GLOBAL, // Global variable, globaly allocated
    VAR_CALLARG, // Argument to function, function allocated
    VAR_ARRAY, // Local array, function allocated (mem only)
    VAR_ARRAY_GLOBAL // Global array, globaly allocated
};

// Returns if this variable is function allocated
static inline bool var_local(enum VarKind kind) {
    return kind == VAR_TEMP || kind == VAR_LOCAL || kind == VAR_CALLARG ||
           kind == VAR_ARRAY;
}

typedef uint32_t symbol_ix;

typedef struct VarData {
    enum VarKind kind;
    name_id name; // NAME_ID_INVALID for unnamed / temporary variables.
    type_id type;

    uint32_t byte_size;
    uint32_t alignment;

    func_id function;

    uint32_t reads;
    uint32_t writes;
    enum {
        ALLOC_NONE, ALLOC_REG, ALLOC_MEM, ALLOC_IMM
    } alloc_type;
    union {
        symbol_ix symbol_ix; // Valid for non-local variables
        struct {
            uint32_t offset;
        } memory;
        uint32_t reg;
        struct {
            uint64_t uint64;
            int64_t int64;
            double float64;
            bool boolean;
        } imm;
    };
} VarData;

typedef struct VarList {
    uint64_t size;
    uint64_t cap;
    VarData* data;
} VarList;

typedef struct StructDef {
    name_id name;
    uint64_t field_count;
    type_id* fields;
} StructDef;

typedef struct BuiltinDef {
    name_id name;
    uint32_t byte_size;
    uint32_t byte_alignment;
} BuiltinDef;

enum TypeDefKind {
    TYPEDEF_BUILTIN,
    TYPEDEF_STRUCT,
    TYPEDEF_FUNCTION
};

typedef struct TypeDef {
    enum TypeDefKind kind;
    union {
        StructDef struct_;  // Valid if kind is TYPEDEF_STRUCT
        FunctionDef* func;  // Valid if kind is TYPEDEF_FUNCTION
        BuiltinDef builtin; // Valid if kind is TYPEDEF_BUILTIN
    };
} TypeDef;


typedef struct TypeData {
    enum {
        TYPE_NORMAL, TYPE_ARRAY, TYPE_PTR
    } kind;
    TypeDef* type_def;
    type_id parent;
    type_id ptr_type;
    uint64_t array_size;
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

typedef struct AllocationInfo {
    uint64_t size;
    uint64_t allignment;
} AllocInfo;

typedef struct StringLiteral {
    const uint8_t* bytes;
    uint32_t len;
    var_id var;
    struct StringLiteral* next;
} StringLiteral;

typedef struct Parser {
    FunctionTable function_table;
    TypeTable type_table;
    NameTable name_table;

    FunctionTable externs;
    
    const uint8_t* indata;
    uint64_t input_size;

    uint64_t pos;

    Arena arena;

    StringLiteral* first_str;
    StringLiteral* last_str;

    Error* first_error;
    Error* last_error;
} Parser;

type_id type_of(NameTable* name_table, name_id name);

type_id type_array_of(TypeTable* type_table, type_id id, uint64_t size);

type_id type_ptr_of(TypeTable* type_table, type_id id);

type_id type_function_create(TypeTable* type_table, FunctionDef* def, Arena* arena);

void name_scope_begin(NameTable* name_table);

void name_scope_end(NameTable* name_table);

name_id name_insert(NameTable* name_table, StrWithLength name,
                    type_id type, enum NameKind kind, Arena* arena);

name_id name_type_insert(NameTable* name_table, StrWithLength name,
                         type_id type, TypeDef* def, Arena* arena);

// Create function type and insert it into name table
name_id name_function_insert(NameTable* name_table, StrWithLength name,
                             TypeTable* type_table, FunctionDef* def, Arena* arena);

name_id name_variable_insert(NameTable* name_table, StrWithLength name, type_id type,
                             func_id func, Arena* arena);

name_id name_find(NameTable* name_table, StrWithLength name);

AllocInfo type_allocation(TypeTable* type_table, type_id id);

#endif
