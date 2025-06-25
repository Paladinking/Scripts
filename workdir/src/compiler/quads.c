#include "quads.h"
#include "mem.h"


typedef struct QuadList {
    Quad* head;
    Quad* tail;
    uint64_t quad_count;
    uint64_t label_count;
    Arena* arena;

    VarList vars;
} QuadList;

Quad* QuadList_addquad(QuadList* quads, uint64_t type, var_id dest) {
    Quad* q = Arena_alloc_type(quads->arena, Quad);
    if (quads->head == NULL) {
        q->last_quad = NULL;
        q->next_quad = NULL;
        quads->head = q;
        quads->tail = q;
    } else {
        quads->tail->next_quad = q;
        q->last_quad = quads->tail;
        q->next_quad = NULL;
        quads->tail = q;
    }
    q->type = type;
    q->dest = dest;
    quads->quad_count += 1;
    return q;
}

void QuadList_mark_arg(QuadList* quads, var_id id) {
    quads->vars.data[id].kind = VAR_CALLARG;
}

var_id QuadList_addvar(QuadList* quads, name_id name, type_id type, Parser* parser) {
    if (quads->vars.size == quads->vars.cap) {
        uint64_t c = quads->vars.cap * 2;
        VarData* data = Mem_realloc(quads->vars.data, c * sizeof(VarData));
        if (data == NULL) {
            out_of_memory(NULL);
        }
        quads->vars.data = data;
        quads->vars.cap = c;
    }
    var_id id = quads->vars.size;
    quads->vars.data[id].name = name;
    quads->vars.data[id].type = type;
    quads->vars.data[id].alloc_type = ALLOC_NONE;
    quads->vars.data[id].reads = 0;
    quads->vars.data[id].writes = 0;
    if (parser->type_table.data[type].type_def->kind == TYPEDEF_FUNCTION) {
        quads->vars.data[id].kind = VAR_FUNCTION;
        if (name != NAME_ID_INVALID) {
            LOG_DEBUG("Function '%.*s' is %llu", parser->name_table.data[name].name_len, 
                      parser->name_table.data[name].name, id);
        }
    } else if (name == NAME_ID_INVALID) {
        quads->vars.data[id].kind = VAR_TEMP;
    } else {
        func_id func = parser->name_table.data[name].function;
        if (parser->type_table.data[type].kind == TYPE_ARRAY) {
            quads->vars.data[id].kind = VAR_ARRAY;
        } else if (func == FUNC_ID_GLOBAL) {
            quads->vars.data[id].kind = VAR_GLOBAL;
        } else {
            quads->vars.data[id].kind = VAR_LOCAL;
        }
        quads->vars.data[id].function = func;
        parser->name_table.data[name].has_var = true;
        parser->name_table.data[name].variable = id;
        LOG_DEBUG("'%.*s' is %llu", parser->name_table.data[name].name_len, 
                  parser->name_table.data[name].name, id);
    }
    AllocInfo alloc = allocation_of(parser, type);
    quads->vars.data[id].alignment = alloc.allignment;
    quads->vars.data[id].byte_size = alloc.size;

    quads->vars.size += 1;
    return id;
}

label_id QuadList_addlabel(QuadList* quads) {
    quads->label_count += 1;
    return quads->label_count - 1;
}

VarData* QuadList_getvar(QuadList* quads, var_id id) {
    return &quads->vars.data[id];
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
        dest = QuadList_addvar(quads, NAME_ID_INVALID, expr->type, parser);
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
        dest = QuadList_addvar(quads, NAME_ID_INVALID, expr->type, parser);
    }
    if (expr->binop.op == BINOP_BOOL_AND || expr->binop.op == BINOP_BOOL_OR) {
        assert(QuadList_getvar(quads, dest)->type == TYPE_ID_BOOL);
        if (label != LABEL_ID_INVALID) {
            Quad* qw = QuadList_addquad(quads, QUAD_LABEL, VAR_ID_INVALID);
            qw->op1.label = label;
        }
        return dest;
    }
    assert(label == LABEL_ID_INVALID);
    var_id lhs = expr->binop.lhs->var;
    var_id rhs = expr->binop.rhs->var;
    assert(QuadList_getvar(quads, lhs)->type == QuadList_getvar(quads, rhs)->type);
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
        dest = QuadList_addvar(quads, NAME_ID_INVALID, expr->type, parser);
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
        dest = QuadList_addvar(quads, NAME_ID_INVALID, expr->type, parser);
    }
    var_id ix = expr->array_index.index->var;
    type_id ix_type = QuadList_getvar(quads, ix)->type;
    assert(ix_type == TYPE_ID_UINT64 || ix_type == TYPE_ID_INT64);

    uint64_t datatype = quad_datatype(parser, expr->type);
    datatype |= quad_scale(parser, expr->type);
    Quad* q = QuadList_addquad(quads, QUAD_GET_ADDR | datatype, dest);
    q->op1.var = expr->array_index.array->var;
    q->op2 = expr->array_index.index->var;

    return dest;
}

var_id quads_call(Parser* parser, Expression* expr, QuadList* quads,
                  var_id dest) {

    if (dest == VAR_ID_INVALID) {
        dest = QuadList_addvar(quads, NAME_ID_INVALID, expr->type, parser);
    }

    for (uint64_t ix = 0; ix < expr->call.arg_count; ++ix) {
        var_id v = expr->call.args[ix]->var;
        type_id t = QuadList_getvar(quads, v)->type;
        uint64_t quad = quad_datatype(parser, t);
        Quad* q = QuadList_addquad(quads, QUAD_PUT_ARG | quad, VAR_ID_INVALID);
        q->op1.uint64 = ix;
        q->op2 = v;
    }

    var_id f = expr->call.function->var;
    Quad* q = QuadList_addquad(quads, QUAD_CALL, VAR_ID_INVALID);
    q->op1.var = f;

    uint64_t quad = quad_datatype(parser, expr->type);
    QuadList_addquad(quads, QUAD_GET_RET | quad, dest);

    return dest;
}




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
        RESERVE(stack, stack_size + 1, stack_cap);
        Expression* subexpr = get_subexpr(e, 0);
        if (subexpr != NULL) {
            stack[stack_size] = (struct Node){e, 1, jmp_label, jmp_quad, 
                                              needs_var, dest, 
                                              LABEL_ID_INVALID};
            ++stack_size;
            if (e->kind == EXPRESSION_BINOP && (
             e->binop.op == BINOP_BOOL_AND || e->binop.op == BINOP_BOOL_OR)) {
                if (needs_var && dest == VAR_ID_INVALID) {
                    dest = QuadList_addvar(quads, NAME_ID_INVALID, e->type, parser);
                    stack[stack_size - 1].dest = dest;
                }
                if (jmp_quad == QUAD_JMP_FALSE) {
                    assert(!needs_var);
                    if (e->binop.op == BINOP_BOOL_OR) {
                        jmp_quad = QUAD_JMP_TRUE;
                        jmp_label = QuadList_addlabel(quads);
                        stack[stack_size - 1].lbl = jmp_label;
                    }
                } else if (jmp_quad == QUAD_JMP_TRUE) {
                    assert(!needs_var);
                    if (e->binop.op == BINOP_BOOL_AND) {
                        jmp_quad = QUAD_JMP_FALSE;
                        jmp_label = QuadList_addlabel(quads);
                        stack[stack_size - 1].lbl = jmp_label;
                    }
                } else {
                    jmp_label = QuadList_addlabel(quads);
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
                dest = QuadList_addvar(quads, NAME_ID_INVALID, e->type, parser);
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
            name_id name = e->variable.ix;
            type_id type = parser->name_table.data[name].type;
            var_id id;
            if (!parser->name_table.data[name].has_var) {
                id =  QuadList_addvar(quads, name, type, parser);
            } else {
                id = parser->name_table.data[name].variable;
            }
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
            Quad* q = QuadList_addquad(quads, jmp_quad | QUAD_BOOL | QUAD_8_BIT, 
                                       VAR_ID_INVALID);
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
                Quad* q = QuadList_addquad(quads, jmp_quad | QUAD_BOOL | QUAD_8_BIT, 
                                           VAR_ID_INVALID);
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
                label_id label = QuadList_addlabel(quads);
                Quad* q = QuadList_addquad(quads, QUAD_LABEL, VAR_ID_INVALID);
                q->op1.label = label;
                label_id l2 = QuadList_addlabel(quads);
                quads_expression(parser, s->while_.condition, quads, l2, 
                                 QUAD_JMP_FALSE, VAR_ID_INVALID, false);
                ++stack_size;
                stack[stack_size - 1].ix += 1;
                stack[stack_size - 1].l1 = label;
                stack[stack_size - 1].l2 = l2;
                uint64_t c = s->while_.statement_count;
                RESERVE(stack, stack_size + c, stack_cap);
                for (uint64_t ix = 0; ix < c; ++ix) {
                    stack[stack_size + ix] = (struct Node) 
                        {s->while_.statements[c - ix - 1], 0,
                        LABEL_ID_INVALID, LABEL_ID_INVALID};
                }
                stack_size += s->while_.statement_count;
            } else {
                Quad* q = QuadList_addquad(quads, QUAD_JMP, VAR_ID_INVALID);
                q->op1.label = l1;
                q = QuadList_addquad(quads, QUAD_LABEL, VAR_ID_INVALID);
                q->op1.label = l2;
            }
            continue;
        } else if (s->type == STATEMENT_IF) {
            if (ix == 0) {
                assert(s->if_.condition != NULL);
                label_id l1 = QuadList_addlabel(quads);
                label_id l2 = l1;
                if (s->if_.else_branch != NULL) {
                    l2 = QuadList_addlabel(quads);
                }
                quads_expression(parser, s->if_.condition, quads, l2,
                                 QUAD_JMP_FALSE, VAR_ID_INVALID, false);
                ++stack_size;
                stack[stack_size - 1].ix += 1;
                stack[stack_size - 1].l1 = l1;
                stack[stack_size - 1].l2 = l2;
                uint64_t c = s->if_.statement_count;
                RESERVE(stack, stack_size + c, stack_cap);
                for (uint64_t ix = 0; ix < c; ++ix) {
                    stack[stack_size + ix] = (struct Node) 
                        {s->if_.statements[c - ix - 1], 0,
                        LABEL_ID_INVALID, LABEL_ID_INVALID};
                }
                stack_size += c;
            } else {
                if (s->if_.else_branch == NULL) {
                    Quad* q = QuadList_addquad(quads, QUAD_LABEL, 
                            VAR_ID_INVALID);
                    q->op1.label = l1;
                    continue;
                }
                Quad* q = QuadList_addquad(quads, QUAD_JMP, VAR_ID_INVALID);
                q->op1.label = l1;
                q = QuadList_addquad(quads, QUAD_LABEL, VAR_ID_INVALID);
                q->op1.label = l2;
                s = s->if_.else_branch;
                l2 = l1;
                if (s->if_.else_branch != NULL) {
                    l2 = QuadList_addlabel(quads);
                }
                if (s->if_.condition != NULL) {
                    quads_expression(parser, s->if_.condition, quads, l2,
                                     QUAD_JMP_FALSE, VAR_ID_INVALID, false);
                }
                ++stack_size;
                stack[stack_size - 1].s = s;
                stack[stack_size - 1].l2 = l2;
                uint64_t c = s->if_.statement_count;
                RESERVE(stack, stack_size + c, stack_cap);
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
                if (!parser->name_table.data[lhs->variable.ix].has_var) {
                    QuadList_addvar(quads, lhs->variable.ix, lhs->type, parser);
                }
                var_id v = parser->name_table.data[lhs->variable.ix].variable;
                lhs->var = v;
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
                                           arr_type, parser);
                rhs->var = a;
                uint64_t datatype = QUAD_PTR | QUAD_PTR_SIZE; 
                datatype |= quad_scale(parser, rhs_type);
                Quad* q = QuadList_addquad(quads, QUAD_CALC_ADDR | datatype, a);
                q->op1.var = arr;
                q->op2 = i;
                datatype = quad_datatype(parser, rhs_type);
                q = QuadList_addquad(quads, QUAD_SET_ADDR | datatype,
                                     VAR_ID_INVALID);
                q->op1.var = a;
                q->op2 = v;
            } else {
                LineInfo l = {0, 0, false};
                fatal_error(parser, PARSE_ERROR_INTERNAL, l);
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


void Quad_update_live(const Quad* q, VarSet* live) {
    enum QuadType type = q->type & QUAD_TYPE_MASK;
    switch (type) {
    case QUAD_DIV:
    case QUAD_MUL:
    case QUAD_MOD:
    case QUAD_SUB:
    case QUAD_ADD:
    case QUAD_RSHIFT:
    case QUAD_LSHIFT:
    case QUAD_CMP_EQ:
    case QUAD_CMP_NEQ:
    case QUAD_CMP_G:
    case QUAD_CMP_L:
    case QUAD_CMP_GE:
    case QUAD_CMP_LE:
    case QUAD_BOOL_AND:
    case QUAD_BOOL_OR:
    case QUAD_BIT_AND:
    case QUAD_BIT_OR:
    case QUAD_BIT_XOR:
    case QUAD_GET_ADDR:
    case QUAD_CALC_ADDR:
        VarSet_clear(live, q->dest);
        VarSet_set(live, q->op1.var);
        VarSet_set(live, q->op2);
        break;
    case QUAD_JMP:
    case QUAD_LABEL:
        break;
    case QUAD_JMP_FALSE:
    case QUAD_JMP_TRUE:
    case QUAD_PUT_ARG:
        VarSet_set(live, q->op2);
        break;
    case QUAD_RETURN:
        VarSet_set(live, q->op1.var);
        break;
    case QUAD_CALL:
        break;
    case QUAD_GET_RET:
    case QUAD_GET_ARG:
        VarSet_clear(live, q->dest);
        break;
    case QUAD_SET_ADDR:
        VarSet_set(live, q->op1.var);
        VarSet_set(live, q->op2);
        break;
    case QUAD_CREATE:
        VarSet_clear(live, q->dest);
        break;
    case QUAD_NEGATE:
    case QUAD_BOOL_NOT:
    case QUAD_BIT_NOT:
    case QUAD_CAST_TO_FLOAT64:
    case QUAD_CAST_TO_INT64:
    case QUAD_CAST_TO_UINT64:
    case QUAD_CAST_TO_BOOL:
    case QUAD_MOVE:
        VarSet_clear(live, q->dest);
        VarSet_set(live, q->op1.var);
        break;
    }
}

void Quad_add_usages(const Quad* q, VarSet* use, VarSet* define, VarList* vars) {
    enum QuadType type = q->type & QUAD_TYPE_MASK;
    switch (type) {
    case QUAD_DIV:
    case QUAD_MUL:
    case QUAD_MOD:
    case QUAD_SUB:
    case QUAD_ADD:
    case QUAD_RSHIFT:
    case QUAD_LSHIFT:
    case QUAD_CMP_EQ:
    case QUAD_CMP_NEQ:
    case QUAD_CMP_G:
    case QUAD_CMP_L:
    case QUAD_CMP_GE:
    case QUAD_CMP_LE:
    case QUAD_BOOL_AND:
    case QUAD_BOOL_OR:
    case QUAD_BIT_AND:
    case QUAD_BIT_OR:
    case QUAD_BIT_XOR:
    case QUAD_GET_ADDR:
    case QUAD_CALC_ADDR:
        ++vars->data[q->op1.var].reads;
        ++vars->data[q->op2].reads;
        ++vars->data[q->dest].writes;
        if (!VarSet_contains(define, q->op1.var)) {
            VarSet_set(use, q->op1.var);
        }
        if (!VarSet_contains(define, q->op2)) {
            VarSet_set(use, q->op2);
        }
        VarSet_set(define, q->dest);
        break;
    case QUAD_JMP:
    case QUAD_LABEL:
        break;
    case QUAD_JMP_FALSE:
    case QUAD_JMP_TRUE:
    case QUAD_PUT_ARG:
        ++vars->data[q->op2].reads;
        if (!VarSet_contains(define, q->op2)) {
            VarSet_set(use, q->op2);
        }
        break;
    case QUAD_RETURN:
        ++vars->data[q->op1.var].reads;
        if (!VarSet_contains(define, q->op1.var)) {
            VarSet_set(use, q->op1.var);
        }
        break;
    case QUAD_CALL:
        break;
    case QUAD_GET_RET:
    case QUAD_GET_ARG:
        ++vars->data[q->dest].writes;
        VarSet_set(define, q->dest);
        break;
    case QUAD_SET_ADDR:
        ++vars->data[q->op1.var].reads;
        ++vars->data[q->op2].reads;
        if (!VarSet_contains(define, q->op1.var)) {
            VarSet_set(use, q->op1.var);
        }
        if (!VarSet_contains(define, q->op2)) {
            VarSet_set(use, q->op2);
        }
        break;
    case QUAD_CREATE:
        ++vars->data[q->dest].writes;
        VarSet_set(define, q->dest);
        break;
    case QUAD_NEGATE:
    case QUAD_BOOL_NOT:
    case QUAD_BIT_NOT:
    case QUAD_CAST_TO_FLOAT64:
    case QUAD_CAST_TO_INT64:
    case QUAD_CAST_TO_UINT64:
    case QUAD_CAST_TO_BOOL:
    case QUAD_MOVE:
        ++vars->data[q->op1.var].reads;
        ++vars->data[q->dest].writes;
        if (!VarSet_contains(define, q->op1.var)) {
            VarSet_set(use, q->op1.var);
        }
        VarSet_set(define, q->dest);
        break;
    }
}

void Quad_GenerateQuads(Parser* parser, Quads* quads, Arena* arena) {
    VarList v = {0, 8, Mem_alloc(8 * sizeof(VarData))};

    QuadList list = {NULL, NULL, 0, 0, arena, v};
    if (v.data == NULL) {
        out_of_memory(parser);
    }
    assert(parser->scope_count);


    for (func_id ix = 0; ix < parser->function_table.size; ++ix) {
        FunctionDef* def = parser->function_table.data[ix];
        type_id t = parser->name_table.data[def->name].type;
        QuadList_addvar(&list, def->name, t, parser);
    }
    // TODO: add other global variables

    uint64_t global_count = list.vars.size;

    for (func_id ix = 0; ix < parser->function_table.size; ++ix) {
        FunctionDef* def = parser->function_table.data[ix];

        label_id l = QuadList_addlabel(&list);
        Quad* q = QuadList_addquad(&list, QUAD_LABEL, VAR_ID_INVALID);
        def->quad_start = q;
        q->op1.label = l;

        for (uint64_t i = 0; i < def->arg_count; ++i) {
            // Add all arguments
            type_id var_t = parser->name_table.data[def->args[i].name].type;
            assert(!parser->name_table.data[def->args[i].name].has_var);
            var_id var = QuadList_addvar(&list, def->args[i].name, var_t, parser);
            QuadList_mark_arg(&list, var);

            uint64_t datatype = quad_datatype(parser, var_t);
            Quad* q = QuadList_addquad(&list, QUAD_GET_ARG | datatype, var);
            q->op1.uint64 = i;
        }
        var_id start = list.vars.size;
        quads_statements(parser, &list, def->statements, def->statement_count);
        def->quad_end = list.tail;

        // Mark all created temporary variables as a part of this function
        for (var_id v = start; v < list.vars.size; ++v) {
            if (list.vars.data[v].kind == VAR_TEMP) {
                list.vars.data[v].function = ix;
            }
        }

        // Move all variables to function, duplicate global vars
        def->vars = list.vars;
        list.vars.data = Mem_alloc(list.vars.cap * sizeof(VarData));
        if (list.vars.data == NULL) {
            out_of_memory(parser);
        }
        list.vars.size = global_count;
        memcpy(list.vars.data, def->vars.data, global_count * sizeof(VarData));
    }

    quads->head = list.head;
    quads->tail = list.tail;
    quads->quads_count = list.quad_count;
    quads->label_count = list.label_count;
    quads->globals = list.vars;
}
