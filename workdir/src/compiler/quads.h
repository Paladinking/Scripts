#ifndef QUADS_H_00
#define QUADS_H_00
#include <stdint.h>
#include <arena.h>

#include "tables.h"
#include "varset.h"


#define QUAD_64_BIT 0x010000
#define QUAD_32_BIT 0x020000
#define QUAD_16_BIT 0x040000
#define QUAD_8_BIT 0x080000
#define QUAD_0_BIT 0x100000

#define QUAD_PTR_SIZE QUAD_64_BIT

#define QUAD_SINT 0x01000000
#define QUAD_UINT 0x02000000
#define QUAD_FLOAT 0x04000000
#define QUAD_BOOL 0x08000000
#define QUAD_PTR 0x10000000

#define QUAD_SCALE_1 0x0100000000
#define QUAD_SCALE_2 0x0200000000
#define QUAD_SCALE_4 0x0400000000
#define QUAD_SCALE_8 0x0800000000

#define QUAD_TYPE_MASK 0xffff
#define QUAD_DATASIZE_MASK 0xff0000
#define QUAD_DATATYPE_MASK 0xff000000
#define QUAD_SCALE_MASK 0xff00000000


// <...> mean allocated variable (memory / register)
// $... means function
// :... means label
// [...] means constant
// x means unused
enum QuadType {
    QUAD_DIV, // <op1>, <op2> -> <dest>
    QUAD_MUL, // <op1>, <op2> -> <dest>
    QUAD_MOD, // <op1>, <op2> -> <dest>
    QUAD_SUB, // <op1>, <op2> -> <dest>
    QUAD_ADD, // <op1>, <op2> -> <dest>
    QUAD_RSHIFT, // <op1>, <op2> -> <dest>
    QUAD_LSHIFT, // <op1>, <op2> -> <dest>
    QUAD_NEGATE, // <op1>, x -> <dest>

    QUAD_CMP_EQ, // <op1>, <op2> -> <bool>
    QUAD_CMP_NEQ, // <op1>, <op2> -> <bool>
    QUAD_CMP_G, // <op1>, <op2> -> <bool>
    QUAD_CMP_L, // <op1>, <op2> -> <bool>
    QUAD_CMP_GE, // <op1>, <op2> -> <bool>
    QUAD_CMP_LE, // <op1>, <op2> -> <bool>

    QUAD_JMP, // :dest, x -> x
    QUAD_JMP_FALSE, // :dest, <bool> -> x
    QUAD_JMP_TRUE, // :dest, <bool> -> x
    QUAD_LABEL, // :self, x -> x

    QUAD_BOOL_AND, // <op1>, <op2> -> <dest>
    QUAD_BOOL_OR, // <op1>, <op2> -> <dest>
    QUAD_BOOL_NOT, // <op1>, x -> <dest>
    
    QUAD_BIT_AND, // <op1>, <op2> -> <dest>
    QUAD_BIT_OR, // <op1>, <op2> -> <dest>
    QUAD_BIT_XOR, // <op1>, <op2> -> <dest>
    QUAD_BIT_NOT, // <op1>, x -> <dest>

    QUAD_CAST_TO_FLOAT64, // <val>, x -> <dest>
    QUAD_CAST_TO_FLOAT32, // <val>, x -> <dest>
    QUAD_CAST_TO_INT64, // <val>, x -> <dest>
    QUAD_CAST_TO_INT32, // <val>, x -> <dest>
    QUAD_CAST_TO_INT16, // <val>, x -> <dest>
    QUAD_CAST_TO_INT8, // <val>, x -> <dest>
    QUAD_CAST_TO_UINT64, // <val>, x -> <dest>
    QUAD_CAST_TO_UINT32, // <val>, x -> <dest>
    QUAD_CAST_TO_UINT16, // <val>, x -> <dest>
    QUAD_CAST_TO_UINT8, // <val>, x -> <dest>
    QUAD_CAST_TO_BOOL, // <val>, x -> <dest>

    QUAD_ARRAY_TO_PTR, // <array>, x -> <dest>
    
    QUAD_PUT_ARG, // [argNr], <var> -> x
    QUAD_CALL, // $func, x -> x
    QUAD_RETURN, // <val>, x -> x
    QUAD_GET_RET, // x, x -> <dest>
    QUAD_GET_ARG, // [argNr], x -> <dest>

    QUAD_MOVE, // <val>, x -> <dest>
    QUAD_GET_ARRAY_ADDR, // <array>, <offset> -> <val>
    QUAD_CALC_ARRAY_ADDR, // <array>, <offset> -> <ptr>
    QUAD_SET_ADDR, // <ptr>, <data> -> x

    QUAD_CREATE, // [val], x -> <dest>
    
    QUAD_ADDROF, // <val>, x -> <dest>
    QUAD_DEREF // <ptr>, x -> <dest>
};

typedef struct Quad {
    uint64_t type; // Bitwise or of QuadType, Data size and Data type
    union {
        var_id var;
        label_id label;
        uint64_t uint64;
        int64_t sint64;
        double float64;
    } op1;
    var_id op2;
    var_id dest;

    struct Quad* last_quad;
    struct Quad* next_quad;
} Quad;

typedef struct Quads {
    Quad* head;
    Quad* tail;
    uint64_t quads_count;
    uint64_t label_count;
    VarList globals;
} Quads;

// Adds op1 and op2 vars to <use>, unless already part of define
// Adds dest to define
void Quad_add_usages(const Quad* q, VarSet* use, VarSet* define, VarList* vars);

void Quad_update_live(const Quad* q, VarSet* live);

void Quad_GenerateQuads(Parser* parser, Quads* quads, Arena* arena);

#endif
