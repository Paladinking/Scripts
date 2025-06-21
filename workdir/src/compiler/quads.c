#include "quads.h"
#include "mem.h"

static void out_of_memory(void* ctx) {
    Parser* parser = ctx;
    LineInfo i;
    i.real = false;
    fatal_error(parser, PARSE_ERROR_OUTOFMEMORY, i);
}


typedef struct QuadList {
    Quad* quads;
    uint64_t quad_count;
    uint64_t var_count;
    uint64_t quad_cap;
    Arena* arena;
} QuadList;

Quad* QuadList_addquad(QuadList* quads, uint64_t type, var_id dest) {
    if (quads->quad_count == quads->quad_cap) {
        quads->quad_cap += 256;
        Arena_alloc_count(quads->arena, Quad, 256);
    }
    quads->quads[quads->quad_count].type = type;
    quads->quads[quads->quad_count].dest = dest;
    quads->quad_count += 1;
    return quads->quads + quads->quad_count - 1;
}

var_id QuadList_addvar(QuadList* quads, name_id name, type_id type) {
    if (quads->var_count == quads->quad_cap) {
        quads->quad_cap += 256;
        Arena_alloc_count(quads->arena, Quad, 256);
    }
    quads->quads[quads->var_count].var_entry.name = name;
    quads->quads[quads->var_count].var_entry.type = type;
    quads->quads[quads->var_count].var_entry.reads = 0;
    quads->quads[quads->var_count].var_entry.writes = 0;
    quads->quads[quads->var_count].var_entry.array_size = NULL;
    quads->var_count += 1;
    return quads->var_count - 1;
}

label_id QuadList_addlabel(QuadList* quads, quad_id dest) {
    if (quads->var_count == quads->quad_cap) {
        quads->quad_cap += 256;
        Arena_alloc_count(quads->arena, Quad, 256);
    }
    quads->quads[quads->var_count].label = dest;
    quads->var_count += 1;
    return quads->var_count - 1;
}

void QuadList_setlabel(QuadList* quads, label_id label_id, quad_id label) {
    quads->quads[label_id].label = label;
}

quad_id QuadList_getlabel(QuadList* quads, label_id label_id) {
    return quads->quads[label_id].label;
}

VarData* QuadList_getvar(QuadList* quads, var_id id) {
    return &quads->quads[id].var_entry;
}


uint64_t quad_datatype(Parser* parser, type_id type) {
    if (parser->type_table.data[type].kind == TYPE_ARRAY) {
        return QUAD_PTR | QUAD_PTR_SIZE;
    }
    uint64_t mask;
    switch (type) {
    case TYPE_ID_UINT64:
        return QUAD_64_BIT | QUAD_UINT;
    case TYPE_ID_INT64:
        return QUAD_64_BIT | QUAD_SINT;
    case TYPE_ID_FLOAT64:
        return QUAD_64_BIT | QUAD_FLOAT;
    case TYPE_ID_BOOL:
        return QUAD_8_BIT | QUAD_BOOL;
    case TYPE_ID_CSTRING:
        return QUAD_PTR_SIZE | QUAD_PTR;
    case TYPE_ID_NONE:
        return QUAD_0_BIT | QUAD_UINT;
    }
    LineInfo l = {0, 0, false};
    fatal_error(parser, PARSE_ERROR_INTERNAL, l);
    return 0;
}

uint64_t quad_scale(Parser* parser, type_id type) {
    uint64_t mask = quad_datatype(parser, type);
    if (mask & QUAD_64_BIT) {
        return QUAD_SCALE_8;
    } else if (mask & QUAD_32_BIT) {
        return QUAD_SCALE_4;
    } else if (mask & QUAD_16_BIT) {
        return QUAD_SCALE_2;
    } else {
        return QUAD_SCALE_1;
    }
}

Expression* get_subexpr(Expression* e, uint64_t ix) {
    switch (e->kind) {
    case EXPRESSION_BINOP:
        if (ix == 0) {
            return e->binop.lhs;
        } else if (ix == 1) {
            return e->binop.rhs;
        }
        return NULL;
    case EXPRESSION_CALL:
        if (ix == 0) {
            return e->call.function;
        } else if (ix <= e->call.arg_count) {
            return e->call.args[ix - 1];
        }
        return NULL;
    case EXPRESSION_ARRAY_INDEX:
        if (ix == 0) {
            return e->array_index.array;
        } else if (ix == 1) {
            return e->array_index.index;
        }
        return NULL;
    case EXPRESSION_UNOP:
        return ix == 0 ? e->unop.expr : NULL;
    case EXPRESSION_CAST:
        return ix == 0 ? e->unop.expr : NULL;
    default:
        return NULL;
    }
}

var_id quads_unop(Parser* parser, Expression* expr, QuadList* quads,
                  var_id dest) {
    var_id v = expr->unop.expr->var;
    uint64_t quad = quad_datatype(parser, QuadList_getvar(quads, v)->type);

    if (dest == VAR_ID_INVALID) {
        if (expr->unop.op == UNOP_POSITIVE || expr->unop.op == UNOP_PAREN) {
            // Positive and paren is a no-op if no move is needed
            return v;
        }
        dest = QuadList_addvar(quads, NAME_ID_INVALID, expr->type);
    }

    switch (expr->unop.op) {
    case UNOP_BITNOT:
        quad |= QUAD_BIT_NOT;
        break;
    case UNOP_BOOLNOT:
        quad |= QUAD_BOOL_NOT;
        break;
    case UNOP_NEGATVIE:
        quad |= QUAD_NEGATE;
        break;
    case UNOP_PAREN:
    case UNOP_POSITIVE:
        quad |= QUAD_MOVE;
        break;
    default:
        fatal_error(parser, PARSE_ERROR_INTERNAL, expr->line);
        break;
    }
    Quad* q = QuadList_addquad(quads, quad, dest);
    q->op1.var = v;
    return dest;
}

var_id quads_binop(Parser* parser, Expression* expr, QuadList* quads,
                   var_id dest, label_id label) {
    if (dest == VAR_ID_INVALID) {
        dest = QuadList_addvar(quads, NAME_ID_INVALID, expr->type);
    }
    if (expr->binop.op == BINOP_BOOL_AND || expr->binop.op == BINOP_BOOL_OR) {
        assert(QuadList_getvar(quads, dest)->type == TYPE_ID_BOOL);
        if (label != LABEL_ID_INVALID) {
            Quad* qw = QuadList_addquad(quads, QUAD_LABEL, VAR_ID_INVALID);
            qw->op1.label = label;
            quad_id q = quads->quad_count - 1;
            QuadList_setlabel(quads, label, q);
        }
        return dest;
    }
    assert(label == LABEL_ID_INVALID);
    var_id lhs = expr->binop.lhs->var;
    var_id rhs = expr->binop.rhs->var;
    uint64_t quad = quad_datatype(parser, QuadList_getvar(quads, lhs)->type);

    switch (expr->binop.op) {
    case BINOP_DIV:
        quad |= QUAD_DIV;
        break;
    case BINOP_MUL:
        quad |= QUAD_MUL;
        break;
    case BINOP_MOD:
        quad |= QUAD_MOD;
        break;
    case BINOP_SUB:
        quad |= QUAD_SUB;
        break;
    case BINOP_ADD:
        quad |= QUAD_ADD;
        break;
    case BINOP_BIT_LSHIFT:
        quad |= QUAD_LSHIFT;
        break;
    case BINOP_BIT_RSHIFT:
        quad |= QUAD_RSHIFT;
        break;
    case BINOP_CMP_LE:
        quad |= QUAD_CMP_LE;
        break;
    case BINOP_CMP_GE:
        quad |= QUAD_CMP_GE;
        break;
    case BINOP_CMP_G:
        quad |= QUAD_CMP_G;
        break;
    case BINOP_CMP_L:
        quad |= QUAD_CMP_L;
        break;
    case BINOP_CMP_EQ:
        quad |= QUAD_CMP_EQ;
        break;
    case BINOP_CMP_NEQ:
        quad |= QUAD_CMP_NEQ;
        break;
    case BINOP_BIT_AND:
        quad |= QUAD_BIT_AND;
        break;
    case BINOP_BIT_XOR:
        quad |= QUAD_BIT_XOR;
        break;
    case BINOP_BIT_OR:
        quad |= QUAD_BIT_OR;
        break;
    default:
        fatal_error(parser, PARSE_ERROR_INTERNAL, expr->line);
    }
    Quad* q = QuadList_addquad(quads, quad, dest);
    q->op1.var = lhs;
    q->op2 = rhs;
    return dest;
}

var_id quads_cast(Parser* parser, Expression* expr, QuadList* quads,
                  var_id dest) {
    type_id src_type = QuadList_getvar(quads, expr->cast.e->var)->type;
    if (dest == VAR_ID_INVALID) {
        if (src_type == expr->type) {
            // cast to same type: no-op
            return expr->cast.e->var;
        }
        dest = QuadList_addvar(quads, NAME_ID_INVALID, expr->type);
    }
    uint64_t datatype = quad_datatype(parser, src_type);

    Quad* q;
    if (src_type == expr->type) {
        q = QuadList_addquad(quads, QUAD_MOVE | datatype, dest);
    } else if (expr->type == TYPE_ID_BOOL) {
        q = QuadList_addquad(quads, QUAD_CAST_TO_BOOL | datatype, dest);
    } else if (expr->type == TYPE_ID_FLOAT64) {
        q = QuadList_addquad(quads, QUAD_CAST_TO_FLOAT64 | datatype, dest);
    } else if (expr->type == TYPE_ID_UINT64) {
        q = QuadList_addquad(quads, QUAD_CAST_TO_UINT64 | datatype, dest);
    } else if (expr->type == TYPE_ID_INT64) {
        q = QuadList_addquad(quads, QUAD_CAST_TO_INT64 | datatype, dest);
    } else {
        fatal_error(parser, PARSE_ERROR_INTERNAL, expr->line);
        return dest;
    }
    q->op1.var = expr->cast.e->var;
    return dest;
}

var_id quads_arrayindex(Parser* parser, Expression* expr, QuadList* quads,
                        var_id dest) {
    if (dest == VAR_ID_INVALID) {
        dest = QuadList_addvar(quads, NAME_ID_INVALID, expr->type);
    }
    var_id ix = expr->array_index.index->var;
    type_id ix_type = QuadList_getvar(quads, ix)->type;
    uint64_t datatype = quad_datatype(parser, ix_type);
    datatype |= quad_scale(parser, expr->type);
    Quad* q = QuadList_addquad(quads, QUAD_GET_ADDR | datatype, dest);
    q->op1.var = expr->array_index.array->var;
    q->op2 = expr->array_index.index->var;

    return dest;
}

var_id quads_call(Parser* parser, Expression* expr, QuadList* quads,
                  var_id dest) {

    if (dest == VAR_ID_INVALID) {
        dest = QuadList_addvar(quads, NAME_ID_INVALID, expr->type);
    }

    for (uint64_t ix = 0; ix < expr->call.arg_count; ++ix) {
        var_id v = expr->call.args[ix]->var;
        type_id t = QuadList_getvar(quads, v)->type;
        uint64_t quad = quad_datatype(parser, t);
        Quad* q = QuadList_addquad(quads, QUAD_PUT_ARG | quad, VAR_ID_INVALID);
        q->op1.var = v;
    }

    var_id f = expr->call.function->var;
    Quad* q = QuadList_addquad(quads, QUAD_CALL, VAR_ID_INVALID);
    q->op1.var = f;

    uint64_t quad = quad_datatype(parser, expr->type);
    QuadList_addquad(quads, QUAD_GET_RET | quad, dest);

    return dest;
}

void reserve(Parser* parser, void** ptr, uint64_t size, uint64_t* cap) {
    if (*cap >= size) {
        return;
    }
    while (size > *cap) {
        (*cap) *= 2;
    }
    void* n = Mem_realloc(*ptr, *cap);
    if (n == NULL) {
        out_of_memory(parser);
    }
    *ptr = n;
}

#define RESERVE(p, ptr, size, cap) reserve(p, (void**)(&ptr), size, &cap)



var_id quads_expression(Parser* parser, Expression* expr, QuadList* quads,
                        label_id jmp_label, enum QuadType jmp_quad, 
                        var_id dest, bool needs_var) {
    struct Node {
        Expression* e;
        uint64_t ix;
        label_id jmp_label;
        enum QuadType jmp_quad;
        bool needs_var;
        var_id dest;
        label_id lbl;
    };

    struct Node* stack = Mem_alloc(8 * sizeof(struct Node));
    if (stack == NULL) {
        out_of_memory(parser);
    }
    uint64_t stack_size = 0;
    uint64_t stack_cap = 8;
    Expression* e = expr;

    while (1) {
loop:
        RESERVE(parser, stack, stack_size + 1, stack_cap);
        Expression* subexpr = get_subexpr(e, 0);
        if (subexpr != NULL) {
            stack[stack_size] = (struct Node){e, 1, jmp_label, jmp_quad, 
                                              needs_var, dest, 
                                              LABEL_ID_INVALID};
            ++stack_size;
            if (e->kind == EXPRESSION_BINOP && (
             e->binop.op == BINOP_BOOL_AND || e->binop.op == BINOP_BOOL_OR)) {
                if (needs_var && dest == VAR_ID_INVALID) {
                    dest = QuadList_addvar(quads, NAME_ID_INVALID, e->type);
                    stack[stack_size - 1].dest = dest;
                }
                if (jmp_quad == QUAD_JMP_FALSE) {
                    assert(!needs_var);
                    if (e->binop.op == BINOP_BOOL_OR) {
                        jmp_quad = QUAD_JMP_TRUE;
                        jmp_label = QuadList_addlabel(quads, QUAD_ID_INVALID);
                        stack[stack_size - 1].lbl = jmp_label;
                    }
                } else if (jmp_quad == QUAD_JMP_TRUE) {
                    assert(!needs_var);
                    if (e->binop.op == BINOP_BOOL_AND) {
                        jmp_quad = QUAD_JMP_FALSE;
                        jmp_label = QuadList_addlabel(quads, QUAD_ID_INVALID);
                        stack[stack_size - 1].lbl = jmp_label;
                    }
                } else {
                    jmp_label = QuadList_addlabel(quads, QUAD_ID_INVALID);
                    stack[stack_size - 1].lbl = jmp_label;
                    if (e->binop.op == BINOP_BOOL_OR) {
                        jmp_quad = QUAD_JMP_TRUE;
                    } else {
                        jmp_quad = QUAD_JMP_FALSE;
                    }
                }
            } else {
                jmp_quad = 0;
                jmp_label = LABEL_ID_INVALID;
                needs_var = true;
                dest = VAR_ID_INVALID;
            }
            e = subexpr;
            continue;
        }

        if (e->kind != EXPRESSION_VARIABLE) {
            uint64_t datatype = quad_datatype(parser, e->type);
            if (dest == VAR_ID_INVALID) {
                dest = QuadList_addvar(quads, NAME_ID_INVALID, e->type);
            }
            Quad* q;
            switch(e->kind) {
            case EXPRESSION_LITERAL_BOOL:
                q = QuadList_addquad(quads, QUAD_CREATE | datatype, dest);
                q->op1.uint64 = e->literal.uint;
                break;
            case EXPRESSION_LITERAL_INT:
                q = QuadList_addquad(quads, QUAD_CREATE | datatype, dest);
                q->op1.sint64 = e->literal.iint;
                break;
            case EXPRESSION_LITERAL_UINT:
                q = QuadList_addquad(quads, QUAD_CREATE | datatype, dest);
                q->op1.uint64 = e->literal.uint;
                break;
            case EXPRESSION_LITERAL_FLOAT:
                q = QuadList_addquad(quads, QUAD_CREATE | datatype, dest);
                q->op1.float64 = e->literal.float64;
                break;
            case EXPRESSION_LITERAL_STRING:
            default:
                fatal_error(parser, PARSE_ERROR_INTERNAL, e->line);
                break;
            }
        } else {
            var_id id = parser->name_table.data[e->variable.ix].variable;
            if (dest == VAR_ID_INVALID) {
                dest = id;
            } else {
                type_id t = QuadList_getvar(quads, id)->type;
                uint64_t datatype = quad_datatype(parser, t);
                Quad* q = QuadList_addquad(quads, QUAD_MOVE | datatype, dest);
                q->op1.var = id;
            }
        }
        e->var = dest;
        if (jmp_quad == QUAD_JMP_TRUE || jmp_quad == QUAD_JMP_FALSE) {
            assert(jmp_label != LABEL_ID_INVALID);
            assert(QuadList_getvar(quads, dest)->type == TYPE_ID_BOOL);
            Quad* q = QuadList_addquad(quads, jmp_quad, VAR_ID_INVALID);
            q->op1.label = jmp_label;
            q->op2 = dest;
        }

        while (stack_size > 0) {
            Expression* top = stack[stack_size - 1].e;
            Expression* subexpr = get_subexpr(top, stack[stack_size - 1].ix);
            jmp_quad = stack[stack_size - 1].jmp_quad;
            jmp_label = stack[stack_size - 1].jmp_label;
            if (subexpr != NULL) {
                dest = VAR_ID_INVALID;
                if (top->kind == EXPRESSION_BINOP && 
                    (top->binop.op == BINOP_BOOL_OR || 
                     top->binop.op == BINOP_BOOL_AND)) {
                    // Jump are added by children already
                    stack[stack_size - 1].jmp_quad = 0;
                    needs_var = stack[stack_size - 1].needs_var;
                    if (needs_var) {
                        dest = stack[stack_size - 1].dest;
                    }
                } else {
                    jmp_quad = 0;
                    jmp_label = LABEL_ID_INVALID;
                    needs_var = true;
                }
                e = subexpr;
                ++stack[stack_size - 1].ix;
                goto loop;
            }
            dest = stack[stack_size - 1].dest;
            label_id lbl = stack[stack_size - 1].lbl;
            e = top;
            --stack_size;
            if (e->kind == EXPRESSION_BINOP) {
                dest = quads_binop(parser, e, quads, dest, lbl);
            } else if (e->kind == EXPRESSION_UNOP) {
                dest = quads_unop(parser, e, quads, dest);
            } else if (e->kind == EXPRESSION_CALL) {
                dest = quads_call(parser, e, quads, dest);
            } else if (e->kind == EXPRESSION_CAST) {
                dest = quads_cast(parser, e, quads, dest);
            } else if (e->kind == EXPRESSION_ARRAY_INDEX) {
                dest = quads_arrayindex(parser, e, quads, dest);
            } else {
                fatal_error(parser, PARSE_ERROR_INTERNAL, e->line);
            }
            e->var = dest;
            if (jmp_quad != 0) {
                assert(lbl == LABEL_ID_INVALID);
                assert(jmp_label != LABEL_ID_INVALID);
                Quad* q = QuadList_addquad(quads, jmp_quad, VAR_ID_INVALID);
                q->op1.label = jmp_label;
                q->op2 = dest;
            }
        }
        break;
    }
    Mem_free(stack);
    return dest;
}


void quads_statements(Parser* parser, QuadList* quads, Statement** statements,
                      uint64_t count) {
    struct Node {
        Statement* s;
        uint64_t ix;
        label_id l1, l2;
    };

    uint64_t stack_cap = count + 8;
    uint64_t stack_size = count;
    struct Node* stack = Mem_alloc((stack_cap) * sizeof(struct Node));
    if (stack == NULL) {
        out_of_memory(parser);
    }

    for (uint64_t ix = 0; ix < count; ++ix) {
        stack[ix] = (struct Node) {statements[count - ix - 1], 0,
                                   LABEL_ID_INVALID, LABEL_ID_INVALID};
    }
    
    while (stack_size > 0) {
        Statement* s = stack[stack_size - 1].s;
        uint64_t ix = stack[stack_size - 1].ix;
        label_id l1 = stack[stack_size - 1].l1;
        label_id l2 = stack[stack_size - 1].l2;
        --stack_size;
        if (s->type == STATEMENT_WHILE) {
            if (ix == 0) {
                label_id label = QuadList_addlabel(quads, quads->quad_count);
                Quad* q = QuadList_addquad(quads, QUAD_LABEL, VAR_ID_INVALID);
                q->op1.label = label;
                label_id l2 = QuadList_addlabel(quads, QUAD_ID_INVALID);
                quads_expression(parser, s->while_.condition, quads, l2, 
                                 QUAD_JMP_FALSE, VAR_ID_INVALID, false);
                ++stack_size;
                stack[stack_size - 1].ix += 1;
                stack[stack_size - 1].l1 = label;
                stack[stack_size - 1].l2 = l2;
                uint64_t c = s->while_.statement_count;
                RESERVE(parser, stack, stack_size + c, stack_cap);
                for (uint64_t ix = 0; ix < c; ++ix) {
                    stack[stack_size + ix] = (struct Node) 
                        {s->while_.statements[c - ix - 1], 0,
                        LABEL_ID_INVALID, LABEL_ID_INVALID};
                }
                stack_size += s->while_.statement_count;
            } else {
                Quad* q = QuadList_addquad(quads, QUAD_JMP, VAR_ID_INVALID);
                q->op1.label = l1;
                QuadList_setlabel(quads, l2, quads->quad_count);
                q = QuadList_addquad(quads, QUAD_LABEL, VAR_ID_INVALID);
                q->op1.label = l2;
            }
            continue;
        } else if (s->type == STATEMENT_IF) {
            if (ix == 0) {
                assert(s->if_.condition != NULL);
                label_id l1 = QuadList_addlabel(quads, QUAD_ID_INVALID);
                label_id l2 = l1;
                if (s->if_.else_branch != NULL) {
                    l2 = QuadList_addlabel(quads, QUAD_ID_INVALID);
                }
                quads_expression(parser, s->if_.condition, quads, l2,
                                 QUAD_JMP_FALSE, VAR_ID_INVALID, false);
                ++stack_size;
                stack[stack_size - 1].ix += 1;
                stack[stack_size - 1].l1 = l1;
                stack[stack_size - 1].l2 = l2;
                uint64_t c = s->if_.statement_count;
                RESERVE(parser, stack, stack_size + c, stack_cap);
                for (uint64_t ix = 0; ix < c; ++ix) {
                    stack[stack_size + ix] = (struct Node) 
                        {s->if_.statements[c - ix - 1], 0,
                        LABEL_ID_INVALID, LABEL_ID_INVALID};
                }
                stack_size += c;
            } else {
                if (s->if_.else_branch == NULL) {
                    QuadList_setlabel(quads, l1, quads->quad_count);
                    Quad* q = QuadList_addquad(quads, QUAD_LABEL, 
                            VAR_ID_INVALID);
                    q->op1.label = l1;
                    continue;
                }
                Quad* q = QuadList_addquad(quads, QUAD_JMP, VAR_ID_INVALID);
                q->op1.label = l1;
                QuadList_setlabel(quads, l2, quads->quad_count);
                q = QuadList_addquad(quads, QUAD_LABEL, VAR_ID_INVALID);
                q->op1.label = l2;
                s = s->if_.else_branch;
                l2 = l1;
                if (s->if_.else_branch != NULL) {
                    l2 = QuadList_addlabel(quads, QUAD_ID_INVALID);
                }
                if (s->if_.condition != NULL) {
                    quads_expression(parser, s->if_.condition, quads, l2,
                                     QUAD_JMP_FALSE, VAR_ID_INVALID, false);
                }
                ++stack_size;
                stack[stack_size - 1].s = s;
                stack[stack_size - 1].l2 = l2;
                uint64_t c = s->if_.statement_count;
                RESERVE(parser, stack, stack_size + c, stack_cap);
                for (uint64_t ix = 0; ix < c; ++ix) {
                    stack[stack_size + ix] = (struct Node) 
                        {s->if_.statements[c - ix - 1], 0,
                        LABEL_ID_INVALID, LABEL_ID_INVALID};
                }
                stack_size += c;
            }
            continue;
        }

        if (s->type == STATEMENT_ASSIGN) {
            Expression* lhs = s->assignment.lhs;
            Expression* rhs = s->assignment.rhs;
            if (lhs->kind == EXPRESSION_VARIABLE) {
                var_id v = parser->name_table.data[lhs->variable.ix].variable;
                quads_expression(parser, rhs, quads, LABEL_ID_INVALID, 
                                 0, v, true);
            } else if (lhs->kind == EXPRESSION_ARRAY_INDEX) {
                type_id rhs_type = rhs->type;
                var_id v = quads_expression(parser, rhs, quads,
                                            LABEL_ID_INVALID, 0,
                                            VAR_ID_INVALID, true);
                type_id ix_type = lhs->array_index.index->type;
                var_id i = quads_expression(parser, lhs->array_index.index, 
                                            quads, LABEL_ID_INVALID, 
                                            0, VAR_ID_INVALID, true);
                type_id arr_type = lhs->array_index.array->type;
                var_id arr = quads_expression(parser, 
                                 lhs->array_index.array, quads, 
                                 LABEL_ID_INVALID, 0, VAR_ID_INVALID, true);
                var_id a = QuadList_addvar(quads, NAME_ID_INVALID, 
                                           arr_type);
                uint64_t datatype = quad_datatype(parser, ix_type);
                datatype |= quad_scale(parser, rhs_type);
                Quad* q = QuadList_addquad(quads, QUAD_CALC_ADDR | datatype, a);
                q->op1.var = arr;
                q->op2 = i;
                datatype = quad_datatype(parser, rhs_type);
                q = QuadList_addquad(quads, QUAD_SET_ADDR | datatype,
                                     VAR_ID_INVALID);
                q->op1.var = a;
                q->op2 = v;
            }
            continue;
        } else if (s->type == STATEMENT_EXPRESSION) {
            quads_expression(parser, s->expression, quads, LABEL_ID_INVALID,
                             0, VAR_ID_INVALID, false);
            continue;
        } else if (s->type == STATEMENT_RETURN) {
            Expression* ret = s->return_.return_value;
            uint64_t datatype = quad_datatype(parser, ret->type);
            var_id v = quads_expression(parser, ret, quads,
                    LABEL_ID_INVALID, 0, VAR_ID_INVALID, true);
            Quad* q = QuadList_addquad(quads, QUAD_RETURN | datatype, 
                                       VAR_ID_INVALID);
            q->op1.var = v;
            continue;
        }
    }
}

void Quad_GenerateQuads(Parser* parser, Quads* quads, Arena* arena) {
    QuadList list = {NULL, 0, 0, 256, arena};
    list.quads = Arena_alloc_count(arena, Quad, 256);
    for (name_id ix = 0; ix < parser->name_table.size; ++ix) {
        NameData* data = &parser->name_table.data[ix];
        if (data->kind != NAME_VARIABLE && data->kind != NAME_FUNCTION) {
            continue;
        }
        var_id id = QuadList_addvar(&list, ix, data->type);
        LOG_DEBUG("'%.*s' is %llu", data->name_len, data->name, id);
        QuadList_getvar(&list, id)->array_size = data->array_size;
        data->variable = id;
    }

    for (uint64_t ix = 0; ix < parser->function_table.size; ++ix) {
        FunctionDef* def = parser->function_table.data[ix];
        label_id l = QuadList_addlabel(&list, list.quad_count);
        Quad* q = QuadList_addquad(&list, QUAD_LABEL, VAR_ID_INVALID);
        q->op1.label = l;
        quads_statements(parser, &list, def->statements, def->statement_count);
    }

    quads->quads = list.quads;
    quads->quads_count = list.quad_count;
}
