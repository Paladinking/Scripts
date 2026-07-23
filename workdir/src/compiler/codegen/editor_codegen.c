#include "editor_codegen.h"
#include "editor_asm.h"
#include "codegen.h"
#include "../format.h"

#define PTR_SIZE 4
#define LOG_CATAGORY LOG_CATAGORY_REGISTER_ALLOCATION

// Calling convention:
// r8 = arg 0
// r9 = arg 1
// r10 = arg 2
// r11 = arg 3
// r12 = arg 4
// After that on stack
//
// r15 is return register

// All integer binary operations that are simple in amd64 asm
static bool is_standard_binop(Quad* q) {
    switch (q->type) {
    case QUAD_ADD:
        if (q->scale != QUADSCALE_NONE) {
            return false;
        }
        break;
    case QUAD_MUL:
    case QUAD_SUB:
    case QUAD_BIT_AND:
    case QUAD_BIT_OR:
    case QUAD_BIT_XOR:
        break;
    default:
        return false;
    }

    return true;
}

uint64_t editor_get_regs() {
    return GEN_REG_COUNT;
}

void editor_inital_contraints(ConflictGraph* graph, VarList* vars) {
    ConflictGraph_clear(graph);

    // Add edges between all registers
    for (uint64_t col = 0; col < graph->reg_count; ++col){
        for (uint64_t row = col + 1; row < graph->reg_count; ++row) {
            ConflictGraph_add_edge(graph, row, col);
        }
    }

    for (uint64_t ix = 0; ix < graph->var_count; ++ix) {
        // Do not allow use of stack pointer
        for (uint64_t reg = R0; reg <= R7; ++reg) {
            ConflictGraph_add_edge(graph, reg, ix + graph->reg_count);
        }
    }
}


// Create a temporary register variable that replaces var, and
// insert a move from var to this variable before quad
static void preinsert_move(ConflictGraph* graph, var_id* var, VarSet* live_set,
                    Quad* quad, FlowNode* node, VarList* vars, Arena* arena) {

    var_id tmp = create_temp_var(graph, vars, node, live_set, *var);
    for (uint64_t reg = R0; reg <= R7; ++reg) {
        ConflictGraph_add_edge(graph, reg, tmp + graph->reg_count);
    }
    Quad* nq = Arena_alloc_type(arena, Quad);
    uint32_t datasize = vars->data[*var].byte_size;
    assert(datasize == 1 || datasize == 2 || datasize == 4);
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

// Create a temporary register variable that replaces dest, and
// insert a move from this variable to dest after quad
static void postinsert_move(ConflictGraph* graph, var_id* dest, VarSet* live_set,
                     Quad* quad, FlowNode* node, VarList* vars, Arena* arena) {
    var_id tmp = create_temp_var(graph, vars, node, live_set, *dest);
    for (uint64_t reg = R0; reg <= R7; ++reg) {
        ConflictGraph_add_edge(graph, reg, tmp + graph->reg_count);
    }
    Quad* nq = Arena_alloc_type(arena, Quad);
    uint32_t datasize = vars->data[*dest].byte_size;
    assert(datasize == 1 || datasize == 2 || datasize == 4);
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

static void require_reg_var(ConflictGraph* graph, var_id* var, 
                            VarSet* live_set, Quad* quad, FlowNode* node,
                            VarList* vars, Arena* arena) {
    if (vars->data[*var].alloc_type == ALLOC_MEM) {
        preinsert_move(graph, var, live_set, quad, node, vars, arena);
    }
}

static void require_reg_dest(ConflictGraph* graph, var_id* dest, 
                             VarSet* live_set, Quad* quad, FlowNode* node,
                             VarList* vars, Arena* arena) {
    if (vars->data[*dest].alloc_type == ALLOC_MEM) {
        postinsert_move(graph, dest, live_set, quad, node, vars, arena);
    }
}

// c = a - b

void editor_add_constrains(ConflictGraph* graph, VarSet* live_set, Quad* quad,
                            VarList* vars, FlowNode* node, Arena* arena) {
    enum QuadType q = quad->type;
    if (is_standard_binop(quad)) {
        require_reg_dest(graph, &quad->dest, live_set, quad, node, vars, arena);
        require_reg_var(graph, &quad->op2, live_set, quad, node, vars, arena);
        if (quad->dest != quad->op2 && quad->dest != quad->op1.var) {
            ConflictGraph_add_edge(graph, quad->dest + graph->reg_count,
                                   quad->op2 + graph->reg_count);
        }
    } else if (q == QUAD_MOVE) {
        if (vars->data[quad->op1.var].alloc_type == ALLOC_MEM &&
            vars->data[quad->dest].alloc_type == ALLOC_MEM) {
            require_reg_var(graph, &quad->op1.var, live_set, quad, node, vars, arena);
        }
    } else if (q == QUAD_RETURN) {
        // Nothing for now...
    } else if (q == QUAD_GET_ARG) {
        require_reg_dest(graph, &quad->dest, live_set, quad, node, vars, arena);
        if (quad->op1.uint64 < 5) {
            uint64_t reg = R8 + quad->op1.uint64;
            require_reg(graph, reg, quad->dest);
        }
    } else if (q == QUAD_PUT_ARG) {
        if (quad->op1.uint64 < 5) {
            uint64_t reg = R8 + quad->op1.uint64;
            var_id v = VarSet_get(live_set);
            while (v != VAR_ID_INVALID) {
                if (v != quad->op2) {
                    ConflictGraph_add_edge(graph, v + graph->reg_count, reg);
                }
                v = VarSet_getnext(live_set, v);
            }
            require_reg_var(graph, &quad->op2, live_set, quad, node, vars, arena);
            require_reg(graph, reg, quad->op2);
        } else {
            require_reg_var(graph, &quad->op2, live_set, quad, node, vars, arena);
        }
    } else if (q == QUAD_GET_RET) {
        require_reg_dest(graph, &quad->dest, live_set, quad, node, vars, arena);
        require_reg(graph, R15, quad->dest);
    } else if (q == QUAD_CALL || q == QUAD_CALL_PTR) {
        // Nothing for now...
    } else if (q == QUAD_CAST_TO_UINT32 || q == QUAD_CAST_TO_INT32) {
        require_reg_dest(graph, &quad->dest, live_set, quad, node, vars, arena);
    } else if (q == QUAD_CREATE) {
        uint64_t data_size = vars->data[quad->dest].byte_size;
        enum VarDatatype datatype = vars->data[quad->dest].datatype;
        if (data_size == 4 && (
            (datatype == VARTYPE_UINT && quad->op1.uint64 > UINT16_MAX) ||
            (datatype == VARTYPE_SINT && (quad->op1.sint64 > INT16_MAX ||
                                          quad->op1.sint64 < INT16_MIN)))) {
            // Variable does not fit into 16-bits
            require_reg_dest(graph, &quad->dest, live_set, quad, node, vars, arena);
            return;
        }
        Quad* next = quad->next_quad;
        if (next == NULL || !fully_local(vars, quad->dest)) {
            require_reg_dest(graph, &quad->dest, live_set, quad, node, vars, arena);
            return;
        }
        if (next->type != QUAD_ADD && next->type != QUAD_CMP_EQ && 
            next->type != QUAD_CMP_NEQ) {
            // Only support ADDI and TEQI
            require_reg_dest(graph, &quad->dest, live_set, quad, node, vars, arena);
            return;
        }

        if (next->op2 == quad->dest || next->op1.var == quad->dest) {
            if (vars->data[quad->dest].alloc_type == ALLOC_NONE) {
                if (datatype == VARTYPE_SINT) {
                    vars->data[quad->dest].imm.int64 = quad->op1.sint64;
                } else if (datatype == VARTYPE_UINT) {
                    vars->data[quad->dest].imm.uint64 = quad->op1.uint64;
                } else {
                    assert(datatype == VARTYPE_BOOL);
                    vars->data[quad->dest].imm.boolean = quad->op1.uint64 != 0;
                }
                return;
            }
        }
        require_reg_dest(graph, &quad->dest, live_set, quad, node, vars, arena);
    } else if (q == QUAD_LABEL) {
        // Pass
    } else {
        String out;
        String_create(&out);
        fmt_quad(quad, &out);
        _printf_e("Not implemented: %s\n", out.buffer);
        assert(false && "Not implemented");
    }
}

Object* editor_generate_asm(NameTable* name_table, FunctionTable* func_table,
                            FunctionTable* externs, StringLiteral* literals,
                            Arena* arena, bool serialize) {

    Object* object = Mem_alloc(sizeof(Object));
    if (object == NULL) {
        out_of_memory(NULL);
    }
    Object_create(object);

    section_ix code_setction = Object_create_section(object, SECTION_CODE);


    assert(false);
    return NULL;
}

bool editor_arg_is_ptr(AllocInfo info) {
    return info.size > 4;
}

bool editor_return_as_ptr(AllocInfo info) {
    return editor_arg_is_ptr(info);
}
