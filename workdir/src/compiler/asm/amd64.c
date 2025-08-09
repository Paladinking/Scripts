#include "mem.h"
#include "amd64.h"
#include "../utils.h"
#include "../log.h"
#include <printf.h>
#include <glob.h>
#include <coff.h>

static enum Amd64OperandType simple_operand(enum Amd64OperandType op) {
    switch (op) {
    case OPERAND_LABEL:
    case OPERAND_SYMBOL_LABEL:
        return OPERAND_IMM32;
    case OPERAND_GLOBAL_MEM8:
        return OPERAND_MEM8;
    case OPERAND_GLOBAL_MEM16:
        return OPERAND_MEM16;
    case OPERAND_GLOBAL_MEM32:
        return OPERAND_MEM32;
    case OPERAND_GLOBAL_MEM64:
        return OPERAND_MEM64;
    default:
        return op;
    }
}

// Returns the offset of <op> if allocated as large as possible
// Increments <offset> by the maximum size needed by <op>.
// Note: RVA, not file offset
// Also computes REX prefix
static uint64_t max_op_offset(AsmCtx* ctx, Amd64Op* op, uint64_t* offset) {
    assert(op->opcode < OPCODE_START);
    uint32_t count = ENCODINGS[op->opcode].count;
    const Encoding* encodings = ENCODINGS[op->opcode].encodings;

    for (uint32_t i = 0; i < count; ++i) {
        if (encodings[i].operand_count != op->count) {
            continue;
        }
        uint32_t j = 0;
        for (; j < op->count; ++j) {
            //if (op->operands[i].type == OPERAND_)
            if (simple_operand(op->operands[j].type) != encodings[i].operands[j]) {
                break;
            }
        }
        if (j < op->count) {
            continue;
        }
        const Encoding* e = &encodings[i];
        uint64_t size = 0;
        uint64_t oper_ix = 0;
        uint8_t rex = 0;
        for (uint32_t j = 0; j < e->mnemonic_count; ++j) {
            enum MnemonicPart p = e->mnemonic[j].type;
            switch (p) {
            case PREFIX:
                ++size;
                break;
            case REX:
                assert(rex == 0);
                rex = e->mnemonic[j].byte;
                ++size;
                break;
            case OPCODE:
                ++size;
                break;
            case OPCODE2:
                size += 2;
                break;
            case MOD_RM:
                ++size;
                break;
            case RM_MEM:
                if (op->operands[oper_ix].type == OPERAND_GLOBAL_MEM8 ||
                    op->operands[oper_ix].type == OPERAND_GLOBAL_MEM16 ||
                    op->operands[oper_ix].type == OPERAND_GLOBAL_MEM32 ||
                    op->operands[oper_ix].type == OPERAND_GLOBAL_MEM64) {
                    // Need 32-bit offset
                    size += 4;
                } else {
                    if (op->operands[oper_ix].reg >= R8 ||
                        (op->operands[oper_ix].scale != 0 && 
                         op->operands[oper_ix].scale_reg >= R8)
                        ) {
                        if (!rex) {
                            ++size;
                            rex = 0x40;
                        }
                        if (op->operands[oper_ix].reg >= R8) {
                            rex |= 1;
                        }
                        if (op->operands[oper_ix].scale != 0 &&
                            op->operands[oper_ix].scale_reg >= R8) {
                            rex |= 2;
                        }
                    }
                    if (op->operands[oper_ix].scale != 0 ||
                         op->operands[oper_ix].reg == RSP ||
                         op->operands[oper_ix].reg == R12) { // SIB needed
                        ++size;
                    }

                    if ((op->operands[oper_ix].reg == RBP ||
                        op->operands[oper_ix].reg == R13) &&
                        op->operands[oper_ix].offset == 0) {
                        // Need to add 0 offset anyways do to adressing mode
                        ++size;
                    } else if (op->operands[oper_ix].offset != 0) {
                        int32_t offset = op->operands[oper_ix].offset;
                        if (offset >= INT8_MIN && offset <= INT8_MAX) {
                            ++size; // 8bit offset is enough
                        } else {
                            size += 4; //32bit offset needed
                        }
                    }
                }
                ++oper_ix;
                break;
            case IMM:
            case REL_ADDR:
                size += e->mnemonic[j].byte;
                ++oper_ix;
                break;
            case REG_REG:
                if (op->operands[oper_ix].reg >= R8) {
                    if (!rex) {
                        rex = 0x40;
                        ++size;
                    }
                    rex |= 4;
                }
                ++oper_ix;
                break;
            case RM_REG:
            case PLUS_REG:
                if (op->operands[oper_ix].reg >= R8) {
                    if (!rex) {
                        rex = 0x40;
                        ++size;
                    }
                    rex |= 1;
                }
                ++oper_ix;
                break;
            case SKIP:
                ++oper_ix;
                break;
            }
        }
        op->rex = rex;
        uint64_t res = *offset;
        *offset += size;
        return res;
    }

    fatal_error(NULL, ASM_ERROR_MISSING_ENCODING, LINE_INFO_NONE);
    return 0;
}

typedef struct PendingAddr {
    Amd64Op* target;
    Amd64Op* source;
    uint64_t byte_offset;

    uint8_t size; // 1, 2 or 4
} PendingAddr;

typedef struct AddrBuf {
    PendingAddr* data;
    uint64_t size;
    uint64_t capacity;
} AddrBuf;

void AddrBuf_create(AddrBuf* buf) {
    buf->data = Mem_alloc(16 * sizeof(PendingAddr));
    if (buf->data == NULL) {
        out_of_memory(NULL);
    }
    buf->size = 0;
    buf->capacity = 16;
}

void AddrBuf_append(AddrBuf* buf, Amd64Op* target, Amd64Op* source,
                    uint64_t byte_offset, uint8_t size) {
    RESERVE(buf->data, buf->size + 1, buf->capacity);
    buf->data[buf->size] = (PendingAddr) {target, source, byte_offset, size};
    buf->size += 1;
}

void AddrBuf_free(AddrBuf* buf) {
    Mem_free(buf->data);
    buf->data = NULL;
    buf->size = 0;
    buf->capacity = 0;
}


static uint64_t assemble_op(AsmCtx* ctx, Amd64Op* op, uint64_t* offset, AddrBuf* addrs) {
    assert(op->opcode < OPCODE_START);
    Amd64Operand operands[OPERAND_MAX];
    for (uint32_t ix = 0; ix < op->count; ++ix) {
        switch (op->operands[ix].type) {
        case OPERAND_SYMBOL_LABEL:
            assert(op->opcode == OP_CALL);
            operands[ix].type = OPERAND_IMM32;
            memset(operands[ix].imm, 0, sizeof(operands[ix].imm));
            break;
        case OPERAND_LABEL: {
            uint32_t label = op->operands[ix].label;
            int64_t cur_offset = op->next->max_offset;
            Amd64Op* label_op = ctx->labels[label].op;
            while (label_op->opcode == OPCODE_LABEL) {
                label_op = label_op->next;
            }
            int64_t target_offset = label_op->max_offset;
            int64_t delta64 = target_offset - cur_offset;
            assert(delta64 >= INT32_MIN && delta64 <= INT32_MAX);
            int32_t delta = delta64;
            if (op->opcode == OP_CALL || delta < INT16_MIN || delta > INT16_MAX) {
                operands[ix].type = OPERAND_IMM32;
            } else if (delta < INT8_MIN || delta > INT8_MAX) {
                operands[ix].type = OPERAND_IMM16;
            } else {
                operands[ix].type = OPERAND_IMM8;
            }
            memset(operands[ix].imm, 0, sizeof(operands[ix].imm));
            break;
        }
        case OPERAND_GLOBAL_MEM8:
        case OPERAND_GLOBAL_MEM16:
        case OPERAND_GLOBAL_MEM32:
        case OPERAND_GLOBAL_MEM64: {
            operands[ix].type = simple_operand(op->operands[ix].type);
            operands[ix].offset = op->operands[ix].offset;
            operands[ix].reg = 255;
            operands[ix].scale = 0;
            operands[ix].scale_reg = 255;
            break;
        }
        default:
            operands[ix] = op->operands[ix];
            break;
        }
    }

    uint32_t count = ENCODINGS[op->opcode].count;
    const Encoding* encodings = ENCODINGS[op->opcode].encodings;
    Object* obj = ctx->object;
    section_ix sec = ctx->code_section;

    uint64_t last_size = obj->sections[sec].data.size;

    for (uint32_t i = 0; i < count; ++i) {
        if (encodings[i].operand_count != op->count) {
            continue;
        }
        uint32_t j = 0;
        for (; j < op->count; ++j) {
            if (operands[j].type != encodings[i].operands[j]) {
                break;
            }
        }
        if (j < op->count) {
            continue;
        }
        const Encoding* e = &encodings[i];
        uint64_t opcode_ix = -1;
        uint64_t mod_rm_ix = -1;
        uint32_t oper_ix = 0;
        for (uint32_t j = 0; j < e->mnemonic_count; ++j) {
            enum MnemonicPart p = e->mnemonic[j].type;
            switch (p) {
            case PREFIX:
                Object_append_byte(obj, sec, e->mnemonic[j].byte);
                break;
            case REX:
                // Rex pre-calculated, added with opcode
                break;
            case OPCODE:
                assert(opcode_ix == ((uint64_t)-1));
                if (op->rex != 0) {
                    Object_append_byte(obj, sec, op->rex);
                }
                opcode_ix = obj->sections[sec].data.size;
                Object_append_byte(obj, sec, e->mnemonic[j].byte);
                break;
            case OPCODE2:
                assert(opcode_ix == ((uint64_t)-1));
                if (op->rex != 0) {
                    Object_append_byte(obj, sec, op->rex);
                }
                Object_append_byte(obj, sec, 0x0f);
                opcode_ix = obj->sections[sec].data.size;
                Object_append_byte(obj, sec, e->mnemonic[j].byte);
                break;
            case MOD_RM:
                assert(mod_rm_ix == ((uint64_t)-1));
                mod_rm_ix = obj->sections[sec].data.size;
                Object_append_byte(obj, sec, e->mnemonic[j].byte);
                break;
            case RM_MEM: {
                assert(mod_rm_ix != ((uint64_t)-1));
                if (operands[oper_ix].reg == 255) { // Global memory
                    // ModRm.mod = 0, ModRm.r/m = 101
                    symbol_ix symbol = op->operands[oper_ix].symbol;
                    obj->sections[sec].data.data[mod_rm_ix] |= 0x5;
                    Object_add_reloc(obj, sec, symbol, RELOCATE_REL32);
                    const uint8_t* b = (const uint8_t*)&operands[oper_ix].offset;
                    Object_append_data(obj, sec, b, 4);
                } else {
                    if (operands[oper_ix].scale != 0 ||
                         operands[oper_ix].reg == RSP ||
                         operands[oper_ix].reg == R12) { // SIB needed
                        // ModRm.r/m = 100
                        obj->sections[sec].data.data[mod_rm_ix] |= 0x4;
                        assert(operands[oper_ix].scale == 0 || 
                               operands[oper_ix].scale_reg != RSP);

                        // SIB.index = scale_reg, SIB.base = reg
                        uint8_t sib = (operands[oper_ix].scale_reg & 0x7) << 3;
                        sib |= (operands[oper_ix].reg & 0x7);
                        if (operands[oper_ix].scale == 0) {
                            // SIB.scale = 00, SIB.index = 100, SIB.base = reg
                            sib = 0x20 | (operands[oper_ix].reg & 0x7);
                        } else if (operands[oper_ix].scale == 2) {
                            // SIB.scale = 01
                            sib |= 0x40;
                        } else if (operands[oper_ix].scale == 4) {
                            // SIB.scale = 10
                            sib |= 0x80;
                        } else if (operands[oper_ix].scale == 8) {
                            // SIB.scale = 11
                            sib |= 0xc0;
                        }
                        Object_append_byte(obj, sec, sib);
                    } else {
                        // ModRm.r/m = reg
                        obj->sections[sec].data.data[mod_rm_ix] |= 
                            (operands[oper_ix].reg & 0x7);
                    }

                    if (operands[oper_ix].offset != 0 ||
                        operands[oper_ix].reg == RBP || operands[oper_ix].reg == R13) {
                        uint8_t* b = (uint8_t*)&operands[oper_ix].offset;
                        if (operands[oper_ix].offset >= INT8_MIN &&
                            operands[oper_ix].offset <= INT8_MAX) {
                            // ModRm.mod = 01
                            obj->sections[sec].data.data[mod_rm_ix] |= 0x40;
                            Object_append_data(obj, sec, b, 1);
                        } else {
                            // ModRm.mod = 10
                            obj->sections[sec].data.data[mod_rm_ix] |= 0x80;
                            Object_append_data(obj, sec, b, 4);
                        }
                    }
                }
                ++oper_ix;
                break;
            }
            case IMM:
                Object_append_data(obj, sec, operands[oper_ix].imm,
                                   e->mnemonic[j].byte);
                ++oper_ix;
                break;
            case REL_ADDR: {
                if (op->operands[oper_ix].type == OPERAND_LABEL) {
                    uint32_t label = op->operands[oper_ix].label;
                    Amd64Op* label_op = ctx->labels[label].op;
                    while (label_op->opcode == OPCODE_LABEL) {
                        label_op = label_op->next;
                    }
                    uint64_t size = obj->sections[sec].data.size;
                    AddrBuf_append(addrs, label_op, op->next, size,
                                   e->mnemonic[j].byte);
                } else {
                    assert(op->operands[oper_ix].type == OPERAND_SYMBOL_LABEL);
                    symbol_ix symbol = op->operands[oper_ix].symbol;
                    Object_add_reloc(obj, sec, symbol, RELOCATE_REL32);
                    assert(e->mnemonic[j].byte == 4);
                }
                Object_append_data(obj, sec, operands[oper_ix].imm,
                                   e->mnemonic[j].byte);
                ++oper_ix;
                break;
            }
            case REG_REG:
                assert(mod_rm_ix != ((uint64_t)-1));
                obj->sections[sec].data.data[mod_rm_ix] |= 
                    ((op->operands[oper_ix].reg & 0x7) << 3);
                ++oper_ix;
                break;
            case RM_REG:
                assert(mod_rm_ix != ((uint64_t)-1));
                obj->sections[sec].data.data[mod_rm_ix] |= 
                    0xc0 | (op->operands[oper_ix].reg & 0x7);
                ++oper_ix;
                break;
            case PLUS_REG:
                assert(opcode_ix != ((uint64_t) -1));
                obj->sections[sec].data.data[opcode_ix] |=
                    (op->operands[oper_ix].reg & 0x7);
                ++oper_ix;
                break;
            case SKIP:
                ++oper_ix;
                break;
            }
        }
        uint64_t new_size = obj->sections[sec].data.size;
        assert(new_size > last_size);
        uint64_t res = *offset;
        *offset += new_size - last_size;

        return res;
    }

    assert(false);
}

Amd64Op* create_op(AsmCtx* ctx, enum Amd64Opcode opcode) {
    Amd64Op* op = Arena_alloc_type(&ctx->arena, Amd64Op);
    op->next = NULL;
    op->prev = NULL;
    op->opcode = opcode;
    op->count = 0;
    op->max_offset = 0;
    op->rex = 0;
    op->offset = 0;
    return op;
}


void asm_ctx_create(AsmCtx* ctx, Object* object, section_ix code_section) {
    if (!Arena_create(&ctx->arena, 0xffffffff, out_of_memory, NULL)) {
        LOG_CRITICAL("Failed creating arena");
        fatal_error(NULL, PARSE_ERROR_NONE, LINE_INFO_NONE);
        return;
    }
    ctx->start = create_op(ctx, OPCODE_START);
    ctx->end = create_op(ctx, OPCODE_END);

    ctx->start->next = ctx->end;
    ctx->end->prev = ctx->start;

    ctx->label_count = 0;
    ctx->label_cap = 32;
    ctx->labels = Mem_alloc(32 * sizeof(LabelData));
    if (ctx->labels == NULL) {
        out_of_memory(NULL);
    }

    ctx->object = object;
    ctx->code_section = code_section;
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
    assert(ix <= UINT32_MAX);
    Amd64Op* op = ctx->end;
    op->operands[op->count].type = OPERAND_LABEL;
    op->operands[op->count++].label = ix;
}


void asm_symbol_label_var(AsmCtx* ctx, symbol_ix symbol) {
    Amd64Op* op = ctx->end;
    op->operands[op->count].type = OPERAND_SYMBOL_LABEL;
    op->operands[op->count++].symbol = symbol;
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

void asm_global_mem_var(AsmCtx* ctx, uint8_t size, symbol_ix symbol,
                        int32_t offset) {
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
    // Sanity check, not actually needed
    assert(offset < 65535 && offset + 65535 > 0);

    op->operands[op->count].offset = offset;
    op->operands[op->count++].symbol = symbol;
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

    ctx->end = create_op(ctx, OPCODE_END);
    ctx->end->prev = op;

    op->next = ctx->end;
}

void asm_put_label(AsmCtx* ctx, uint64_t label) {
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
            ctx->labels[i].op = NULL;
        }
        ctx->label_count = label + 1;
    }
    ctx->labels[label].op = ctx->end;

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
        String_format_append(dest, "L%lu", operand.label);
        return;
    case OPERAND_SYMBOL_LABEL: {
        const uint8_t* n = ctx->object->symbols[operand.symbol].name;
        uint32_t l = ctx->object->symbols[operand.symbol].name_len;
        String_format_append(dest, "%*.*s", l, l, n);
        return;
    }
    case OPERAND_GLOBAL_MEM8: {
        const uint8_t* n = ctx->object->symbols[operand.symbol].name;
        uint32_t l = ctx->object->symbols[operand.symbol].name_len;
        String_format_append(dest, "BYTE [%*.*s]", l, l, n);
        return;
    }
    case OPERAND_GLOBAL_MEM16: {
        const uint8_t* n = ctx->object->symbols[operand.symbol].name;
        uint32_t l = ctx->object->symbols[operand.symbol].name_len;
        String_format_append(dest, "WORD [%*.*s]", l, l, n);
        return;
    }
    case OPERAND_GLOBAL_MEM32: {
        const uint8_t* n = ctx->object->symbols[operand.symbol].name;
        uint32_t l = ctx->object->symbols[operand.symbol].name_len;
        String_format_append(dest, "DWORD [%*.*s]", l, l, n);
        return;
    }
    case OPERAND_GLOBAL_MEM64: {
        const uint8_t* n = ctx->object->symbols[operand.symbol].name;
        uint32_t l = ctx->object->symbols[operand.symbol].name_len;
        String_format_append(dest, "QWORD [%*.*s]", l, l, n);
        return;
    }
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
    for (; op != ctx->end; op = op->next) {
        if (op->opcode == OPCODE_START) {
            const uint8_t* name = ctx->object->symbols[op->symbol].name;
            uint32_t name_len = ctx->object->symbols[op->symbol].name_len;
            String_format_append(dest, "%*.*s:\n", name_len, name_len, name);
            continue;
        }
        if (op->opcode == OPCODE_LABEL) {
            String_format_append(dest, "L%u:\n", op->label);
            continue;
        }
        assert(op->opcode < OPCODE_START);
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
}

void asm_assemble(AsmCtx* ctx, symbol_ix symbol) {
    ctx->start->symbol = symbol;
    uint64_t offset = 0;
    Amd64Op* op;
    for (op = ctx->start->next; op->opcode != OPCODE_END; op = op->next) {
        if (op->opcode == OPCODE_LABEL) {
            op->max_offset = offset;
            continue;
        }
        uint64_t off = max_op_offset(ctx, op, &offset);
        assert(off <= UINT32_MAX);
        op->max_offset = off;
    }

    LOG_WARNING("Done computing offsets");

    AddrBuf addrs;
    AddrBuf_create(&addrs);

    offset = 0;
    for (op = ctx->start->next; op->opcode != OPCODE_END; op = op->next) {
        if (op->opcode == OPCODE_LABEL) {
            op->offset = offset;
            continue;
        }
        uint64_t off = assemble_op(ctx, op, &offset, &addrs);
        assert(off <= UINT32_MAX);
        op->offset = off;
    }

    ByteBuffer* buf = &ctx->object->sections[ctx->code_section].data;

    for (uint64_t i = 0; i < addrs.size; ++i) {
        int64_t source = addrs.data[i].source->offset;
        int64_t target = addrs.data[i].target->offset;

        int64_t delta = (target - source);
        if (addrs.data[i].size == 1) {
            assert(delta >= INT8_MIN && delta <= INT8_MAX);
            int8_t d = delta;
            memcpy(buf->data + addrs.data[i].byte_offset, &d, 1);
        } else if (addrs.data[i].size == 2) {
            assert(delta >= INT16_MIN && delta <= INT16_MAX);
            int16_t d = delta;
            memcpy(buf->data + addrs.data[i].byte_offset, &d, 2);
        } else {
            assert(addrs.data[i].size == 4);
            assert(delta >= INT32_MIN && delta <= INT32_MAX);
            int32_t d = delta;
            memcpy(buf->data + addrs.data[i].byte_offset, &d, 4);
        }
    }

    AddrBuf_free(&addrs);

    LOG_WARNING("Done assembly: %llu bytes", buf->size);
}

void asm_reset(AsmCtx* ctx) {
    ctx->start = ctx->end;

    asm_instr(ctx, OPCODE_START);
    asm_instr_end(ctx);

}
