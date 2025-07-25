#include "amd64_asm.h"
#include <printf.h>
#include "format.h"

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

#define ALLIGN_TO(i, size) if (((i) % (size)) != 0) { \
    (i) = (i) + ((size) - ((i) % (size)));            \
}

const uint8_t GEN_CALL_REGS[4] = {RCX, RDX, R8, R9};
const bool GEN_VOLATILE_REGS[GEN_REG_COUNT] = {
    true, true, true, false, false, false, false, false,
    true, true, true, true, false, false, false, false
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
    case QUAD_64_BIT:
        return QWORD_REG_NAMES[reg];
    case QUAD_32_BIT:
        return DWORD_REG_NAMES[reg];
    case QUAD_16_BIT:
        return WORD_REG_NAMES[reg];
    case QUAD_8_BIT:
        return BYTE_REG_NAMES[reg];
    }
    assert(false);
}


uint64_t Backend_get_regs() {
    return GEN_REG_COUNT;
}

void Backend_inital_contraints(ConflictGraph* graph, var_id var_count) {
    ConflictGraph_clear(graph);

    // Add edges between all registers
    for (uint64_t col = 0; col < graph->reg_count; ++col){
        for (uint64_t row = col + 1; row < graph->reg_count; ++row) {
            ConflictGraph_add_edge(graph, row, col);
        }
    }

    for (uint64_t ix = 0; ix < graph->var_count; ++ix) {
        // Do not allow use of stack pointer
        ConflictGraph_add_edge(graph, RSP, ix + graph->reg_count);
    }
}

// Require <var> uses <reg>
void requre_gen_reg(ConflictGraph* graph, uint64_t reg, var_id var) {
    for (uint64_t ix = 0; ix < graph->reg_count; ++ix) {
        if (reg != ix) {
            ConflictGraph_add_edge(graph, var + graph->reg_count, ix);
        }
    }
}

// Require <var> uses memory
void require_mem(ConflictGraph* graph, var_id var) {
    for (uint64_t ix = 0; ix < graph->reg_count; ++ix) {
        ConflictGraph_add_edge(graph, var + graph->reg_count, ix);
    }
}

uint64_t move_type(var_id var, VarList* vars) {
    // TODO: this is not good, should read type_table instead
    if (vars->data[var].type == TYPE_ID_FLOAT64) {
        return QUAD_64_BIT | QUAD_FLOAT;
    }
    if (vars->data[var].byte_size == 1) {
        return QUAD_8_BIT | QUAD_UINT;
    } else if (vars->data[var].byte_size == 2) {
        return QUAD_16_BIT | QUAD_UINT;
    } else if (vars->data[var].byte_size == 4) {
        return QUAD_32_BIT | QUAD_UINT;
    } else if (vars->data[var].byte_size == 8) {
        return QUAD_64_BIT | QUAD_UINT;
    }
    assert(false);
}

void preinsert_move(ConflictGraph* graph, var_id* var, VarSet* live_set,
                    Quad* quad, FlowNode* node, VarList* vars, Arena* arena) {

    var_id tmp = create_temp_var(graph, vars, node, live_set, *var);
    ConflictGraph_add_edge(graph, RSP, tmp + graph->reg_count);
    Quad* nq = Arena_alloc_type(arena, Quad);
    nq->type = QUAD_MOVE | move_type(*var, vars);
    nq->op1.var = *var;
    nq->dest = tmp;
    nq->last_quad = quad->last_quad;
    nq->next_quad = quad;
    if (quad == node->start) {
        node->start = nq;
    }
    if (quad->last_quad != NULL) {
        quad->last_quad->next_quad = nq;
    }
    quad->last_quad = nq;
    *var = tmp;
    vars->data[tmp].alloc_type = ALLOC_REG;
    LOG_DEBUG("Inserted move before quad: %llu", tmp);
}

void postinsert_move(ConflictGraph* graph, var_id* dest, VarSet* live_set,
                     Quad* quad, FlowNode* node, VarList* vars, Arena* arena) {
    var_id tmp = create_temp_var(graph, vars, node, live_set, *dest);
    ConflictGraph_add_edge(graph, RSP, tmp + graph->reg_count);
    Quad* nq = Arena_alloc_type(arena, Quad);
    nq->type = QUAD_MOVE | move_type(*dest, vars);
    nq->op1.var = tmp;
    nq->dest = *dest;
    nq->last_quad = quad;
    nq->next_quad = quad->next_quad;
    if (quad == node->end) {
        node->end = nq;
    }
    if (quad->next_quad != NULL) {
        quad->next_quad->last_quad = nq;
    }
    quad->next_quad = nq;
    *dest = tmp;

    var_id v = VarSet_get(live_set);
    while (v != VAR_ID_INVALID) {
        if (v != nq->dest) {
            ConflictGraph_add_edge(graph, v + graph->reg_count, 
                                   tmp + graph->reg_count);
        }
        v = VarSet_getnext(live_set, v);
    }

    vars->data[tmp].alloc_type = ALLOC_REG;
    LOG_DEBUG("Inserted move after quad: %llu", tmp);
}

void requre_gen_reg_var(ConflictGraph* graph, uint64_t reg, var_id* var, 
                        VarSet* live_set, Quad* quad, FlowNode* node,
                        VarList* vars, Arena* arena) {
    if (vars->data[*var].alloc_type == ALLOC_MEM) {
        preinsert_move(graph, var, live_set, quad, node, vars, arena);
    }
    requre_gen_reg(graph, reg, *var);
}

void requre_gen_reg_dest(ConflictGraph* graph, uint64_t reg, var_id* dest, 
                        VarSet* live_set, Quad* quad, FlowNode* node,
                        VarList* vars, Arena* arena) {
    if (vars->data[*dest].alloc_type == ALLOC_MEM) {
        postinsert_move(graph, dest, live_set, quad, node, vars, arena);
    }
    requre_gen_reg(graph, reg, *dest);
}


// All integer binary operations that are simple in amd64 asm
bool is_standard_binop(uint64_t type) {
    if (type & QUAD_FLOAT) {
        return false;
    }
    enum QuadType t = type & QUAD_TYPE_MASK;
    switch (t) {
    case QUAD_MUL:
        return (type & QUAD_DATATYPE_MASK) == QUAD_SINT;
    case QUAD_SUB:
    case QUAD_ADD:
    case QUAD_BIT_AND:
    case QUAD_BIT_OR:
    case QUAD_BIT_XOR:
        return true;
    default:
        return false;
    }
}

bool is_symetrical(uint64_t type) {
    enum QuadType t = type & QUAD_TYPE_MASK;
    switch (t) {
    case QUAD_ADD:
    case QUAD_MUL:
    case QUAD_BIT_AND:
    case QUAD_BIT_OR:
    case QUAD_BIT_XOR:
    case QUAD_CMP_EQ:
    case QUAD_CMP_NEQ:
        return true;
    default:
        return false;
    }
}

bool is_cmp(uint64_t type) {
    enum QuadType t = type & QUAD_TYPE_MASK;
    switch (t) {
    case QUAD_CMP_LE:
    case QUAD_CMP_L:
    case QUAD_CMP_GE:
    case QUAD_CMP_G:
    case QUAD_CMP_EQ:
    case QUAD_CMP_NEQ:
        return true;
    default:
        return false;
    }
}

bool allow_op2_imm(uint64_t type) {
    if (is_standard_binop(type)) {
        return true;
    }
    if (type & QUAD_FLOAT) {
        return false;
    }
    if (is_cmp(type)) {
        return true;
    }
    enum QuadType t = type & QUAD_TYPE_MASK;
    return t == QUAD_RSHIFT || t == QUAD_LSHIFT;
}

bool allow_op1_imm(uint64_t type) {
    if (type & QUAD_FLOAT) {
        return false;
    }
    enum QuadType t = type & QUAD_TYPE_MASK;
    return t == QUAD_CAST_TO_INT64 ||
           t == QUAD_CAST_TO_UINT64 ||
           t == QUAD_CAST_TO_BOOL;
}


bool fully_local(VarList* list, var_id v) {
    return list->data[v].reads == 1 && list->data[v].writes == 1 &&
           list->data[v].kind == VAR_TEMP;
}

void Backend_add_constrains(ConflictGraph* graph, VarSet* live_set, Quad* quad,
                            VarList* vars, FlowNode* node, Arena* arena) {
    enum QuadType q = quad->type & QUAD_TYPE_MASK;
    uint64_t datatype = quad->type & QUAD_DATATYPE_MASK;

    assert(datatype != QUAD_FLOAT);

    if (is_standard_binop(quad->type)) {
        if (vars->data[quad->dest].alloc_type == ALLOC_MEM &&
            vars->data[quad->op2].alloc_type == ALLOC_MEM) {
            // Two memory ops not allowed, insert move
            preinsert_move(graph, &quad->op2, live_set, quad, node, vars, arena);
        }

        if (quad->dest != quad->op2 && quad->dest != quad->op1.var) {
            ConflictGraph_add_edge(graph, quad->dest + graph->reg_count,
                    quad->op2 + graph->reg_count);
        }
    } else if (q == QUAD_BOOL_NOT) {
        // cmp + setz
    } else if (q == QUAD_SET_ADDR) {
        if (vars->data[quad->op1.var].alloc_type == ALLOC_MEM) {
            preinsert_move(graph, &quad->op1.var, live_set, quad, node, vars, arena);
        }
        if (vars->data[quad->op2].alloc_type == ALLOC_MEM) {
            preinsert_move(graph, &quad->op2, live_set, quad, node, vars, arena);
        }
    } else if (q == QUAD_CALC_ADDR || q == QUAD_GET_ADDR) {
        if (vars->data[quad->op2].alloc_type == ALLOC_MEM) {
            preinsert_move(graph, &quad->op2, live_set, quad, node, vars, arena);
        }
        if (vars->data[quad->dest].alloc_type == ALLOC_MEM) {
            postinsert_move(graph, &quad->dest, live_set, quad, node, vars, arena);
        }
        if (quad->dest != quad->op2 && quad->dest != quad->op1.var) {
            ConflictGraph_add_edge(graph, quad->dest + graph->reg_count,
                                   quad->op2 + graph->reg_count);
        }
    } else if (q == QUAD_RSHIFT || q == QUAD_LSHIFT) {
        requre_gen_reg_var(graph, RCX, &quad->op2, live_set, quad, node, 
                           vars, arena);
        if (quad->dest != quad->op2) {
            ConflictGraph_add_edge(graph, quad->dest + graph->reg_count,
                    quad->op2 + graph->reg_count);
        }
        var_id v = VarSet_get(live_set);
        while (v != VAR_ID_INVALID) {
            if (v != quad->dest) {
                ConflictGraph_add_edge(graph, v + graph->reg_count, RCX);
            }
            v = VarSet_getnext(live_set, v);
        }
    } else if (q == QUAD_CREATE) {
        if (quad->next_quad == NULL || !fully_local(vars, quad->dest)) {
            return;
        }
        uint64_t data_size = quad->type & QUAD_DATASIZE_MASK;
        uint64_t next = quad->next_quad->type;
        bool good_datatype = false;
        if (datatype == QUAD_SINT) {
            good_datatype = data_size > QUAD_64_BIT || 
                (quad->op1.sint64 <= INT32_MAX &&
                 quad->op1.sint64 >= INT32_MIN);
        } else if (datatype == QUAD_UINT) {
            good_datatype = data_size > QUAD_16_BIT ||
                quad->op1.uint64 <= INT32_MAX;
        } else if (datatype == QUAD_BOOL) {
            good_datatype = true;
        }
        if (!good_datatype) {
            return;
        }
        if (is_cmp(next) && vars->data[quad->next_quad->dest].alloc_type != ALLOC_IMM) {
            // cmp into value generates conditional moves, needs registers
            return;
        }
        if ((allow_op2_imm(next) && (quad->next_quad->op2 == quad->dest ||
             is_symetrical(next) && quad->next_quad->op1.var == quad->dest)) ||
             allow_op1_imm(next) && quad->next_quad->op1.var == quad->dest) {
            if (vars->data[quad->dest].alloc_type == ALLOC_NONE) {
                vars->data[quad->dest].alloc_type = ALLOC_IMM;
                assert((next & QUAD_DATATYPE_MASK) == datatype);
                if (datatype == QUAD_SINT) {
                    LOG_DEBUG("Created imm singed %lld", quad->op1.sint64);
                    vars->data[quad->dest].imm.int64 = quad->op1.sint64;
                } else if (datatype == QUAD_UINT) {
                    LOG_DEBUG("Created imm unsinged %llu, %llu", quad->op1.uint64,
                               quad->dest);
                    vars->data[quad->dest].imm.uint64 = quad->op1.uint64;
                } else {
                    LOG_DEBUG("Created imm boolean %u", quad->op1.uint64);
                    vars->data[quad->dest].imm.boolean = quad->op1.uint64 != 0;
                }
            }
        }
    } else if (q == QUAD_GET_ARG && quad->op1.uint64 < 4) {
        uint64_t reg = GEN_CALL_REGS[quad->op1.uint64];
        requre_gen_reg_dest(graph, reg, &quad->dest, live_set, quad, node, 
                            vars, arena);
    } else if (q == QUAD_CMP_EQ || q == QUAD_CMP_NEQ || q == QUAD_CMP_G ||
        q == QUAD_CMP_L || q == QUAD_CMP_GE || q == QUAD_CMP_LE) {

        // If next quad is JMP_TRUE or JMP_FALSE, and uses quad->dest
        // and quad->dest is only used here, make it ALLOC_IMM
        if (quad->next_quad != NULL) {
            enum QuadType next = quad->next_quad->type & QUAD_TYPE_MASK;
            if ((next == QUAD_JMP_TRUE || next == QUAD_JMP_FALSE) &&
                quad->next_quad->op2 == quad->dest &&
                fully_local(vars, quad->dest)) {
                vars->data[quad->dest].alloc_type = ALLOC_IMM;

            }
        }
        if (vars->data[quad->op1.var].alloc_type == ALLOC_MEM &&
            vars->data[quad->op2].alloc_type == ALLOC_MEM) {
            preinsert_move(graph, &quad->op1.var, live_set, quad, 
                           node, vars, arena);
        }
        if (vars->data[quad->dest].alloc_type != ALLOC_IMM) {
            // Need at least 2 different register for this
            if (vars->data[quad->dest].alloc_type == ALLOC_MEM) {
                postinsert_move(graph, &quad->dest, live_set, quad, node, 
                                vars, arena);
            }
            if (quad->dest == quad->op1.var ||
                vars->data[quad->op1.var].alloc_type == ALLOC_MEM) {
                preinsert_move(graph, &quad->op1.var, live_set, quad, node,
                               vars, arena);
            }
            ConflictGraph_add_edge(graph, quad->dest + graph->reg_count,
                                   quad->op1.var + graph->reg_count);
        }
    } else if ((q == QUAD_MUL && datatype == QUAD_UINT) ||
        q == QUAD_DIV || q == QUAD_MOD) {
        // Make sure RAX and RDX are free for MUL / DIV / IDIV
        var_id v = VarSet_get(live_set);
        while (v != VAR_ID_INVALID) {
            if (v != quad->dest) {
                ConflictGraph_add_edge(graph, v + graph->reg_count, RAX);
                ConflictGraph_add_edge(graph, v + graph->reg_count, RDX);
            }
            v = VarSet_getnext(live_set, v);
        }
        if (quad->dest != quad->op2) {
            ConflictGraph_add_edge(graph, quad->dest + graph->reg_count,
                    quad->op2 + graph->reg_count);
        }
        if (q == QUAD_DIV || q == QUAD_MOD) {
            // operand 1 has to be in RAX
            requre_gen_reg_var(graph, RAX, &quad->op1.var, live_set, quad, 
                               node, vars, arena);
            // operand 2 cannot be in RDX
            ConflictGraph_add_edge(graph, quad->op2 + graph->reg_count, RDX);
        }
        if (q == QUAD_MOD) {
            // Mod result is in RDX
            requre_gen_reg_dest(graph, RDX, &quad->dest, live_set, quad, 
                               node, vars, arena);
        } else {
            // MUL / DIV / IDIV result is in RAX
            requre_gen_reg_dest(graph, RAX, &quad->dest, live_set, quad, 
                               node, vars, arena);
        }
    } else if (q == QUAD_CALL) {
        var_id v = VarSet_get(live_set);
        while (v != VAR_ID_INVALID) {
            ConflictGraph_add_edge(graph, v + graph->reg_count, RAX);
            ConflictGraph_add_edge(graph, v + graph->reg_count, RCX);
            ConflictGraph_add_edge(graph, v + graph->reg_count, RDX);
            ConflictGraph_add_edge(graph, v + graph->reg_count, R8);
            ConflictGraph_add_edge(graph, v + graph->reg_count, R9);
            ConflictGraph_add_edge(graph, v + graph->reg_count, R10);
            ConflictGraph_add_edge(graph, v + graph->reg_count, R11);
            v = VarSet_getnext(live_set, v);
        }
    } else if (q == QUAD_PUT_ARG && quad->op1.uint64 < 4) {
        // The first four arguments are passed in registers
        var_id v = VarSet_get(live_set);
        uint64_t reg = GEN_CALL_REGS[quad->op1.uint64];
        while (v != VAR_ID_INVALID) {
            if (v != quad->op2) {
                ConflictGraph_add_edge(graph, v + graph->reg_count, reg);
            }
            v = VarSet_getnext(live_set, v);
        }
        requre_gen_reg_var(graph, reg, &quad->op2, live_set, quad, 
                           node, vars, arena);
    } else if (q == QUAD_GET_RET) {
        requre_gen_reg_dest(graph, RAX, &quad->dest, live_set, quad, 
                            node, vars, arena);
    } else if (q == QUAD_ADDROF) {
        if (vars->data[quad->dest].alloc_type == ALLOC_MEM) {
            postinsert_move(graph, &quad->dest, live_set, quad, node, vars, arena);
        }
        require_mem(graph, quad->op1.var);
    } else if (q == QUAD_DEREF) {
        if (vars->data[quad->op1.var].alloc_type == ALLOC_MEM) {
            preinsert_move(graph, &quad->op1.var, live_set, quad, node, vars, arena);
        }
        if (vars->data[quad->dest].alloc_type == ALLOC_MEM) {
            postinsert_move(graph, &quad->dest, live_set, quad, node, vars, arena);
        }
    } else if (q == QUAD_RETURN) {
        // Readd this once allocation is better.
        // For now extra move is probably cheaper than a likely spill
        //requre_gen_reg_var(graph, RAX, &quad->op1.var, live_set, quad, 
        //                   node, vars, arena);
    } else if (q == QUAD_CAST_TO_INT64 || q == QUAD_CAST_TO_UINT64 ||
               q == QUAD_CAST_TO_BOOL || q == QUAD_MOVE) {
        // Pass?
    } else if (q == QUAD_LABEL || q == QUAD_JMP || q == QUAD_JMP_FALSE ||
               q == QUAD_JMP_TRUE) {
        // Pass
    } else {
        String out;
        String_create(&out);
        fmt_quad(quad, &out);
        _printf_e("Not implemented: %s\n", out.buffer);
        assert(false && "Not implemented");
    }
}

const char* sizestr(uint64_t mask) {
    switch (mask & QUAD_DATASIZE_MASK) {
    case QUAD_64_BIT:
        return "QWORD";
    case QUAD_32_BIT:
        return "DWORD";
    case QUAD_16_BIT:
        return "WORD";
    case QUAD_8_BIT:
        return "BYTE";
    }
    assert(false);
}

bool is_same(VarData* vars, var_id a, var_id b) {
    if (a == b) {
        return true;
    }
    if (vars[a].alloc_type == ALLOC_REG &&
        vars[b].alloc_type == ALLOC_REG) {
        return vars[a].reg == vars[b].reg;
    }
    return false;
}

void emit_instr_name(const char* instr) {
    _printf("    %s", instr);
}

void emit_label(label_id label) {
    _printf("L%llu:\n", label);
}

void emit_instr_end() {
    outputa("\n", 1);
}

void emit_reg(uint32_t reg, uint64_t datasize) {
    _printf("%s", regname(reg, datasize));
}

#define outputsep(ix) if (ix == 0) { outputa(" ", 1); } else {outputa(", ", 2); }

void emit_var_endlabel(const uint8_t* name, uint32_t name_len, uint32_t ix) {
    outputsep(ix);
    _printf("%.*s_end", name_len, name);
}

void emit_var_reg(uint32_t reg, uint64_t datasize, uint32_t ix) {
    outputsep(ix);
    _printf("%s", regname(reg, datasize));
}

void emit_var_label(label_id label, uint32_t ix) {
    outputsep(ix);
    _printf("L%llu", label);
}

void emit_var_fn(const uint8_t* name, uint32_t name_len, uint32_t ix) {
    outputsep(ix);
    _printf("%.*s", name_len, name);

}

void emit_var_uint(uint64_t var, uint32_t ix) {
    outputsep(ix);
    _printf("%llu", var);
}


void emit_var_sint(int64_t var, uint32_t ix) {
    outputsep(ix);
    _printf("%lld", var);
}

void emit_var_scaled_ix(uint64_t scale, uint32_t base_reg, uint32_t offset_reg,
                        uint64_t ixsize, uint64_t destsize, uint64_t ix) {
    outputsep(ix);
    _printf("%s [", sizestr(destsize));
    emit_reg(base_reg, QUAD_PTR_SIZE);
    outputa(" + ", 3);
    if (scale == QUAD_SCALE_8) {
        outputa("8 * ", 4);
    } else if (scale == QUAD_SCALE_4) {
        outputa("4 * ", 4);
    } else if (scale == QUAD_SCALE_2) {
        outputa("2 * ", 4);
    }
    emit_reg(offset_reg, ixsize);
    outputa("]", 1);
}

void emit_var_deref(uint32_t base_reg, uint64_t datasize, uint32_t ix) {
    outputsep(ix);
    _printf("%s [", sizestr(datasize));
    emit_reg(base_reg, QUAD_PTR_SIZE);
    outputa("]", 1);
}

void emit_var(VarData* var, uint64_t datatype, uint64_t datasize, uint32_t ix) {
    outputsep(ix);
    if (var->alloc_type == ALLOC_MEM) {
        _printf("%s [rsp + %lu]", sizestr(datasize), var->memory.offset);
    } else if (var->alloc_type == ALLOC_REG) {
        _printf("%s", regname(var->reg, datasize));
    } else if (var->alloc_type == ALLOC_IMM) {
        if (datatype == QUAD_SINT) {
            LOG_DEBUG("Signed imm %lld", var->imm.int64);
            _printf("%lld", var->imm.int64);
        } else if (datatype == QUAD_UINT) {
            LOG_DEBUG("Signed imm %llu", var->imm.uint64);
            _printf("%llu", var->imm.uint64);
        } else if (datatype == QUAD_BOOL) {
            _printf("%u", var->imm.boolean ? 1 : 0);
        } else {
            assert(false);
        }
    } else {
        assert(false);
    }
}

void emit_op1_dest_move(Quad* quad, VarData* vars) {
    uint64_t datatype = quad->type & QUAD_DATATYPE_MASK;
    uint64_t datasize = quad->type & QUAD_DATASIZE_MASK;
    VarData* op1 = &vars[quad->op1.var];
    VarData* dest = &vars[quad->dest];
    emit_instr_name("mov");
    emit_var(dest, datatype, datasize, 0);
    emit_var(op1, datatype, datasize, 1);
    emit_instr_end();
}

void emit_binop(const char* name, Quad* quad, VarData* vars) {
    uint64_t datatype = quad->type & QUAD_DATATYPE_MASK;
    uint64_t datasize = quad->type & QUAD_DATASIZE_MASK;
    VarData* op1 = &vars[quad->op1.var];
    VarData* op2 = &vars[quad->op2];
    VarData* dest = &vars[quad->dest];
    if (op1->alloc_type == ALLOC_IMM) {
        assert(is_symetrical(quad->type));
        VarData* tmp = op1;
        op1 = op2;
        op2 = tmp;
    }
    assert(dest->alloc_type == ALLOC_MEM || dest->alloc_type == ALLOC_REG);
    assert(op1->alloc_type == ALLOC_MEM || op1->alloc_type == ALLOC_REG);
    if (!is_same(vars, quad->op1.var, quad->dest)) {
        if (is_symetrical(quad->type) && 
            is_same(vars, quad->op2, quad->dest)) {
            VarData* tmp = op1;
            op1 = op2;
            op2 = tmp;
        } else {
            emit_op1_dest_move(quad, vars);
        }
    }
    assert(dest->alloc_type != ALLOC_MEM || op2->alloc_type != ALLOC_MEM);
    emit_instr_name(name);
    emit_var(dest, datatype, datasize, 0);
    emit_var(op2, datatype, datasize, 1);
    emit_instr_end();
}

#define assert_in_reg(v, vars, r) assert((vars)[(v)].alloc_type == ALLOC_REG && \
        (vars)[(v)].reg == r)

void Backend_generate_fn(FunctionDef* def, Arena* arena,
                         NameTable* name_table, FunctionTable* func_table) {
    VarList* var_list = &def->vars;
    VarData* vars = var_list->data;
    assert((def->quad_start->type & QUAD_TYPE_MASK) == QUAD_LABEL);
    const uint8_t* name = name_table->data[def->name].name;
    uint32_t name_len = name_table->data[def->name].name_len;
    _printf(";funtion '%.*s'\n", name_len, name);
    outputa((const char*)name, name_len);
    outputa(":\n", 2);

    uint64_t stack_size = 32;

    bool push_reg[GEN_REG_COUNT] = {false};

    for (uint64_t ix = 0; ix < var_list->size; ++ix) {
        if (!var_local(vars[ix].kind)) {
            continue;
        }
        if (vars[ix].alloc_type == ALLOC_MEM) {
            ALLIGN_TO(stack_size, vars[ix].alignment);
            vars[ix].memory.offset = stack_size;
            stack_size += vars[ix].byte_size;
        } else if (vars[ix].alloc_type == ALLOC_REG &&
                   vars[ix].reg < GEN_REG_COUNT) {
            if (!GEN_VOLATILE_REGS[vars[ix].reg]) {
                push_reg[vars[ix].reg] = true;
            }
        }
    }

    for (uint32_t reg = 0; reg < GEN_REG_COUNT; ++reg) {
        if (push_reg[reg]) {
            emit_instr_name("push");
            emit_var_reg(reg, QUAD_64_BIT, 0);
            emit_instr_end();
            stack_size += 8;
        }
    }

    if (stack_size % 16 == 0) {
        stack_size += 8;
    }
    emit_instr_name("sub");
    emit_var_reg(RSP, QUAD_64_BIT, 0);
    emit_var_uint(stack_size, 1);
    emit_instr_end();

    Quad* q = def->quad_start->next_quad;
    String out;

    for (; q != def->quad_end->next_quad; q = q->next_quad) {
        enum QuadType type = q->type & QUAD_TYPE_MASK;
        uint64_t datatype = q->type & QUAD_DATATYPE_MASK;
        uint64_t datasize = q->type & QUAD_DATASIZE_MASK;

        switch (type) {
        case QUAD_GET_ARG: {
            if (q->op1.uint64 > 3) {
                assert(false);
                break;
            }
            uint32_t src_reg = GEN_CALL_REGS[q->op1.uint64];

            if (vars[q->dest].alloc_type != ALLOC_REG ||
                vars[q->dest].reg != src_reg) {
                emit_instr_name("mov");
                emit_var(&vars[q->dest], datatype, datasize, 0);
                emit_var_reg(src_reg, datasize, 1);
                emit_instr_end();
            }
            break;
        }
        case QUAD_PUT_ARG: {
            if (q->op1.uint64 > 3) {
                assert(false);
                break;
            }
            uint32_t reg = GEN_CALL_REGS[q->op1.uint64];
            if (vars[q->op2].alloc_type != ALLOC_REG ||
                vars[q->op2].reg != reg) {
                emit_instr_name("mov");
                emit_var_reg(reg, datasize, 0);
                emit_var(&vars[q->dest], datatype, datasize, 1);
                emit_instr_end();
            }
            break;
        }
        case QUAD_MOVE:
            if (is_same(vars, q->op1.var, q->dest)) {
                break;
            }
            emit_op1_dest_move(q, vars);
            break;
        case QUAD_CREATE:
            if (vars[q->dest].alloc_type == ALLOC_IMM) {
                break;
            }
            emit_instr_name("mov");
            emit_var(&vars[q->dest], datatype, datasize, 0);
            if (datatype == QUAD_UINT) {
                emit_var_uint(q->op1.uint64, 1);
            } else if (datatype == QUAD_SINT) {
                emit_var_sint(q->op1.sint64, 1);
            } else if (datatype == QUAD_BOOL) {
                emit_var_uint(q->op1.uint64 == 0 ? 0 : 1, 1);
            } else {
                assert(false);
            }
            emit_instr_end();
            break;
        case QUAD_MUL:
            if (datatype == QUAD_UINT) {
                assert_in_reg(q->dest, vars, RAX);
                if (!is_same(vars, q->op1.var, q->dest)) {
                    if (is_same(vars, q->op2, q->dest)) {
                        q->op2 = q->op1.var;
                    } else {
                        emit_op1_dest_move(q, vars);
                    }
                }
                emit_instr_name("mul");
                emit_var(&vars[q->op2], datatype, datasize, 0);
                emit_instr_end();
            } else if (datatype == QUAD_SINT) {
                emit_binop("imul", q, vars);
            } else {
                assert(false);
            }
            break;
        case QUAD_DIV:
        case QUAD_MOD: {
            uint32_t reg = type == QUAD_DIV ? RAX : RDX;
            assert_in_reg(q->dest, vars, reg);
            if (vars[q->op1.var].alloc_type != ALLOC_REG ||
                vars[q->op1.var].reg != RAX) {
                emit_instr_name("mov");
                emit_var_reg(RAX, datasize, 0);
                emit_var(&vars[q->op1.var], datatype, datasize, 1);
                emit_instr_end();
            }
            emit_instr_name("xor");
            emit_var_reg(RDX, datasize, 0);
            emit_var_reg(RDX, datasize, 1);
            emit_instr_end();
            emit_instr_name("div");
            emit_var(&vars[q->op2], datatype, datasize, 0);
            emit_instr_end();
            break;
        }
        case QUAD_RSHIFT:
        case QUAD_LSHIFT: {
            if (vars[q->op2].alloc_type != ALLOC_IMM) {
                assert_in_reg(q->op2, vars, RCX);
            }
            const char* instr;
            if (type == QUAD_RSHIFT) {
                instr = (datatype & QUAD_UINT) ? "shr" : "sar";
            } else {
                instr = (datasize & QUAD_UINT) ? "shl" : "sal";
            }
            if (!is_same(vars, q->op1.var, q->dest)) {
                emit_op1_dest_move(q, vars);
            }
            emit_instr_name(instr);
            emit_var(&vars[q->dest], datatype, datasize, 0);
            emit_var(&vars[q->op2], datatype, QUAD_8_BIT, 1);
            emit_instr_end();
            break;
        }
        case QUAD_CALL: {
            assert(vars[q->op1.var].name != NAME_ID_INVALID);
            name_id n = name_table->data[vars[q->op1.var].name].func_def->name;
            const uint8_t* name = name_table->data[n].name;
            uint32_t name_len = name_table->data[n].name_len;
            emit_instr_name("call");
            emit_var_fn(name, name_len, 0);
            emit_instr_end();
            break;
        }
        case QUAD_GET_RET:
            assert_in_reg(q->dest, vars, RAX);
            break;
        case QUAD_CMP_LE:
        case QUAD_CMP_L:
        case QUAD_CMP_GE:
        case QUAD_CMP_G:
        case QUAD_CMP_EQ:
        case QUAD_CMP_NEQ:
            if (vars[q->dest].alloc_type == ALLOC_IMM) {
                assert(datatype != QUAD_FLOAT);
                Quad* next = q->next_quad;
                const char* instr;
                bool inv = (next->type & QUAD_TYPE_MASK) == QUAD_JMP_FALSE;
                bool sign = datatype == QUAD_SINT;
                if (type == QUAD_CMP_LE) {
                    instr = inv ? (sign ? "jg" : "ja") : (sign ? "jle" : "jbe");
                } else if (type == QUAD_CMP_L) {
                    instr = inv ? (sign ? "jge" : "jae") : (sign ? "jl" : "jb");
                } else if (type == QUAD_CMP_G) {
                    instr = inv ? (sign ? "jle" : "jbe") : (sign ? "jg" : "ja");
                } else if (type == QUAD_CMP_GE) {
                    instr = inv ? (sign ? "jl" : "jb") : (sign ? "jge" : "jae");
                } else if (type == QUAD_CMP_EQ) {
                    instr = inv ? (sign ? "jne" : "jnz") : (sign ? "je" : "jz");
                } else {
                    instr = inv ? (sign ? "je" : "jz") : (sign ? "jne" : "jnz");
                }
                emit_instr_name("cmp");
                emit_var(&vars[q->op1.var], datatype, datasize, 0);
                if (vars[q->op2].alloc_type == ALLOC_IMM) {
                    LOG_DEBUG("%llu is imm", q->op2);
                }
                emit_var(&vars[q->op2], datatype, datasize, 1);
                emit_instr_end();
                emit_instr_name(instr);
                emit_var_label(next->op1.label, 0);
                emit_instr_end();

                q = next;
            } else {
                bool sign = datatype == QUAD_SINT;
                const char* instr;
                if (type == QUAD_CMP_LE) {
                    instr = sign ? "cmovle" : "cmovbe";
                } else if (type == QUAD_CMP_L) {
                    instr = sign ? "cmovl" : "cmovb";
                } else if (type == QUAD_CMP_G) {
                    instr = sign ? "cmovg" : "cmova";
                } else if (type == QUAD_CMP_GE) {
                    instr = sign ? "cmovge" : "cmovae";
                } else if (type == QUAD_CMP_EQ) {
                    instr = sign ? "cmove" : "cmovz";
                } else {
                    instr = sign ? "cmovne" : "cmovnz";
                }

                assert(vars[q->dest].alloc_type == ALLOC_REG);
                assert(vars[q->op1.var].alloc_type == ALLOC_REG);
                assert(vars[q->op1.var].reg != vars[q->dest].reg);
                emit_instr_name("cmp");
                emit_var(&vars[q->op1.var], datatype, datasize, 0);
                emit_var(&vars[q->op2], datatype, datasize, 1);
                emit_instr_end();
                emit_instr_name("mov");
                emit_var(&vars[q->dest], QUAD_BOOL, QUAD_32_BIT, 0);
                emit_var_uint(0, 1);
                emit_instr_end();
                emit_instr_name("mov");
                emit_var(&vars[q->op1.var], QUAD_BOOL, QUAD_32_BIT, 0);
                emit_var_uint(1, 1);
                emit_instr_end();
                emit_instr_name(instr);
                emit_var(&vars[q->dest], QUAD_BOOL, QUAD_32_BIT, 0);
                emit_var(&vars[q->op1.var], QUAD_BOOL, QUAD_32_BIT, 1);
                emit_instr_end();
            }
            break;
        case QUAD_JMP_FALSE:
        case QUAD_JMP_TRUE:
            assert(datatype == QUAD_BOOL);
            if (vars[q->op2].alloc_type == ALLOC_IMM) {
                if (vars[q->op2].imm.boolean) {
                    if (type == QUAD_JMP_TRUE) {
                        emit_instr_name("jmp");
                        emit_var_label(q->op1.label, 0);
                        emit_instr_end();
                    }
                } else {
                    if (type == QUAD_JMP_FALSE) {
                        emit_instr_name("jmp");
                        emit_var_label(q->op1.label, 0);
                        emit_instr_end();
                    }
                }
            } else if (vars[q->op2].alloc_type == ALLOC_MEM) {
                emit_instr_name("cmp");
                emit_var(&vars[q->op2], datatype, datasize, 0);
                emit_var_uint(0, 1);
                emit_instr_end();
            } else {
                emit_instr_name("test");
                emit_var(&vars[q->op2], datatype, datasize, 0);
                emit_var(&vars[q->op2], datatype, datasize, 1);
                emit_instr_end();
            }
            if (type == QUAD_JMP_TRUE) {
                emit_instr_name("jne");
            } else {
                emit_instr_name("je");
            }
            emit_var_label(q->op1.label, 0);
            emit_instr_end();
            break;
        case QUAD_LABEL:
            emit_label(q->op1.label);
            break;
        case QUAD_CAST_TO_INT64:
            if (datatype == QUAD_SINT) {
                if (datasize == QUAD_64_BIT) {
                    if (!is_same(vars, q->op1.var, q->dest)) {
                        emit_op1_dest_move(q, vars);
                    }
                    break;
                }
                assert(false);
            } else if (datatype == QUAD_UINT) {
                if (datasize == QUAD_64_BIT) {
                    if (!is_same(vars, q->op1.var, q->dest)) {
                        emit_op1_dest_move(q, vars);
                    }
                    break;
                }
                assert(false);
            } else if (datatype == QUAD_BOOL) {
                assert(false);
            } else {
                assert(false);
            }
        case QUAD_JMP:
            emit_instr_name("jmp");
            emit_var_label(q->op1.label, 0);
            emit_instr_end();
            break;
        case QUAD_SUB:
            emit_binop("sub", q, vars);
            break;
        case QUAD_ADD:
            emit_binop("add", q, vars);
            break;
        case QUAD_BIT_AND:
            emit_binop("and", q, vars);
            break;
        case QUAD_BIT_OR:
            emit_binop("or", q, vars);
            break;
        case QUAD_BIT_XOR:
            emit_binop("xor", q, vars);
        case QUAD_BOOL_NOT:
            assert(datatype == QUAD_BOOL);
            if (vars[q->op1.var].alloc_type == ALLOC_IMM) {
                emit_instr_name("mov");
                emit_var(&vars[q->dest], datatype, datasize, 0);
                emit_var_uint(vars[q->op1.var].imm.boolean ? 1 : 0, 1);
                emit_instr_end();
                break;
            } else if (vars[q->op1.var].alloc_type == ALLOC_MEM) {
                emit_instr_name("cmp");
                emit_var(&vars[q->op1.var], datatype, datasize, 0);
                emit_var_uint(0, 1);
                emit_instr_end();
            } else {
                emit_instr_name("test");
                emit_var(&vars[q->op1.var], datatype, datasize, 0);
                emit_var(&vars[q->op1.var], datatype, datasize, 1);
                emit_instr_end();
            }
            emit_instr_name("setz");
            emit_var(&vars[q->dest], datatype, datasize, 0);
            emit_instr_end();
            break;
        case QUAD_RETURN:
            if (vars[q->op1.var].alloc_type != ALLOC_REG ||
                vars[q->op1.var].reg != RAX) {
                emit_instr_name("mov");
                emit_var_reg(RAX, datasize, 0);
                emit_var(&vars[q->op1.var], datatype, datasize, 1);
                emit_instr_end();
            }
            emit_instr_name("jmp");
            emit_var_endlabel(name, name_len, 0);
            emit_instr_end();
            break;
        case QUAD_GET_ADDR:
        case QUAD_CALC_ADDR: {
            assert(vars[q->op2].alloc_type == ALLOC_REG);
            assert(vars[q->dest].alloc_type == ALLOC_REG);
            if (vars[q->op1.var].kind == VAR_ARRAY ||
                vars[q->op1.var].kind == VAR_ARRAY_GLOBAL) {
                emit_instr_name("lea");
                emit_var(&vars[q->dest], QUAD_PTR, QUAD_PTR_SIZE, 0);
                emit_var(&vars[q->op1.var], QUAD_PTR, QUAD_PTR_SIZE, 1);
                emit_instr_end();
            } else {
                if (!is_same(vars, q->op1.var, q->dest)) {
                    emit_op1_dest_move(q, vars);
                }
            }
            if (type == QUAD_GET_ADDR) {
                emit_instr_name("mov");
            } else {
                emit_instr_name("lea");
            }
            emit_var(&vars[q->dest], datatype, datasize, 0);
            emit_var_scaled_ix(q->type & QUAD_SCALE_MASK, vars[q->dest].reg, 
                               vars[q->op2].reg, QUAD_64_BIT, datasize, 1);
            emit_instr_end();
            break;
        }
        case QUAD_SET_ADDR:
            assert(vars[q->op1.var].alloc_type == ALLOC_REG);
            assert(vars[q->op2].alloc_type == ALLOC_REG);
            emit_instr_name("mov");
            emit_var_deref(vars[q->op1.var].reg, datasize, 0);
            emit_var(&vars[q->op2], datatype, datasize, 1);
            emit_instr_end();
            break;
        case QUAD_ADDROF:
            assert(vars[q->dest].alloc_type == ALLOC_REG);
            assert(vars[q->op1.var].alloc_type == ALLOC_MEM);
            emit_instr_name("lea");
            emit_var_reg(vars[q->dest].reg, QUAD_PTR_SIZE, 0);
            emit_var(&vars[q->op1.var], datatype, datasize, 1);
            emit_instr_end();
            break;
        case QUAD_DEREF:
            assert(vars[q->dest].alloc_type == ALLOC_REG);
            assert(vars[q->op1.var].alloc_type == ALLOC_REG);
            emit_instr_name("mov");
            emit_var_reg(vars[q->dest].reg, datasize, 0);
            emit_var_deref(vars[q->op1.var].reg, datasize, 1);
            emit_instr_end();
            break;
        default:
            String_create(&out);
            fmt_quad(q, &out);
            _printf_e("Not implemented: %s\n", out.buffer);
            assert(false && "Not implemented");
            break;
        }
    }
    _printf("%.*s_end:\n", name_len, name);
    emit_instr_name("add");
    emit_var_reg(RSP, QUAD_64_BIT, 0);
    emit_var_uint(stack_size, 1);
    emit_instr_end();

    for (int32_t reg = GEN_REG_COUNT - 1; reg >= 0; --reg) {
        if (push_reg[reg]) {
            emit_instr_name("pop");
            emit_var_reg(reg, QUAD_64_BIT, 0);
            emit_instr_end();
        }
    }
    emit_instr_name("ret");
    emit_instr_end();
}

void Backend_generate_asm(NameTable *name_table, FunctionTable *func_table, Arena* arena) {
    _printf("format PE64 console\n");
    _printf("entry main\n\n");

    for (uint64_t ix = 0; ix < func_table->size; ++ix) {
        Backend_generate_fn(func_table->data[ix], arena, 
                            name_table, func_table);
    }
}
