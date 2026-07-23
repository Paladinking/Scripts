#ifndef COMPILER_ASM_EDITOR_H_00
#define COMPILER_ASM_EDITOR_H_00

#include <arena.h>
#include <stdint.h>
#include <dynamic_string.h>
#include "../linker/linker.h"


#define GEN_REG_COUNT 32

#define RSP 7
#define R0 0
#define R1 1
#define R2 2
#define R3 3
#define R4 4
#define R5 5
#define R6 6
#define R7 7
#define R8 8
#define R9 9
#define R10 10
#define R11 11
#define R12 12
#define R13 13
#define R14 14
#define R15 15

enum EditorOpcode {
    OP_NOP   = 0b00000000,
    OP_HALT  = 0b00001000,
    OP_RET   = 0b00010000,
    OP_RETI  = 0b00011000,
    OP_SLEEP = 0b00100000,

    OP_MOV     = 0b00000001,
    OP_AND     = 0b00001001,
    OP_OR      = 0b00010001,
    OP_XOR     = 0b00011001,
    OP_ADD     = 0b00100001,
    OP_NOT     = 0b00101001,
    OP_SUB     = 0b00110001,
    OP_MULU    = 0b00111001,
    OP_MULS    = 0b01000001,
    OP_DIVU    = 0b01001001,
    OP_DIVS    = 0b01010001,
    OP_STORE32 = 0b01011001,
    OP_STORE16 = 0b01100001,
    OP_STORE8  = 0b01101001,
    OP_LOAD32  = 0b01110001,
    OP_LOAD16  = 0b01111001,
    OP_LOAD8   = 0b10000001,
    OP_TEQ     = 0b10001001,
    OP_TGTU    = 0b10010001,
    OP_TLTU    = 0b10011001,
    OP_TGTS    = 0b10100001,
    OP_TLTS    = 0b10101001,

    OP_MOVFS1 = 0b00000111,
    OP_MOVTS1 = 0b00100111,
    OP_MOVFS2 = 0b10000111,
    OP_MOVTS2 = 0b10100111,
    OP_PUSH   = 0b00001111,
    OP_POP    = 0b00010111,

    OP_ADDI    = 0b00000010,
    OP_TEQI    = 0b00001010,
    OP_MOVI    = 0b00010010,
    OP_RSHIFTI = 0b00011010,
    OP_LSHIFTI = 0b00100010,

    OP_STOREX32 = 0b00000011,
    OP_STOREX16 = 0b00001011,
    OP_STOREX8  = 0b00010011,
    OP_LOADX32  = 0b00011011,
    OP_LOADX16  = 0b00100011,
    OP_LOADX8   = 0b00101011,

    OP_JMP  = 0b00000100,
    OP_JT   = 0b00001100,
    OP_JF   = 0b00010100,
    OP_CALL = 0b00011100,
    OP_INT  = 0b00100100,

    OP_LRA  = 0b00000101,

    OPCODE_START = 256,
    OPCODE_END = 257,
    OPCODE_LABEL = 258
};



typedef struct EditorOp {
    uint8_t opcode;
    union {
        struct {
            uint8_t reg1;
            uint8_t reg2;
            uint8_t imm;
        } imm8; // Used by RR, SR, RRI 
        struct {
            uint8_t reg;
            uint16_t imm;
        } imm16; // Used by RI
        struct {
            uint8_t reg;
            uint32_t imm;
        } imm24; // Used by I, LRA
    };

    struct EditorOp* next;
} EditorOp;

typedef struct EditorAsmCtx {
    EditorOp* start;
    EditorOp* end;
    Arena arena;

    Object* object;
    section_ix code_section;

    uint64_t label_count;
    uint64_t label_cap;
    EditorOp* labels;
} EditorAsmCtx;

void editor_asm_ctx_create(EditorAsmCtx* ctx, Object* object, section_ix code_section);
void editor_asm_add_n(EditorAsmCtx* ctx, enum EditorOpcode op);
void editor_asm_add_rr(EditorAsmCtx* ctx, enum EditorOpcode op, uint8_t reg1, uint8_t reg2);
void editor_asm_add_r(EditorAsmCtx* ctx, enum EditorOpcode op, uint8_t reg);
void editor_asm_add_sr(EditorAsmCtx* ctx, enum EditorOpcode op, uint8_t sreg, uint8_t reg);
void editor_asm_add_ri(EditorAsmCtx* ctx, enum EditorOpcode op, uint8_t reg1, int64_t imm);
void editor_asm_add_rri(EditorAsmCtx* ctx, enum EditorOpcode op, uint8_t reg1, uint8_t reg2, int64_t imm);
void editor_asm_add_label(EditorAsmCtx* ctx, uint64_t label);
void editor_asm_add_i(EditorAsmCtx* ctx, enum EditorOpcode op, int64_t imm);
void editor_asm_add_lra(EditorAsmCtx* ctx, uint8_t reg, uint64_t label);

#endif
