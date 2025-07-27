#include "mem.h"
#include "amd64.h"
#include "../utils.h"
#include "../log.h"
#include <printf.h>

void asm_ctx_create(AsmCtx* ctx) {
    if (!Arena_create(&ctx->arena, 0xffffffff, out_of_memory, NULL)) {
        LOG_CRITICAL("Failed creating arena");
        fatal_error(NULL, PARSE_ERROR_NONE, LINE_INFO_NONE);
        return;
    }
    ctx->start = Arena_alloc_type(&ctx->arena, Amd64Op);
    ctx->end = Arena_alloc_type(&ctx->arena, Amd64Op);

    ctx->start->next = ctx->end;
    ctx->start->prev = NULL;
    ctx->start->opcode = OPCODE_START;
    ctx->start->count = 0;

    ctx->end->next = NULL;
    ctx->end->prev = ctx->start;
    ctx->end->opcode = OPCODE_START;
    ctx->end->count = 0;

    ctx->label_count = 0;
    ctx->data_count = 0;
    ctx->label_cap = 32;
    ctx->labels = Mem_alloc(32 * sizeof(LabelData));
    if (ctx->labels == NULL) {
        out_of_memory(NULL);
    }
}

void asm_instr(AsmCtx* ctx, enum Amd64Opcode opcode) {
    Amd64Op* op = ctx->end;
    op->opcode = opcode;
}

void asm_reg_var(AsmCtx* ctx, uint8_t size, uint8_t reg) {
    Amd64Op* op = ctx->end;
    if (size == 1) {
        op->operands[op->count].type = OPERAND_REG8;
    } else if (size == 2) {
        op->operands[op->count].type = OPERAND_REG16;
    } else if (size == 4) {
        op->operands[op->count].type = OPERAND_REG32;
    } else {
        assert(size == 8);
        op->operands[op->count].type = OPERAND_REG64;
    }
    op->operands[op->count++].reg = reg;
}

void asm_label_var(AsmCtx* ctx, uint64_t ix) {
    ix += ctx->data_count;
    assert(ix <= UINT32_MAX);
    Amd64Op* op = ctx->end;
    op->operands[op->count].type = OPERAND_LABEL;
    op->operands[op->count++].label = ix;
}

void asm_mem_var(AsmCtx* ctx, uint8_t size, uint8_t reg, uint8_t scale,
                 uint8_t scale_reg, int32_t offset) {
    Amd64Op* op = ctx->end;
    if (size == 1) {
        op->operands[op->count].type = OPERAND_MEM8;
    } else if (size == 2) {
        op->operands[op->count].type = OPERAND_MEM16;
    } else if (size == 4) {
        op->operands[op->count].type = OPERAND_MEM32;
    } else {
        assert(size == 8);
        op->operands[op->count].type = OPERAND_MEM64;
    }
    op->operands[op->count].reg = reg;
    op->operands[op->count].scale = scale;
    op->operands[op->count].scale_reg = scale_reg;
    op->operands[op->count++].offset = offset;
}

void asm_global_mem_var(AsmCtx* ctx, uint8_t size, uint64_t data) {
    assert(data < ctx->data_count);
    Amd64Op* op = ctx->end;
    if (size == 1) {
        op->operands[op->count].type = OPERAND_GLOBAL_MEM8;
    } else if (size == 2) {
        op->operands[op->count].type = OPERAND_GLOBAL_MEM16;
    } else if (size == 4) {
        op->operands[op->count].type = OPERAND_GLOBAL_MEM32;
    } else {
        assert(size == 8);
        op->operands[op->count].type = OPERAND_GLOBAL_MEM64;
    }
    op->operands[op->count++].label = data;
}

void asm_imm_var(AsmCtx* ctx, uint8_t size, const uint8_t* data) {
    Amd64Op* op = ctx->end;
    if (size == 1) {
        op->operands[op->count].type = OPERAND_IMM8;
    } else if (size == 2) {
        op->operands[op->count].type = OPERAND_IMM16;
    } else if (size == 4) {
        op->operands[op->count].type = OPERAND_IMM32;
    } else {
        assert(size == 8);
        op->operands[op->count].type = OPERAND_IMM64;
    }
    memcpy(op->operands[op->count++].imm, data, size);
}

void asm_instr_end(AsmCtx* ctx) {
    Amd64Op* op = ctx->end;

    ctx->end = Arena_alloc_type(&ctx->arena, Amd64Op);
    ctx->end->next = NULL;
    ctx->end->prev = op;
    ctx->end->opcode = OPCODE_END;
    ctx->end->count = 0;

    op->next = ctx->end;
}

static uint64_t add_data_label(AsmCtx* ctx, const uint8_t* sym, uint32_t sym_len) {
    if (ctx->data_count > ctx->label_cap) {
        ctx->label_cap *= 2;
        LabelData* p = Mem_realloc(ctx->labels, ctx->label_cap * sizeof(LabelData));
        if (p == NULL) {
            out_of_memory(NULL);
        }
        ctx->labels = p;
    }
    ctx->data_count += 1;
    ctx->label_count += 1;
    uint64_t data_label = ctx->data_count - 1;

    ctx->labels[data_label].op = ctx->end;
    ctx->labels[data_label].symbol = sym;
    ctx->labels[data_label].symbol_len = sym_len;

    asm_instr(ctx, OPCODE_LABEL);
    ctx->end->label = data_label;
    asm_instr_end(ctx);

    return data_label;
}

uint64_t asm_declare_mem(AsmCtx* ctx, uint64_t len, const uint8_t* data, uint32_t symbol_len,
                         const uint8_t* symbol) {
    assert(ctx->label_count == ctx->data_count);

    uint64_t ix = add_data_label(ctx, symbol, symbol_len);

    Amd64Op* op = ctx->end;
    op->opcode = DECLARE_MEM;
    op->count = 255;
    op->mem.datasize = len;
    op->mem.data = data;

    asm_instr_end(ctx);
    return ix;
}

uint64_t asm_reserve_mem(AsmCtx* ctx, uint64_t len, uint32_t symbol_len,
                         const uint8_t* symbol) {
    assert(ctx->label_count == ctx->data_count);

    uint64_t ix = add_data_label(ctx, symbol, symbol_len);

    Amd64Op* op = ctx->end;
    op->opcode = RESERVE_MEM;
    op->count = 255;
    op->mem.datasize = len;
    op->mem.data = NULL;

    asm_instr_end(ctx);
    return ix;
}

void asm_put_label(AsmCtx* ctx, uint64_t label, const uint8_t* sym, uint32_t sym_len) {
    label += ctx->data_count;
    assert(label < UINT32_MAX);
    if (label >= ctx->label_count) {
        if (label >= ctx->label_cap) {
            do {
                ctx->label_cap *= 2;
            } while (ctx->label_cap <= label);
            LabelData* p = Mem_realloc(ctx->labels, ctx->label_cap * sizeof(LabelData));
            if (p == NULL) {
                out_of_memory(NULL);
            }
            ctx->labels = p;
        }
        for (uint64_t i = ctx->label_count; i < label; ++i) {
            ctx->labels[i].symbol = NULL;
            ctx->labels[i].symbol_len = 0;
            ctx->labels[i].op = NULL;
        }
        ctx->label_count = label + 1;
    }
    ctx->labels[label].op = ctx->end;
    ctx->labels[label].symbol = sym;
    ctx->labels[label].symbol_len = sym_len;

    asm_instr(ctx, OPCODE_LABEL);
    ctx->end->label = label;
    asm_instr_end(ctx);
}

const char* OP_NAMES[] = {
    "nop", "mov", "push", "pop", "ret",
    "and", "or",
    "sub", "add", "lea",
    "mul", "imul", "div", "idiv",
    "xor", "neg",
    "shr", "sar", "shl", "sal",
    "call",
    "jg", "ja", "jle", "jbe",
    "jge", "jae", "jl", "jb",
    "jne", "jnz", "je", "jz",
    "jmp", "cmp", "test",
    "cmovle", "cmovbe", "cmovl", "cmovb",
    "cmovg", "cmova", "cmovge", "cmovae",
    "cmove", "cmovz", "cmovne", "cmovnz",
    "movsxd", "movsx", "movzx",
    "setz"
};


const char* QWORD_REG_NAMES[] = {
    "rax", "rcx", "rdx", "rbx", "rsp",
    "rbp", "rsi", "rdi", "r8", "r9",
    "r10", "r11", "r12", "r13", "r14", "r15"
};

const char* DWORD_REG_NAMES[] = {
    "eax", "ecx", "edx", "ebx", "esp",
    "ebp", "esi", "edi", "r8d", "r9d",
    "r10d", "r11d", "r12d", "r13d", 
    "r14d", "r15d"
};

const char* WORD_REG_NAMES[] = {
    "ax", "cx", "dx", "bx", "sp",
    "bp", "si", "di", "r8w", "r9w",
    "r10w", "r11w", "r12w", "r13w", 
    "r14w", "r15w"
};

const char* BYTE_REG_NAMES[] = {
    "al", "cl", "dl", "bl", "spl",
    "bpl", "sil", "dil", "r8b", "r9b",
    "r10b", "r11b", "r12b", "r13b", 
    "r14b", "r15b"
};

const char* regname(uint32_t reg, uint64_t size) {
    switch (size) {
    case 8:
        return QWORD_REG_NAMES[reg];
    case 4:
        return DWORD_REG_NAMES[reg];
    case 2:
        return WORD_REG_NAMES[reg];
    case 1:
        return BYTE_REG_NAMES[reg];
    }
    assert(false);
}

void serialize_operand(AsmCtx* ctx, String* dest, Amd64Operand operand) {
    switch (operand.type) {
    case OPERAND_MEM8:
    case OPERAND_MEM16:
    case OPERAND_MEM32:
    case OPERAND_MEM64:
        if (operand.type == OPERAND_MEM8) {
            String_extend(dest, "BYTE [");
        } else if (operand.type == OPERAND_MEM16) {
            String_extend(dest, "WORD [");
        } else if (operand.type == OPERAND_MEM32) {
            String_extend(dest, "DWORD [");
        } else {
            String_extend(dest, "QWORD [");
        }
        String_extend(dest, regname(operand.reg, 8));
        if (operand.scale != 0) {
            String_format_append(dest, " + %s * %u", regname(operand.scale_reg, 8),
                                 operand.scale);
        }
        if (operand.offset != 0) {
            String_format_append(dest, " + %d", operand.offset);
        }
        String_append(dest, ']');
        return;
    case OPERAND_REG8:
        String_extend(dest, regname(operand.reg, 1));
        return;
    case OPERAND_REG16:
        String_extend(dest, regname(operand.reg, 2));
        return;
    case OPERAND_REG32:
        String_extend(dest, regname(operand.reg, 4));
        return;
    case OPERAND_REG64:
        String_extend(dest, regname(operand.reg, 8));
        return;
    case OPERAND_LABEL:
        if (ctx->labels[operand.label].symbol == NULL) {
            String_format_append(dest, "L%lu", operand.label);
        } else {
            String_append_count(dest, (const char*)ctx->labels[operand.label].symbol,
                                ctx->labels[operand.label].symbol_len);
        }
        return;
    case OPERAND_GLOBAL_MEM8:
        String_format_append(dest, "BYTE [%*.*s]",
                             ctx->labels[operand.label].symbol_len,
                             ctx->labels[operand.label].symbol_len, 
                             ctx->labels[operand.label].symbol);
        return;
    case OPERAND_GLOBAL_MEM16:
        String_format_append(dest, "WORD [%*.*s]",
                             ctx->labels[operand.label].symbol_len,
                             ctx->labels[operand.label].symbol_len, 
                             ctx->labels[operand.label].symbol);
        return;
    case OPERAND_GLOBAL_MEM32:
        String_format_append(dest, "DWORD [%*.*s]",
                             ctx->labels[operand.label].symbol_len,
                             ctx->labels[operand.label].symbol_len, 
                             ctx->labels[operand.label].symbol);
        return;
    case OPERAND_GLOBAL_MEM64:
        String_format_append(dest, "QWORD [%*.*s]",
                             ctx->labels[operand.label].symbol_len,
                             ctx->labels[operand.label].symbol_len, 
                             ctx->labels[operand.label].symbol);
        return;
    case OPERAND_IMM8: {
        uint32_t v = operand.imm[0];
        String_format_append(dest, "%u", v);
        return;
    }
    case OPERAND_IMM16: {
        uint16_t v;
        memcpy(&v, operand.imm, 2);
        String_format_append(dest, "%u", v);
        return;
    }
    case OPERAND_IMM32: {
        uint32_t v;
        memcpy(&v, operand.imm, 4);
        String_format_append(dest, "%u", v);
        return;
    }
    case OPERAND_IMM64: {
        uint64_t v;
        memcpy(&v, operand.imm, 8);
        String_format_append(dest, "%llu", v);
        return;
    }
    default:
        assert(false);
    }
}

void asm_serialize(AsmCtx* ctx, String* dest) {
    Amd64Op* op = ctx->start;
    while (op != ctx->end) {
        if (op->opcode == RESERVE_MEM) {
            if (op->mem.datasize > 0) {
                String_format_append(dest, "    rb %u\n", op->mem.datasize);
            }
        } else if (op->opcode == DECLARE_MEM) {
            if (op->mem.datasize > 0) {
                String_append_count(dest, "    db ", 7);
                String_format_append(dest, "%u", op->mem.data[0]);
                for (uint64_t i = 1; i < op->mem.datasize; ++i) {
                    String_format_append(dest, ", %u", op->mem.data[i]);
                }
                String_append(dest, '\n');
            }
        } else if (op->opcode == OPCODE_LABEL) {
            const uint8_t* label = ctx->labels[op->label].symbol;
            if (label == NULL) {
                String_format_append(dest, "L%llu:\n", op->label);
            } else {
                String_append_count(dest, (const char*)label,
                        ctx->labels[op->label].symbol_len);
                String_append_count(dest, ":\n", 2);
            }
        } else if (op->opcode == OPCODE_START) {
            String_extend(dest, "format PE64 console\nentry main\n");
        } else if (op->opcode == OPCODE_RDATA_SECTION) {
            String_extend(dest, "\nsection '.rdata' data readable\n");
        } else if (op->opcode == OPCODE_DATA_SECTION) {
            String_extend(dest, "\nsection '.data' data readable writable\n");
        } else if (op->opcode == OPCODE_TEXT_SECTION) {
            String_extend(dest, "\nsection '.text' code readable executable\n");
        } else {
            assert(op->opcode < RESERVE_MEM);
            String_format_append(dest, "    %s", OP_NAMES[op->opcode]);

            if (op->count > 0) {
                String_append(dest, ' ');
                serialize_operand(ctx, dest, op->operands[0]);
                for (int i = 1; i < op->count; ++i) {
                    String_append_count(dest, ", ", 2);
                    serialize_operand(ctx, dest, op->operands[i]);
                }
            }
            String_append(dest, '\n');
        }
        op = op->next;
    }
}
