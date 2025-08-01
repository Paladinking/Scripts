#ifndef COMPILER_ASM_AMD64_H_00
#define COMPILER_ASM_AMD64_H_00

#include <stdint.h>
#include <arena.h>
#include <dynamic_string.h>
#include <ochar.h>

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
    SKIP
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
    OP_SETZ,
    
    RESERVE_MEM,
    DECLARE_MEM,
    OPCODE_START,
    OPCODE_END,
    OPCODE_LABEL,
    OPCODE_TEXT_SECTION,
    OPCODE_DATA_SECTION,
    OPCODE_RDATA_SECTION
};

extern const Encodings ENCODINGS[RESERVE_MEM];

typedef struct Adm64Operand {
    uint8_t type;
    uint8_t reg;
    uint8_t scale_reg;
    uint8_t scale; // 0 (no scaled offset), 1, 2, 4 or 8
    union {
        int32_t offset;
        uint32_t label;
        uint8_t imm[8];
    };
} Amd64Operand;

typedef struct DynamicModule {
    const uint8_t* name;
    uint32_t name_len;

    uint32_t label_offset;
    uint32_t label_count;

    struct DynamicModule* next;
} DynamicModule;

typedef struct Amd64Op {
    uint16_t opcode;
    uint8_t count;
    uint8_t rex;
    uint32_t max_offset;
    uint32_t offset;
    union {
        // Valid when opcode != DECLARE_MEM && opcode != RESERVE_MEM
        Amd64Operand operands[OPERAND_MAX];
        // Valid when opcode == DECLARE_MEM || opcode == RESERVE_MEM
        struct {
            uint64_t datasize;
            uint64_t alignment;
            const uint8_t* data;
        } mem;
        // Valid when opcode == OPCODE_LABEL
        uint64_t label;
    };
    struct Amd64Op* next;
    struct Amd64Op* prev;
} Amd64Op;

typedef struct LabelData {
    const uint8_t* symbol;
    uint32_t symbol_len;
    Amd64Op* op;
} LabelData;

typedef struct AsmCtx {
    Amd64Op* start;
    Amd64Op* end;
    Arena arena;

    Amd64Op* import_start;
    Amd64Op* import_end;

    uint64_t data_count;
    uint32_t dynamic_label_offset;
    uint32_t dynamic_label_count; 
    Amd64Op* entrypoint;

    DynamicModule* dyn_module;

    uint64_t label_count;
    uint64_t label_cap;
    LabelData* labels;
} AsmCtx;

void asm_ctx_create(AsmCtx* ctx);

uint64_t asm_add_import(AsmCtx* ctx, const uint8_t* name, uint32_t name_len);

void asm_add_module(AsmCtx* ctx, const ochar_t* name);

void asm_set_entrypoint(AsmCtx* ctx, uint64_t ix);

void asm_instr(AsmCtx* ctx, enum Amd64Opcode op);

void asm_reg_var(AsmCtx* ctx, uint8_t size, uint8_t reg);

void asm_label_var(AsmCtx* ctx, uint64_t ix);

void asm_mem_var(AsmCtx* ctx, uint8_t size, uint8_t reg, uint8_t scale,
                 uint8_t scale_reg, int32_t offset);

void asm_global_mem_var(AsmCtx* ctx, uint8_t size, uint64_t data);

void asm_imm_var(AsmCtx* ctx, uint8_t size, const uint8_t* data);

void asm_instr_end(AsmCtx* ctx);

uint64_t asm_declare_mem(AsmCtx* ctx, uint64_t len, const uint8_t* data, uint32_t symbol_len,
                         const uint8_t* symbol, uint64_t alignment);

uint64_t asm_reserve_mem(AsmCtx* ctx, uint64_t len, uint32_t symbol_len, const uint8_t* symbol,
                         uint64_t alignment);

void asm_put_label(AsmCtx* ctx, uint64_t label, const uint8_t* sym, uint32_t sym_len);

void asm_serialize(AsmCtx* ctx, String* dest);

void asm_assemble(AsmCtx* ctx);


#endif
