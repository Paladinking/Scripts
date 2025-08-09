#ifndef COMPILER_ASM_AMD64_H_00
#define COMPILER_ASM_AMD64_H_00

#include <stdint.h>
#include <arena.h>
#include <dynamic_string.h>
#include <ochar.h>

#include "../linker/linker.h"

enum MnemonicPart {
    PREFIX, // e.g 0x66 (operand size overide prefix)
    REX, // Rex Prefix 
    OPCODE, // Standard opcode
    OPCODE2, // Opcode from secondary opcode map
    MOD_RM, // ModRM byte, should always preceed REG_REG, RM_REG or MEM
    REG_REG, // Register in ModRm.reg
    PLUS_REG, // Register in second opcode nibble, should come directly after OPCODE, OPCODE2
    RM_MEM, // Memory in ModRm.r/m (+ potentially SIB)
    RM_REG, // Register in ModRm.r/m
    IMM, // imm
    REL_ADDR, // Relative jump address
    SKIP, // Skip one operand
};

typedef struct Mnemonic {
    uint8_t type;
    uint8_t byte;
} Mnemonic;

#define MNEMOIC_MAX 8
#define OPERAND_MAX 3

enum Amd64OperandType {
    OPERAND_MEM8 = 1,  // e.g BYTE [rcx + 8 * rdx - 32]
    OPERAND_MEM16, // e.g WORD [rcx + 8 * rdx - 32]
    OPERAND_MEM32, // e.g DWORD [rcx + 8 * rdx - 32]
    OPERAND_MEM64, // e.g QWORD [rcx + 8 * rdx - 32]
    OPERAND_REG8,  // e.g al
    OPERAND_REG16, // e.g cx
    OPERAND_REG32, // e.g r10d
    OPERAND_REG64, // e.g r12
    OPERAND_LABEL, // e.g L12
    OPERAND_SYMBOL_LABEL, // e.g function
    OPERAND_GLOBAL_MEM8,  // e.g BYTE [__literal_string0]
    OPERAND_GLOBAL_MEM16, // e.g WORD [__literal_string0]
    OPERAND_GLOBAL_MEM32, // e.g DWORD [__literal_string0]
    OPERAND_GLOBAL_MEM64, // e.g QWORD [__literal_string0]
    OPERAND_IMM8,
    OPERAND_IMM16,
    OPERAND_IMM32,
    OPERAND_IMM64
};

typedef struct Encoding {
    uint8_t operand_count;
    uint8_t mnemonic_count;
    Mnemonic mnemonic[MNEMOIC_MAX];
    enum Amd64OperandType operands[OPERAND_MAX];
} Encoding;

// Opcode is extended by number in ModRm.reg
#define MOD_RM_OPCODE(n) {MOD_RM, (n) << 3}

typedef struct Encodings {
    uint32_t count;
    const Encoding* encodings;
} Encodings;

#define RAX 0
#define RCX 1
#define RDX 2
#define RBX 3
#define RSP 4
#define RBP 5
#define RSI 6
#define RDI 7
#define R8 8
#define R9 9
#define R10 10
#define R11 11
#define R12 12
#define R13 13
#define R14 14
#define R15 15

#define GEN_REG_COUNT 16

enum Amd64Opcode {
    OP_NOP, OP_MOV, OP_PUSH, OP_POP, OP_RET,
    OP_AND, OP_OR,
    OP_SUB, OP_ADD, OP_LEA,
    OP_MUL, OP_IMUL, OP_DIV, OP_IDIV,
    OP_XOR, OP_NEG,
    OP_SHR, OP_SAR, OP_SHL, OP_SAL,
    OP_CALL,
    OP_JG, OP_JA, OP_JLE, OP_JBE,
    OP_JGE, OP_JAE, OP_JL, OP_JB,
    OP_JNE, OP_JNZ, OP_JE, OP_JZ,
    OP_JMP, 
    OP_CMP, OP_TEST,
    OP_CMOVG, OP_CMOVA, OP_CMOVLE, OP_CMOVBE,
    OP_CMOVGE, OP_CMOVAE, OP_CMOVL, OP_CMOVB,
    OP_CMOVNE, OP_CMOVNZ, OP_CMOVE, OP_CMOVZ,
    OP_MOVSXD, OP_MOVSX, OP_MOVZX,
    OP_SETG, OP_SETA, OP_SETLE, OP_SETBE,
    OP_SETGE, OP_SETAE, OP_SETL, OP_SETB,
    OP_SETNE, OP_SETNZ, OP_SETE, OP_SETZ,
    
    OPCODE_START,
    OPCODE_END,
    OPCODE_LABEL
};

extern const Encodings ENCODINGS[OPCODE_START];

typedef struct Adm64Operand {
    uint8_t type;
    uint8_t reg;
    uint8_t scale_reg;
    uint8_t scale; // 0 (no scaled offset), 1, 2, 4 or 8
    int32_t offset;
    union {
        uint32_t label;
        symbol_ix symbol;
        uint8_t imm[8];
    };
} Amd64Operand;

typedef struct Amd64Op {
    uint16_t opcode;
    uint8_t count;
    uint8_t rex;
    uint32_t max_offset;
    uint32_t offset;
    union {
        // Valid when opcode != OPCODE_START && opcode != OPCODE_LABEL
        Amd64Operand operands[OPERAND_MAX];
        // Valid when opcode == OPCODE_LABEL
        uint64_t label;
        // Valid when opcode == OPCODE_START
        symbol_ix symbol;
    };
    struct Amd64Op* next;
    struct Amd64Op* prev;
} Amd64Op;

typedef struct LabelData {
    Amd64Op* op;
} LabelData;

typedef struct AsmCtx {
    Amd64Op* start;
    Amd64Op* end;
    Arena arena;

    Object* object;
    section_ix code_section;

    uint64_t label_count;
    uint64_t label_cap;
    LabelData* labels;
} AsmCtx;

void asm_ctx_create(AsmCtx* ctx, Object* object, section_ix code_section);

void asm_instr(AsmCtx* ctx, enum Amd64Opcode op);

void asm_reg_var(AsmCtx* ctx, uint8_t size, uint8_t reg);

void asm_label_var(AsmCtx* ctx, uint64_t ix);

void asm_symbol_label_var(AsmCtx* ctx, symbol_ix symbol);

void asm_mem_var(AsmCtx* ctx, uint8_t size, uint8_t reg, uint8_t scale,
                 uint8_t scale_reg, int32_t offset);

void asm_global_mem_var(AsmCtx* ctx, uint8_t size, symbol_ix symbol,
                        int32_t offset);

void asm_imm_var(AsmCtx* ctx, uint8_t size, const uint8_t* data);

void asm_instr_end(AsmCtx* ctx);

void asm_put_label(AsmCtx* ctx, uint64_t label);

void asm_serialize(AsmCtx* ctx, String* dest);

void asm_assemble(AsmCtx* ctx, symbol_ix symbol);

void asm_reset(AsmCtx* ctx);


#endif
