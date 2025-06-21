#ifndef QUADS_H_00
#define QUADS_H_00
#include <stdint.h>
#include <arena.h>

#include "parser.h"


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
    QUAD_CAST_TO_INT64, // <val>, x -> <dest>
    QUAD_CAST_TO_UINT64, // <val>, x -> <dest>
    QUAD_CAST_TO_BOOL, // <val>, x -> <dest>
    
    QUAD_PUT_ARG, // <val>, x -> x
    QUAD_CALL, // $func, x -> x
    QUAD_RETURN, // <val>, x -> x
    QUAD_GET_RET, // x, x -> <dest>

    QUAD_MOVE, // <val>, x -> <dest>
    QUAD_GET_ADDR, // <ptr>, <offset> -> <dest>
    QUAD_CALC_ADDR, // <ptr>, <offset> -> <dest>
    QUAD_SET_ADDR, // <ptr>, <data> -> x

    QUAD_CREATE // [val], x -> <dest>
};

typedef var_id label_id;
#define LABEL_ID_INVALID ((label_id) -1)
typedef uint64_t quad_id;
#define QUAD_ID_INVALID ((quad_id) -1)

typedef struct VarData {
    name_id name; // NAME_ID_INVALID for unnamed / temporary variables.
    type_id type;
    ArraySize* array_size;
    uint32_t reads;
    uint32_t writes;
} VarData;

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

    // Entry in var_table, might be invalid
    union {
        VarData var_entry;
        quad_id label;
    };
} Quad;

typedef struct Quads {
    Quad* quads;
    uint64_t quads_count;
} Quads;

void Quad_GenerateQuads(Parser* parser, Quads* quads, Arena* arena);

#endif
