#include "amd64_asm.h"
#include "asm/amd64.h"
#include <printf.h>
#include "format.h"

#define LOG_CATAGORY LOG_CATAGORY_REGISTER_ALLOCATION

const uint8_t GEN_CALL_REGS[4] = {RCX, RDX, R8, R9};
const bool GEN_VOLATILE_REGS[GEN_REG_COUNT] = {
    true, true, true, false, false, false, false, false,
    true, true, true, true, false, false, false, false
};



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

void preinsert_move(ConflictGraph* graph, var_id* var, VarSet* live_set,
                    Quad* quad, FlowNode* node, VarList* vars, Arena* arena) {

    var_id tmp = create_temp_var(graph, vars, node, live_set, *var);
    ConflictGraph_add_edge(graph, RSP, tmp + graph->reg_count);
    Quad* nq = Arena_alloc_type(arena, Quad);
    uint32_t datasize = vars->data[*var].byte_size;
    assert(datasize == 1 || datasize == 2 || datasize == 4 || datasize == 8);
    nq->type = QUAD_MOVE;
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
    uint32_t datasize = vars->data[*dest].byte_size;
    assert(datasize == 1 || datasize == 2 || datasize == 4 || datasize == 8);
    nq->type = QUAD_MOVE;
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
bool is_standard_binop(Quad* q, VarList* vars) {
    switch (q->type) {
    case QUAD_ADD:
        if (q->scale != QUADSCALE_NONE) {
            return false;
        }
        break;
    case QUAD_SUB:
    case QUAD_BIT_AND:
    case QUAD_BIT_OR:
    case QUAD_BIT_XOR:
        break;
    default:
        return false;
    }

    if (vars->data[q->dest].datatype == VARTYPE_FLOAT) {
        return false;
    }
    return true;
}

bool is_symetrical(Quad* q) {
    switch (q->type) {
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

bool is_cmp(Quad* q) {
    switch (q->type) {
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

bool allow_op2_imm(Quad* q, VarList* vars) {
    if (is_standard_binop(q, vars)) {
        return true;
    }
    if (is_cmp(q)) {
        return vars->data[q->op1.var].datatype != VARTYPE_FLOAT;
    }
    return q->type == QUAD_RSHIFT || q->type == QUAD_LSHIFT;
}

bool allow_op1_imm(Quad* q, VarList* vars) {
    if (q->type == QUAD_CAST_TO_INT64 || q->type == QUAD_CAST_TO_UINT64 ||
        q->type == QUAD_CAST_TO_BOOL) {
        return vars->data[q->op1.var].datatype != VARTYPE_FLOAT;
    }
    return false;
}


bool fully_local(VarList* list, var_id v) {
    return list->data[v].reads == 1 && list->data[v].writes == 1 &&
           list->data[v].kind == VAR_TEMP;
}

void Backend_add_constrains(ConflictGraph* graph, VarSet* live_set, Quad* quad,
                            VarList* vars, FlowNode* node, Arena* arena) {
    enum QuadType q = quad->type;

    if (is_standard_binop(quad, vars)) {
        if (vars->data[quad->dest].alloc_type == ALLOC_MEM &&
            vars->data[quad->op2].alloc_type == ALLOC_MEM) {
            // Two memory ops not allowed, insert move
            preinsert_move(graph, &quad->op2, live_set, quad, node, vars, arena);
        }

        if (quad->dest != quad->op2 && quad->dest != quad->op1.var) {
            ConflictGraph_add_edge(graph, quad->dest + graph->reg_count,
                    quad->op2 + graph->reg_count);
        }
    } else if (q == QUAD_ADD && vars->data[quad->dest].datatype != VARTYPE_FLOAT) {
        assert(quad->scale != QUADSCALE_NONE);
        if (vars->data[quad->dest].alloc_type == ALLOC_MEM) {
            postinsert_move(graph, &quad->dest, live_set, quad, node, vars, arena);
        }
        if (vars->data[quad->op2].alloc_type == ALLOC_MEM) {
            preinsert_move(graph, &quad->op2, live_set, quad, node, vars, arena);
        }

        if (quad->dest != quad->op2 && quad->dest != quad->op1.var) {
            ConflictGraph_add_edge(graph, quad->dest + graph->reg_count,
                    quad->op2 + graph->reg_count);
        }
    } else if (q == QUAD_BOOL_NOT) {
        // cmp + setz
    } else if (q == QUAD_NEGATE) {
        // neg
    } else if (q == QUAD_SET_ADDR) {
        if (vars->data[quad->op1.var].alloc_type == ALLOC_MEM) {
            preinsert_move(graph, &quad->op1.var, live_set, quad, node, vars, arena);
        }
        if (vars->data[quad->op2].alloc_type == ALLOC_MEM) {
            preinsert_move(graph, &quad->op2, live_set, quad, node, vars, arena);
        }
    } else if (q == QUAD_CALC_ARRAY_ADDR || q == QUAD_GET_ARRAY_ADDR) {
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
        uint64_t data_size = vars->data[quad->dest].byte_size;
        enum VarDatatype datatype = vars->data[quad->dest].datatype;
        // 64 bit values require a register
        if (data_size == 8 && (
            (datatype == VARTYPE_UINT && quad->op1.uint64 > UINT32_MAX) ||
            (datatype == VARTYPE_SINT && (quad->op1.sint64 < INT32_MIN ||
                                       quad->op1.sint64 > INT32_MAX)))) {
            if (vars->data[quad->dest].alloc_type == ALLOC_MEM) {
                postinsert_move(graph, &quad->dest, live_set, quad, node, 
                                vars, arena);
            }
            return;
        }
        if (quad->next_quad == NULL || !fully_local(vars, quad->dest)) {
            return;
        }
        Quad* next = quad->next_quad;
        bool good_datatype = false;
        if (datatype == VARTYPE_SINT) {
            good_datatype = data_size < 8 || 
                (quad->op1.sint64 <= INT32_MAX &&
                 quad->op1.sint64 >= INT32_MIN);
        } else if (datatype == VARTYPE_UINT) {
            good_datatype = data_size < 4 || quad->op1.uint64 <= INT32_MAX;
        } else if (datatype == VARTYPE_BOOL) {
            good_datatype = true;
        }
        if (!good_datatype) {
            return;
        }
        if (is_cmp(next) && vars->data[quad->next_quad->dest].alloc_type != ALLOC_IMM) {
            // cmp into value generates conditional moves, needs registers
            return;
        }
        if ((allow_op2_imm(next, vars) && (quad->next_quad->op2 == quad->dest ||
             is_symetrical(next) && quad->next_quad->op1.var == quad->dest)) ||
             allow_op1_imm(next, vars) && quad->next_quad->op1.var == quad->dest) {
            if (vars->data[quad->dest].alloc_type == ALLOC_NONE) {
                vars->data[quad->dest].alloc_type = ALLOC_IMM;
                if (datatype == VARTYPE_SINT) {
                    LOG_DEBUG("Created imm singed %lld", quad->op1.sint64);
                    vars->data[quad->dest].imm.int64 = quad->op1.sint64;
                } else if (datatype == VARTYPE_UINT) {
                    LOG_DEBUG("Created imm unsinged %llu, %llu", quad->op1.uint64,
                               quad->dest);
                    vars->data[quad->dest].imm.uint64 = quad->op1.uint64;
                } else {
                    assert(datatype == VARTYPE_BOOL);
                    LOG_DEBUG("Created imm boolean %u", quad->op1.uint64);
                    vars->data[quad->dest].imm.boolean = quad->op1.uint64 != 0;
                }
            }
        }
    } else if (q == QUAD_GET_ARG && quad->op1.uint64 < 4) {
        uint64_t reg = GEN_CALL_REGS[quad->op1.uint64];
        requre_gen_reg_dest(graph, reg, &quad->dest, live_set, quad, node, 
                            vars, arena);
    } else if (q == QUAD_GET_ARG) {
        if (vars->data[quad->dest].alloc_type == ALLOC_MEM) {
            postinsert_move(graph, &quad->dest, live_set, quad, node,
                            vars, arena);
        }
    } else if (q == QUAD_CMP_EQ || q == QUAD_CMP_NEQ || q == QUAD_CMP_G ||
        q == QUAD_CMP_L || q == QUAD_CMP_GE || q == QUAD_CMP_LE) {

        // If next quad is JMP_TRUE or JMP_FALSE, and uses quad->dest
        // and quad->dest is only used here, make it ALLOC_IMM
        if (quad->next_quad != NULL) {
            enum QuadType next = quad->next_quad->type;
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
    } else if ((q == QUAD_MUL && (vars->data[quad->dest].datatype == VARTYPE_UINT ||
                 (vars->data[quad->dest].byte_size == 1))) ||
                q == QUAD_DIV || q == QUAD_MOD) {
        enum VarDatatype datatype = vars->data[quad->dest].datatype;
        assert(datatype != VARTYPE_FLOAT);
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
    } else if (q == QUAD_MUL) {
        enum VarDatatype datatype = vars->data[quad->dest].datatype;
        assert(datatype != VARTYPE_FLOAT);
        if (vars->data[quad->dest].alloc_type == ALLOC_MEM) {
            postinsert_move(graph, &quad->dest, live_set, quad, node, vars, arena);
        }
        if (quad->dest != quad->op2 && quad->dest != quad->op1.var) {
            ConflictGraph_add_edge(graph, quad->dest + graph->reg_count,
                    quad->op2 + graph->reg_count);
        }
    } else if (q == QUAD_CALL || q == QUAD_CALL_PTR) {
        if (q == QUAD_CALL_PTR) {
            // Just to make sure everything is fine when adding fn pointers
            assert(vars->data[quad->op1.var].kind == VAR_GLOBAL);
        }
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
    } else if (q == QUAD_PUT_ARG) {
        if (vars->data[quad->op2].alloc_type == ALLOC_MEM) {
            preinsert_move(graph, &quad->op2, live_set, quad, node, vars, arena);
        }
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
    } else if (q == QUAD_CAST_TO_UINT64 || q == QUAD_CAST_TO_INT64) {
        uint64_t datasize = vars->data[quad->op1.var].byte_size;
        if (vars->data[quad->op1.var].datatype == VARTYPE_FLOAT) {
            assert(false);
        } else {
            if (vars->data[quad->dest].alloc_type == ALLOC_MEM) {
                postinsert_move(graph, &quad->dest, live_set, quad,
                                node, vars, arena);
            }
        }
    } else if (q == QUAD_CAST_TO_UINT32 || q == QUAD_CAST_TO_INT32) {
        uint64_t datasize = vars->data[quad->op1.var].byte_size;
        if (vars->data[quad->op1.var].datatype == VARTYPE_FLOAT) {
            assert(false);
        } else {
            if (vars->data[quad->dest].alloc_type == ALLOC_MEM) {
                postinsert_move(graph, &quad->dest, live_set, quad, node,
                                vars, arena);
            }
        }
    } else if (q == QUAD_CAST_TO_UINT16 || q == QUAD_CAST_TO_INT16) {
        uint64_t datasize = vars->data[quad->op1.var].byte_size;
        if (vars->data[quad->op1.var].datatype == VARTYPE_FLOAT) {
            assert(false);
        } else {
            if (vars->data[quad->dest].alloc_type == ALLOC_MEM) {
                postinsert_move(graph, &quad->dest, live_set, quad,
                                node, vars, arena);
            }
        }
    } else if (q == QUAD_ARRAY_TO_PTR) {
        assert(vars->data[quad->op1.var].kind == VAR_ARRAY ||
               vars->data[quad->op1.var].kind == VAR_ARRAY_GLOBAL);
        if (vars->data[quad->dest].alloc_type == ALLOC_MEM) {
            postinsert_move(graph, &quad->dest, live_set, quad, node, vars, arena);
        }
    } else if (q == QUAD_CAST_TO_UINT8 || q == QUAD_CAST_TO_INT8) {
        if (vars->data[quad->op1.var].datatype == VARTYPE_FLOAT) {
            assert(false);
        } else {
            // Pass
        }
    } else if (q == QUAD_CAST_TO_BOOL) {
        if (vars->data[quad->op1.var].datatype == VARTYPE_FLOAT) {
            assert(false);
        } else {
            // Pass?
        }
    } else if (q == QUAD_MOVE) {
        if (vars->data[quad->op1.var].alloc_type == ALLOC_MEM &&
            vars->data[quad->dest].alloc_type == ALLOC_MEM) {
            preinsert_move(graph, &quad->op1.var, live_set, quad,
                           node, vars, arena);
        }
    } else if (q == QUAD_LABEL || q == QUAD_JMP || q == QUAD_JMP_FALSE ||
               q == QUAD_JMP_TRUE) {
        // Pass
    } else if (q == QUAD_STRUCT_ADDR) {
        if (vars->data[quad->dest].alloc_type == ALLOC_MEM) {
            postinsert_move(graph, &quad->dest, live_set, quad, node, vars, arena);
        }
        require_mem(graph, quad->op2);
    } else { 
        String out;
        String_create(&out);
        fmt_quad(quad, &out);
        _printf_e("Not implemented: %s\n", out.buffer);
        assert(false && "Not implemented");
    }
}

#undef LOG_CATAGORY
#define LOG_CATAGORY LOG_CATAGORY_ASM_GENERATION

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

uint8_t scale_val(enum QuadScale scale) {
    switch (scale) {
    case QUADSCALE_1:
        return 1;
    case QUADSCALE_2:
        return 2;
    case QUADSCALE_4:
        return 4;
    case QUADSCALE_8:
        return 8;
    default:
        assert(false);
    }
}

void emit_var_with_size(VarData* var, uint32_t datasize, AsmCtx* ctx) {
    if (var->alloc_type == ALLOC_MEM) {
        if (var_local(var->kind)) {
            asm_mem_var(ctx, datasize, RSP, 0, RAX, var->memory.offset);
        } else if (var->kind == VAR_ARRAY_GLOBAL || var->kind == VAR_GLOBAL) {
            asm_global_mem_var(ctx, datasize, var->symbol_ix, 0);
        } else {
            assert(false);
        }
    } else if (var->alloc_type == ALLOC_REG) {
        asm_reg_var(ctx, datasize, var->reg);
    } else if (var->alloc_type == ALLOC_IMM) {
        // TODO: endianness?
        enum VarDatatype datatype = var->datatype;
        if (datatype == VARTYPE_SINT) {
            if (datasize == 8) {
                assert(var->imm.int64 >= INT32_MIN && var->imm.int64 <= INT32_MAX);
                datasize = 4;
            }
            if (datasize == 4 && var->imm.int64 >= INT8_MIN &&
                       var->imm.int64 <= INT8_MAX) {
                datasize = 1;
            }
            const uint8_t* data = (const uint8_t*)&var->imm.int64;
            asm_imm_var(ctx, datasize, data);
        } else if (datatype == VARTYPE_UINT) {
            if (datasize == 8) {
                assert(var->imm.uint64 <= UINT32_MAX);
                datasize = 4;
            }
            if (datasize == 4 && var->imm.uint64 <= UINT8_MAX) {
                datasize = 1;
            }
            const uint8_t* data = (const uint8_t*)&var->imm.uint64;
            asm_imm_var(ctx, datasize, data);
        } else if (datatype == VARTYPE_BOOL) {
            uint8_t b = var->imm.boolean ? 1 : 0;
            asm_imm_var(ctx, 1, &b);
        } else {
            assert(false);
        }
    } else {
        assert(false);
    }
}

void emit_var(VarData* var, AsmCtx* ctx) {
    uint32_t datasize = var->byte_size;
    emit_var_with_size(var, datasize, ctx);
}

void emit_op1_dest_move(Quad* quad, VarData* vars, AsmCtx* ctx) {
    VarData* op1 = &vars[quad->op1.var];
    VarData* dest = &vars[quad->dest];

    asm_instr(ctx, OP_MOV);
    emit_var(dest, ctx);
    if (op1->alloc_type == ALLOC_IMM) {
        uint32_t datasize = op1->byte_size;
        enum VarDatatype datatype = op1->datatype;
        const uint8_t* data;
        if (datatype == VARTYPE_SINT) {
            if (datasize == 8 && op1->imm.int64 >= INT32_MIN &&
                op1->imm.int64 <= INT32_MAX) {
                int32_t v = op1->imm.int64;
                asm_imm_var(ctx, 4, (const uint8_t*)&v);
            } else {
                data = (const uint8_t*)&op1->imm.int64;
                asm_imm_var(ctx, datasize, data);
            }
        } else if (datatype == VARTYPE_UINT) {
            if (datasize == 8 && op1->imm.uint64 < UINT32_MAX) {
                uint32_t v = op1->imm.uint64;
                asm_imm_var(ctx, 4, (const uint8_t*)&v);
            } else {
                data = (const uint8_t*)&op1->imm.uint64;
                asm_imm_var(ctx, datasize, data);
            }
        } else {
            assert(datatype == VARTYPE_BOOL);
            uint8_t d = op1->imm.boolean ? 1 : 0;
            asm_imm_var(ctx, datasize, &d);
        }
    } else {
        emit_var(op1, ctx);
    }
    asm_instr_end(ctx);
}

void emit_binop(enum Amd64Opcode op, Quad* quad, VarData* vars, AsmCtx* ctx) {
    VarData* op1 = &vars[quad->op1.var];
    VarData* op2 = &vars[quad->op2];
    VarData* dest = &vars[quad->dest];
    if (op1->alloc_type == ALLOC_IMM) {
        assert(is_symetrical(quad));
        VarData* tmp = op1;
        op1 = op2;
        op2 = tmp;
    }
    assert(dest->alloc_type == ALLOC_MEM || dest->alloc_type == ALLOC_REG);
    assert(op1->alloc_type == ALLOC_MEM || op1->alloc_type == ALLOC_REG);
    if (!is_same(vars, quad->op1.var, quad->dest)) {
        if (is_symetrical(quad) &&
            is_same(vars, quad->op2, quad->dest)) {
            VarData* tmp = op1;
            op1 = op2;
            op2 = tmp;
        } else {
            emit_op1_dest_move(quad, vars, ctx);
        }
    }
    assert(dest->alloc_type != ALLOC_MEM || op2->alloc_type != ALLOC_MEM);
    asm_instr(ctx, op);
    emit_var(dest, ctx);
    emit_var(op2, ctx);
    asm_instr_end(ctx);
}

#define assert_in_reg(v, vars, r) assert((vars)[(v)].alloc_type == ALLOC_REG && \
        (vars)[(v)].reg == r)

void Backend_generate_fn(FunctionDef* def, Arena* arena, AsmCtx* ctx,
                         NameTable* name_table, FunctionTable* func_table) {
    VarList* var_list = &def->vars;
    VarData* vars = var_list->data;
    assert(def->quad_start->type == QUAD_LABEL);
    const uint8_t* name = name_table->data[def->name].name;

    uint32_t name_len = name_table->data[def->name].name_len;
    uint64_t stack_size = 32;

    for (Quad* q = def->quad_start; q != def->quad_end->next_quad; q = q->next_quad) {
        if (q->type == QUAD_PUT_ARG) {
            if (q->op1.uint64 >= 4 && q->op1.uint64 * 8 + 8 > stack_size) {
                stack_size = q->op1.uint64 * 8 + 8;
            }
        }
    }

    bool push_reg[GEN_REG_COUNT] = {false};

    for (uint64_t ix = 0; ix < var_list->size; ++ix) {
        if (!var_local(vars[ix].kind)) {
            continue;
        }
        if (vars[ix].alloc_type == ALLOC_MEM) {
            ALIGN_TO(stack_size, vars[ix].alignment);
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
            asm_instr(ctx, OP_PUSH);
            asm_reg_var(ctx, 8, reg);
            asm_instr_end(ctx);
            stack_size += 8;
        }
    }

    if (stack_size % 16 == 0) {
        stack_size += 8;
    }
    assert(stack_size <= INT32_MAX);
    uint32_t stack_size_u32 = stack_size;

    asm_instr(ctx, OP_SUB);
    asm_reg_var(ctx, 8, RSP);
    asm_imm_var(ctx, 4, (const uint8_t*)&stack_size_u32);
    asm_instr_end(ctx);

    Quad* q = def->quad_start->next_quad;
    String out;

    for (; q != def->quad_end->next_quad; q = q->next_quad) {
        enum QuadType type = q->type;

        switch (type) {
        case QUAD_GET_ARG: {
            uint32_t datasize = vars[q->dest].byte_size;
            // For now...
            assert(datasize <= 8);
            if (q->op1.uint64 > 3) {
                assert(vars[q->dest].alloc_type == ALLOC_REG);
                asm_instr(ctx, OP_MOV);
                emit_var(&vars[q->dest], ctx);
                asm_mem_var(ctx, datasize, RSP, 0, RAX,
                            stack_size + q->op1.uint64 * 8 + 8);
                asm_instr_end(ctx);
                break;
            }
            uint32_t src_reg = GEN_CALL_REGS[q->op1.uint64];

            if (vars[q->dest].alloc_type != ALLOC_REG ||
                vars[q->dest].reg != src_reg) {
                asm_instr(ctx, OP_MOV);
                emit_var(&vars[q->dest], ctx);
                asm_reg_var(ctx, datasize, src_reg);
                asm_instr_end(ctx);
            }
            break;
        }
        case QUAD_PUT_ARG: {
            uint32_t datasize = vars[q->op2].byte_size;
            if (q->op1.uint64 > 3) {
                assert(vars[q->op2].alloc_type == ALLOC_REG);
                asm_instr(ctx, OP_MOV);
                asm_mem_var(ctx, datasize, RSP, 0, RAX,
                            q->op1.uint64 * 8);
                emit_var(&vars[q->op2], ctx);
                asm_instr_end(ctx);
                break;
            }
            uint32_t reg = GEN_CALL_REGS[q->op1.uint64];
            if (vars[q->op2].alloc_type != ALLOC_REG ||
                vars[q->op2].reg != reg) {
                asm_instr(ctx, OP_MOV);
                asm_reg_var(ctx, datasize, reg);
                emit_var(&vars[q->dest], ctx);
                asm_instr_end(ctx);
            }
            break;
        }
        case QUAD_MOVE:
            if (is_same(vars, q->op1.var, q->dest)) {
                break;
            }
            emit_op1_dest_move(q, vars, ctx);
            break;
        case QUAD_CREATE: {
            if (vars[q->dest].alloc_type == ALLOC_IMM) {
                break;
            }
            asm_instr(ctx, OP_MOV);
            emit_var(&vars[q->dest], ctx);
            enum VarDatatype datatype = vars[q->dest].datatype;
            uint32_t datasize = vars[q->dest].byte_size;
            if (datatype == VARTYPE_UINT) {
                if (datasize == 8) {
                    if (q->op1.uint64 > UINT32_MAX) {
                        asm_imm_var(ctx, 8, (const uint8_t*)&q->op1.uint64);
                    } else {
                        uint32_t v = q->op1.uint64;
                        asm_imm_var(ctx, 4, (const uint8_t*)&v);
                    }
                } else if (datasize == 4) {
                    uint32_t v = q->op1.uint64;
                    asm_imm_var(ctx, 4, (const uint8_t*)&v);
                } else if (datasize == 2) {
                    uint16_t v = q->op1.uint64;
                    asm_imm_var(ctx, 2, (const uint8_t*)&v);
                } else {
                    assert(datasize == 1);
                    uint8_t v = q->op1.uint64;
                    asm_imm_var(ctx, 1, &v);
                }
            } else if (datatype == VARTYPE_SINT) {
                if (datasize == 8) {
                    if (q->op1.sint64 > INT32_MAX || q->op1.sint64 < INT32_MIN) {
                        asm_imm_var(ctx, 8, (const uint8_t*)&q->op1.sint64);
                    } else {
                        int32_t v = q->op1.sint64;
                        asm_imm_var(ctx, 4, (const uint8_t*)&v);
                    }
                } else if (datasize == 4) {
                    int32_t v = q->op1.sint64;
                    asm_imm_var(ctx, 4, (const uint8_t*)&v);
                } else if (datasize == 2) {
                    int16_t v = q->op1.sint64;
                    asm_imm_var(ctx, 2, (const uint8_t*)&v);
                } else {
                    assert(datasize == 1);
                    int8_t v = q->op1.sint64;
                    asm_imm_var(ctx, 1, (const uint8_t*)&v);
                }
            } else if (datatype == VARTYPE_BOOL) {
                assert(datasize == 1);
                uint8_t v = (q->op1.uint64 == 0) ? 0 : 1;
                asm_imm_var(ctx, 1, &v);
            } else {
                assert(false);
            }
            asm_instr_end(ctx);
            break;
        }
        case QUAD_MUL: {
            enum VarDatatype datatype = vars[q->dest].datatype;
            uint32_t datasize = vars[q->dest].byte_size;
            if (datatype == VARTYPE_UINT || datasize == 1) {
                assert_in_reg(q->dest, vars, RAX);
                if (!is_same(vars, q->op1.var, q->dest)) {
                    if (is_same(vars, q->op2, q->dest)) {
                        q->op2 = q->op1.var;
                    } else {
                        emit_op1_dest_move(q, vars, ctx);
                    }
                }
                if (datatype == VARTYPE_UINT) {
                    asm_instr(ctx, OP_MUL);
                } else {
                    asm_instr(ctx, OP_IMUL);
                }
                emit_var(&vars[q->op2], ctx);
                asm_instr_end(ctx);
            } else if (datatype == VARTYPE_SINT) {
                emit_binop(OP_IMUL, q, vars, ctx);
            } else {
                assert(false);
            }
            break;
        }
        case QUAD_DIV:
        case QUAD_MOD: {
            enum VarDatatype datatype = vars[q->dest].datatype;
            uint32_t datasize = vars[q->dest].byte_size;

            uint32_t reg = type == QUAD_DIV ? RAX : RDX;
            assert_in_reg(q->dest, vars, reg);
            if (vars[q->op1.var].alloc_type != ALLOC_REG ||
                vars[q->op1.var].reg != RAX) {
                asm_instr(ctx, OP_MOV);
                asm_reg_var(ctx, datasize, RAX);
                emit_var(&vars[q->op1.var], ctx);
                asm_instr_end(ctx);
            }
            asm_instr(ctx, OP_XOR);
            asm_reg_var(ctx, datasize, RDX);
            asm_reg_var(ctx, datasize, RDX);
            asm_instr_end(ctx);
            if (datatype == VARTYPE_SINT) {
                asm_instr(ctx, OP_IDIV);
            } else {
                asm_instr(ctx, OP_DIV);
            }
            emit_var(&vars[q->op2], ctx);
            asm_instr_end(ctx);
            break;
        }
        case QUAD_RSHIFT:
        case QUAD_LSHIFT: {
            enum VarDatatype datatype = vars[q->dest].datatype;
            if (vars[q->op2].alloc_type != ALLOC_IMM) {
                assert_in_reg(q->op2, vars, RCX);
            }
            enum Amd64Opcode instr;
            if (type == QUAD_RSHIFT) {
                instr = (datatype == VARTYPE_UINT) ? OP_SHR : OP_SAR;
            } else {
                instr = (datatype == VARTYPE_UINT) ? OP_SHL : OP_SAL;
            }
            if (!is_same(vars, q->op1.var, q->dest)) {
                emit_op1_dest_move(q, vars, ctx);
            }
            asm_instr(ctx, instr);
            emit_var(&vars[q->dest], ctx);
            emit_var(&vars[q->op2], ctx);
            asm_instr_end(ctx);
            break;
        }
        case QUAD_CALL_PTR: {
            if (vars[q->op1.var].kind == VAR_GLOBAL) {
                symbol_ix ix = vars[q->op1.var].symbol_ix;
                asm_instr(ctx, OP_CALL);
                asm_global_mem_var(ctx, PTR_SIZE, ix, 0);
                asm_instr_end(ctx);
            } else {
                assert("Fn pointer not supported" && false);
            }
            break;
        }
        case QUAD_CALL: {
            assert(vars[q->op1.var].name != NAME_ID_INVALID);
            symbol_ix sym = name_table->data[vars[q->op1.var].name].func_def->symbol;
            asm_instr(ctx, OP_CALL);
            asm_symbol_label_var(ctx, sym);
            asm_instr_end(ctx);
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
        case QUAD_CMP_NEQ: {
            uint32_t datasize = vars[q->op1.var].byte_size;
            enum VarDatatype datatype = vars[q->op1.var].datatype;
            if (vars[q->dest].alloc_type == ALLOC_IMM) {
                assert(datatype != VARTYPE_FLOAT);
                Quad* next = q->next_quad;
                enum Amd64Opcode instr;
                bool inv = next->type == QUAD_JMP_FALSE;
                bool sign = datatype == VARTYPE_SINT;
                if (type == QUAD_CMP_LE) {
                    instr = inv ? (sign ? OP_JG : OP_JA) : (sign ? OP_JLE : OP_JBE);
                } else if (type == QUAD_CMP_L) {
                    instr = inv ? (sign ? OP_JGE : OP_JAE) : (sign ? OP_JL : OP_JB);
                } else if (type == QUAD_CMP_G) {
                    instr = inv ? (sign ? OP_JLE : OP_JBE) : (sign ? OP_JG : OP_JA);
                } else if (type == QUAD_CMP_GE) {
                    instr = inv ? (sign ? OP_JL : OP_JB) : (sign ? OP_JGE : OP_JAE);
                } else if (type == QUAD_CMP_EQ) {
                    instr = inv ? (sign ? OP_JNE : OP_JNZ) : (sign ? OP_JE : OP_JZ);
                } else {
                    instr = inv ? (sign ? OP_JE : OP_JZ) : (sign ? OP_JNE : OP_JNZ);
                }
                asm_instr(ctx, OP_CMP);
                emit_var(&vars[q->op1.var], ctx);
                emit_var(&vars[q->op2], ctx);
                asm_instr_end(ctx);
                asm_instr(ctx, instr);
                asm_label_var(ctx, next->op1.label);
                asm_instr_end(ctx);

                q = next;
            } else {
                bool sign = datatype == VARTYPE_SINT;
                enum Amd64Opcode instr;
                if (type == QUAD_CMP_LE) {
                    instr = sign ? OP_SETLE : OP_SETBE;
                } else if (type == QUAD_CMP_L) {
                    instr = sign ? OP_SETL : OP_SETB;
                } else if (type == QUAD_CMP_G) {
                    instr = sign ? OP_SETG : OP_SETA;
                } else if (type == QUAD_CMP_GE) {
                    instr = sign ? OP_SETGE : OP_SETAE;
                } else if (type == QUAD_CMP_EQ) {
                    instr = sign ? OP_SETE : OP_SETZ;
                } else {
                    instr = sign ? OP_SETNE : OP_SETNZ;
                }

                assert(vars[q->op1.var].alloc_type == ALLOC_REG ||
                       vars[q->op2].alloc_type == ALLOC_REG);

                asm_instr(ctx, OP_CMP);
                emit_var(&vars[q->op1.var], ctx);
                emit_var(&vars[q->op2], ctx);
                asm_instr_end(ctx);

                asm_instr(ctx, instr);
                emit_var(&vars[q->dest], ctx);
                asm_instr_end(ctx);
            }
            break;
        }
        case QUAD_JMP_FALSE:
        case QUAD_JMP_TRUE: {
            if (vars[q->op2].alloc_type == ALLOC_IMM) {
                if (vars[q->op2].imm.boolean) {
                    if (type == QUAD_JMP_TRUE) {
                        asm_instr(ctx, OP_JMP);
                        asm_label_var(ctx, q->op1.label);
                        asm_instr_end(ctx);
                    }
                } else {
                    if (type == QUAD_JMP_FALSE) {
                        asm_instr(ctx, OP_JMP);
                        asm_label_var(ctx, q->op1.label);
                        asm_instr_end(ctx);
                    }
                }
            } else if (vars[q->op2].alloc_type == ALLOC_MEM) {
                asm_instr(ctx, OP_CMP);
                emit_var(&vars[q->op2], ctx);
                uint8_t v = 0;
                asm_imm_var(ctx, 1, &v);
                asm_instr_end(ctx);
            } else {
                asm_instr(ctx, OP_TEST);
                emit_var(&vars[q->op2], ctx);
                emit_var(&vars[q->op2], ctx);
                asm_instr_end(ctx);
            }
            if (type == QUAD_JMP_TRUE) {
                asm_instr(ctx, OP_JNE);
            } else {
                asm_instr(ctx, OP_JE);
            }
            asm_label_var(ctx, q->op1.label);
            asm_instr_end(ctx);
            break;
        }
        case QUAD_LABEL:
            asm_put_label(ctx, q->op1.label);
            break;
        case QUAD_CAST_TO_UINT64:
        case QUAD_CAST_TO_INT64: {
            uint32_t datasize = vars[q->op1.var].byte_size;
            enum VarDatatype datatype = vars[q->op1.var].datatype;
            if (datasize == 8 && (datatype == VARTYPE_UINT || 
                                  datatype == VARTYPE_SINT)) {
                if (!is_same(vars, q->op1.var, q->dest)) {
                    emit_op1_dest_move(q, vars, ctx);
                }
                break;
            }
            assert(vars[q->dest].alloc_type == ALLOC_REG);
            if (datasize == 4) {
                if (datatype == VARTYPE_UINT) {
                    asm_instr(ctx, OP_MOV);
                    emit_var_with_size(&vars[q->dest], 4, ctx);
                    emit_var(&vars[q->op1.var], ctx);
                    asm_instr_end(ctx);
                    break;
                } else if (datatype == VARTYPE_SINT) {
                    asm_instr(ctx, OP_MOVSXD);
                    emit_var(&vars[q->dest], ctx);
                    emit_var(&vars[q->op1.var], ctx);
                    asm_instr_end(ctx);
                    break;
                }
            } else if (datasize == 2 || datasize == 1) {
                if (datatype == VARTYPE_UINT || datatype == VARTYPE_BOOL) {
                    asm_instr(ctx, OP_MOVZX);
                } else if (datatype == VARTYPE_SINT) {
                    asm_instr(ctx, OP_MOVSX);
                } else {
                    assert(false);
                }
                emit_var(&vars[q->dest], ctx);
                emit_var(&vars[q->op1.var], ctx);
                asm_instr_end(ctx);
                break;
            }
            assert(false);
            break;
        }
        case QUAD_CAST_TO_UINT32:
        case QUAD_CAST_TO_INT32: {
            uint32_t datasize = vars[q->op1.var].byte_size;
            enum VarDatatype datatype = vars[q->op1.var].datatype;
            if ((datasize == 8 || datasize == 4) && 
                (datatype == VARTYPE_SINT || datatype == VARTYPE_UINT)) {
                if (!is_same(vars, q->op1.var, q->dest)) {
                    asm_instr(ctx, OP_MOV);
                    emit_var(&vars[q->dest], ctx);
                    emit_var_with_size(&vars[q->op1.var], 4, ctx);
                    asm_instr_end(ctx);
                }
                break;
            }
            assert(vars[q->dest].alloc_type == ALLOC_REG);
            if (datasize == 2 || datasize == 1) {
                if (datatype == VARTYPE_UINT || datatype == VARTYPE_BOOL) {
                    asm_instr(ctx, OP_MOVZX);
                } else if (datatype == VARTYPE_SINT) {
                    asm_instr(ctx, OP_MOVSX);
                } else {
                    assert(false);
                }
                emit_var(&vars[q->dest], ctx);
                emit_var(&vars[q->op1.var], ctx);
                asm_instr_end(ctx);
                break;
            }
            assert(false);
            break;
        }
        case QUAD_CAST_TO_INT16:
        case QUAD_CAST_TO_UINT16: {
            uint32_t datasize = vars[q->op1.var].byte_size;
            enum VarDatatype datatype = vars[q->op1.var].datatype;
            if ((datasize == 8 || datasize == 4 || datasize == 2) &&
                (datatype == VARTYPE_UINT || datatype == VARTYPE_SINT)) {
                if (!is_same(vars, q->op1.var, q->dest)) {
                    asm_instr(ctx, OP_MOV);
                    emit_var(&vars[q->dest], ctx);
                    emit_var_with_size(&vars[q->op1.var], 2, ctx);
                    asm_instr_end(ctx);
                }
                break;
            }
            assert(vars[q->dest].alloc_type == ALLOC_REG);
            if (datatype == VARTYPE_UINT || datatype == VARTYPE_BOOL) {
                asm_instr(ctx, OP_MOVZX);
            } else if (datatype == VARTYPE_SINT) {
                asm_instr(ctx, OP_MOVSX);
            } else {
                assert(false);
            }
            emit_var(&vars[q->dest], ctx);
            emit_var(&vars[q->op1.var], ctx);
            asm_instr_end(ctx);
            break;
        }
        case QUAD_CAST_TO_INT8:
        case QUAD_CAST_TO_UINT8: {
            enum VarDatatype datatype = vars[q->op1.var].datatype;
            assert(datatype == VARTYPE_UINT || datatype == VARTYPE_SINT ||
                   datatype == VARTYPE_BOOL);
            if (!is_same(vars, q->op1.var, q->dest)) {
                asm_instr(ctx, OP_MOV);
                emit_var(&vars[q->dest], ctx);
                emit_var_with_size(&vars[q->op1.var], 1, ctx);
                asm_instr_end(ctx);
            }
            break;
        }
        case QUAD_ARRAY_TO_PTR:
            assert(vars[q->op1.var].kind == VAR_ARRAY ||
                   vars[q->op1.var].kind == VAR_ARRAY_GLOBAL);
            assert(vars[q->dest].alloc_type == ALLOC_REG);
            asm_instr(ctx, OP_LEA);
            emit_var(&vars[q->dest], ctx);
            emit_var_with_size(&vars[q->op1.var], PTR_SIZE, ctx);
            asm_instr_end(ctx);
            break;
        case QUAD_JMP:
            if (q->next_quad->type == QUAD_LABEL &&
                q->next_quad->op1.label == q->op1.label) {
                break;
            }
            asm_instr(ctx, OP_JMP);
            asm_label_var(ctx, q->op1.label);
            asm_instr_end(ctx);
            break;
        case QUAD_SUB:
            emit_binop(OP_SUB, q, vars, ctx);
            break;
        case QUAD_ADD:
            if (q->scale != QUADSCALE_NONE) {
                assert(vars[q->dest].alloc_type == ALLOC_REG);
                assert(vars[q->op2].alloc_type == ALLOC_REG);
                if (vars[q->op1.var].alloc_type != ALLOC_REG) {
                    emit_op1_dest_move(q, vars, ctx);
                    asm_instr(ctx, OP_LEA);
                    emit_var(&vars[q->dest], ctx);
                    asm_mem_var(ctx, PTR_SIZE, vars[q->dest].reg,
                                scale_val(q->scale), vars[q->op2].reg, 0);
                } else {
                    asm_instr(ctx, OP_LEA);
                    emit_var(&vars[q->dest], ctx);
                    asm_mem_var(ctx, PTR_SIZE, vars[q->op1.var].reg,
                                scale_val(q->scale), vars[q->op2].reg, 0);
                }
                asm_instr_end(ctx);
            } else {
                emit_binop(OP_ADD, q, vars, ctx);
            }
            break;
        case QUAD_BIT_AND:
            emit_binop(OP_AND, q, vars, ctx);
            break;
        case QUAD_BIT_OR:
            emit_binop(OP_OR, q, vars, ctx);
            break;
        case QUAD_BIT_XOR:
            emit_binop(OP_XOR, q, vars, ctx);
        case QUAD_BOOL_NOT: {
            if (vars[q->op1.var].alloc_type == ALLOC_IMM) {
                asm_instr(ctx, OP_MOV);
                emit_var(&vars[q->dest], ctx);
                uint8_t v = vars[q->op1.var].imm.boolean ? 1 : 0;
                asm_imm_var(ctx, 1, &v);
                asm_instr_end(ctx);
                break;
            } else if (vars[q->op1.var].alloc_type == ALLOC_MEM) {
                asm_instr(ctx, OP_CMP);
                emit_var(&vars[q->op1.var], ctx);
                uint8_t v = 0;
                asm_imm_var(ctx, 1, &v);
                asm_instr_end(ctx);
            } else {
                asm_instr(ctx, OP_TEST);
                emit_var(&vars[q->op1.var], ctx);
                emit_var(&vars[q->op1.var], ctx);
                asm_instr_end(ctx);
            }
            asm_instr(ctx, OP_SETZ);
            emit_var(&vars[q->dest], ctx);
            asm_instr_end(ctx);
            break;
        }
        case QUAD_RETURN:
            if (vars[q->op1.var].alloc_type != ALLOC_REG ||
                vars[q->op1.var].reg != RAX) {
                uint32_t datasize = vars[q->op1.var].byte_size;
                asm_instr(ctx, OP_MOV);
                asm_reg_var(ctx, datasize, RAX);
                emit_var(&vars[q->op1.var], ctx);
                asm_instr_end(ctx);
            }
            if (q->next_quad == NULL ||
                (q->next_quad->type == QUAD_LABEL &&
                q->next_quad->op1.label == def->end_label)) {
                break;
            }
            asm_instr(ctx, OP_JMP);
            asm_label_var(ctx, def->end_label);
            asm_instr_end(ctx);
            break;
        case QUAD_GET_ARRAY_ADDR:
        case QUAD_CALC_ARRAY_ADDR: {
            assert(vars[q->op2].alloc_type == ALLOC_REG);
            assert(vars[q->dest].alloc_type == ALLOC_REG);
            assert(vars[q->op1.var].kind == VAR_ARRAY ||
                   vars[q->op1.var].kind == VAR_ARRAY_GLOBAL);
            uint32_t datasize = vars[q->dest].byte_size;
            asm_instr(ctx, OP_LEA);
            emit_var_with_size(&vars[q->dest], datasize, ctx);
            emit_var_with_size(&vars[q->op1.var], PTR_SIZE, ctx);
            asm_instr_end(ctx);
            if (type == QUAD_GET_ARRAY_ADDR) {
                asm_instr(ctx, OP_MOV);
            } else {
                asm_instr(ctx, OP_LEA);
            }
            emit_var(&vars[q->dest], ctx);
            asm_mem_var(ctx, datasize, vars[q->dest].reg,
                        scale_val(q->scale), vars[q->op2].reg, 0);
            asm_instr_end(ctx);
            break;
        }
        case QUAD_SET_ADDR: {
            assert(vars[q->op1.var].byte_size == PTR_SIZE);
            assert(vars[q->op1.var].alloc_type == ALLOC_REG);
            assert(vars[q->op2].alloc_type == ALLOC_REG);
            uint32_t datasize = vars[q->op2].byte_size;
            asm_instr(ctx, OP_MOV);

            asm_mem_var(ctx, datasize, vars[q->op1.var].reg, 0, RAX, 0);
            emit_var(&vars[q->op2], ctx);
            asm_instr_end(ctx);
            break;
        }
        case QUAD_ADDROF:
            assert(vars[q->dest].alloc_type == ALLOC_REG);
            assert(vars[q->op1.var].alloc_type == ALLOC_MEM);
            asm_instr(ctx, OP_LEA);
            asm_reg_var(ctx, PTR_SIZE, vars[q->dest].reg);
            emit_var_with_size(&vars[q->op1.var], 8, ctx);
            asm_instr_end(ctx);
            break;
        case QUAD_DEREF:
            assert(vars[q->dest].alloc_type == ALLOC_REG);
            assert(vars[q->op1.var].alloc_type == ALLOC_REG);
            uint32_t datasize = vars[q->dest].byte_size;
            asm_instr(ctx, OP_MOV);
            asm_reg_var(ctx, datasize, vars[q->dest].reg);
            asm_mem_var(ctx, datasize, vars[q->op1.var].reg, 0, RAX, 0);
            asm_instr_end(ctx);
            break;
        case QUAD_NEGATE:
            if (!is_same(vars, q->op1.var, q->dest)) {
                emit_op1_dest_move(q, vars, ctx);
            }
            asm_instr(ctx, OP_NEG);
            emit_var(&vars[q->dest], ctx);
            asm_instr_end(ctx);
            break;
        case QUAD_STRUCT_ADDR:
            assert(vars[q->op2].alloc_type == ALLOC_MEM);
            assert(vars[q->op2].datatype == VARTYPE_STRUCT);
            assert(vars[q->dest].alloc_type == ALLOC_REG);
            asm_instr(ctx, OP_LEA);
            emit_var(&vars[q->dest], ctx);
            if (var_local(vars[q->op2].kind)) {
                int32_t offset = vars[q->op2].memory.offset + q->op1.uint64;
                asm_mem_var(ctx, PTR_SIZE, RSP, 0, RAX, offset);
            } else {
                int32_t offset = q->op1.uint64;
                asm_global_mem_var(ctx, PTR_SIZE, vars[q->op2].symbol_ix, offset);
            }
            asm_instr_end(ctx);
            break;
        default:
            String_create(&out);
            fmt_quad(q, &out);
            _printf_e("Not implemented: %s\n", out.buffer);
            assert(false && "Not implemented");
            break;
        }
    }

    uint8_t* end_name = Arena_alloc_count(&ctx->arena, uint8_t, name_len + 6);
    end_name[0] = '_';
    end_name[1] = '_';
    memcpy(end_name + 2, name, name_len);
    end_name[name_len + 2] = '_';
    end_name[name_len + 3] = 'e';
    end_name[name_len + 4] = 'n';
    end_name[name_len + 5] = 'd';
    asm_put_label(ctx, def->end_label);

    asm_instr(ctx, OP_ADD);
    asm_reg_var(ctx, 8, RSP);
    asm_imm_var(ctx, 4, (const uint8_t*)&stack_size_u32);
    asm_instr_end(ctx);

    for (int32_t reg = GEN_REG_COUNT - 1; reg >= 0; --reg) {
        if (push_reg[reg]) {
            asm_instr(ctx, OP_POP);
            asm_reg_var(ctx, 8, reg);
            asm_instr_end(ctx);
        }
    }
    asm_instr(ctx, OP_RET);
    asm_instr_end(ctx);
}

static const uint8_t* str_literalname(uint64_t num, uint32_t* len, Arena* arena) {
    *len = 17; //__literal_string_

    uint64_t val = num;
    uint64_t pow = 1;
    do {
        *len += 1;
        val /= 10;
        pow *= 10;
    } while (val > 0);

    uint8_t* str = Arena_alloc_count(arena, uint8_t, *len);
    memcpy(str, "__literal_string_", 17);

    if (num == 0) {
        str[17] = '0';
        return str;
    }

    uint64_t i = 17;
    do {
        pow /= 10;
        uint64_t v = (num / pow) % 10;
        str[i++] = v + '0';
    } while (pow > 1);

    return str;
}

Object* Backend_generate_asm(NameTable *name_table, FunctionTable *func_table,
                             FunctionTable* externs, StringLiteral* literals,
                             Arena* arena, bool serialize) {
    Object* object = Mem_alloc(sizeof(Object));
    if (object == NULL) {
        out_of_memory(NULL);
    }
    Object_create(object);

    section_ix code_section = Object_create_section(object, SECTION_CODE);
    section_ix rdata_section = Object_create_section(object, SECTION_RDATA);

    AsmCtx ctx;
    asm_ctx_create(&ctx, object, code_section);

    StringLiteral* liter = literals;
    uint64_t i = 0;
    while (liter != NULL) {
        var_id v = liter->var;
        uint32_t len;
        const uint8_t* sym = str_literalname(i, &len, &ctx.arena);
        symbol_ix symbol = Object_declare_var(object, rdata_section, sym, len, 1, false);
        Object_append_data(object, rdata_section, liter->bytes, liter->len + 1);

        for (uint64_t ix = 0; ix < func_table->size; ++ix) {
            func_table->data[ix]->vars.data[v].symbol_ix = symbol;
        }
        ++i;
        liter = liter->next;
    }

    for (uint64_t i = 0; i < externs->size; ++i) {
        name_id id = externs->data[i]->name;
        const uint8_t* name = name_table->data[id].name;
        uint32_t name_len = name_table->data[id].name_len;
        var_id v = name_table->data[id].variable;

        symbol_ix sym = Object_declare_fn(object, SECTION_IX_NONE,
                                          name, name_len, true);
        externs->data[i]->symbol = sym;
        sym = Object_declare_import(object, SECTION_IX_NONE, name, name_len);

        for (uint64_t ix = 0; ix < func_table->size; ++ix) {
            func_table->data[ix]->vars.data[v].symbol_ix = sym;
        }
    }

    for (uint64_t i = 0; i < func_table->size; ++i) {
        FunctionDef* def = func_table->data[i];
        const uint8_t* name = name_table->data[def->name].name;
        uint32_t name_len = name_table->data[def->name].name_len;
        symbol_ix sym = Object_declare_fn(object, code_section, name,
                                          name_len, true);
        def->symbol = sym;
    }

    Amd64Op* start = ctx.start;
    for (uint64_t ix = 0; ix < func_table->size; ++ix) {
        FunctionDef* def = func_table->data[ix];
        object->symbols[def->symbol].offset = object->sections[code_section].data.size;
        Backend_generate_fn(def, arena, &ctx,
                            name_table, func_table);
        asm_assemble(&ctx, def->symbol);
        if (ix + 1 < func_table->size) {
            asm_reset(&ctx);
        }
    }

    LOG_INFO("Done generating functions");

    ctx.start = start;
    String out;
    if (serialize && String_create(&out)) {
        asm_serialize(&ctx, &out);
        outputUtf8(out.buffer, out.length);
    }

    return object;
}


bool Backend_arg_is_ptr(AllocInfo info) {
    return !(info.size == 0 || info.size == 1 || info.size == 2 ||
             info.size == 4 || info.size == 8);
}

bool Backend_return_as_ptr(AllocInfo info) {
    return Backend_arg_is_ptr(info);
}
