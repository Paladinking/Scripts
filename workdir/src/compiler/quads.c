#include "quads.h"
#include "ast.h"
#include "mem.h"
#include "code_generation.h"

const static enum LogCatagory LOG_CATAGORY = LOG_CATAGORY_QUADS_GENERATION;

enum VarDatatype quad_datatype(Parser* parser, type_id type) {
    if (parser->type_table.data[type].kind == TYPE_ARRAY) {
        return VARTYPE_ARRAY;
    } else if (parser->type_table.data[type].kind == TYPE_PTR) {
        return VARTYPE_UINT;
    }
    uint64_t mask;
    switch (type) {
    case TYPE_ID_UINT64:
        return VARTYPE_UINT;
    case TYPE_ID_UINT32:
        return VARTYPE_UINT;
    case TYPE_ID_UINT16:
        return VARTYPE_UINT;
    case TYPE_ID_UINT8:
        return VARTYPE_UINT;
    case TYPE_ID_INT64:
        return VARTYPE_SINT;
    case TYPE_ID_INT32:
        return VARTYPE_SINT;
    case TYPE_ID_INT16:
        return VARTYPE_SINT;
    case TYPE_ID_INT8:
        return VARTYPE_SINT;
    case TYPE_ID_FLOAT64:
        return VARTYPE_FLOAT;
    case TYPE_ID_FLOAT32:
        return VARTYPE_FLOAT;
    case TYPE_ID_BOOL:
        return VARTYPE_BOOL;
    case TYPE_ID_NULL:
        return VARTYPE_UINT;
    case TYPE_ID_NONE:
        return VARTYPE_UINT;
    }

    if (parser->type_table.data[type].type_def->kind == TYPEDEF_FUNCTION) {
        return VARTYPE_FUNCTION;
    }
    if (parser->type_table.data[type].type_def->kind == TYPEDEF_STRUCT) {
        return VARTYPE_STRUCT;
    }

    assert(false && "Unkown type definition");
}

enum QuadScale quad_scale(Parser* parser, type_id type) {
    AllocInfo i = type_allocation(&parser->type_table, type);
    if (i.size == 8) {
        return QUADSCALE_8;
    } else if (i.size == 4) {
        return QUADSCALE_4;
    } else if (i.size == 2) {
        return QUADSCALE_2;
    } else {
        assert(i.size == 1);
        return QUADSCALE_1;
    }
}


typedef struct QuadList {
    Quad* head;
    Quad* tail;
    uint64_t quad_count;
    uint64_t label_count;
    Arena* arena;

    VarList vars;
} QuadList;

Quad* QuadList_addquad(QuadList* quads, enum QuadType type, var_id dest) {
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
    q->scale = QUADSCALE_NONE;
    quads->quad_count += 1;
    return q;
}

void QuadList_mark_arg(QuadList* quads, var_id id) {
    quads->vars.data[id].kind = VAR_CALLARG;
}

var_id QuadList_add_extern_func(QuadList* quads, name_id name, type_id type, Parser* parser) {
    if (quads->vars.size == quads->vars.cap) {
        uint64_t c = quads->vars.cap * 2;
        VarData* data = Mem_realloc(quads->vars.data, c * sizeof(VarData));
        if (data == NULL) {
            out_of_memory(NULL);
        }
        quads->vars.data = data;
        quads->vars.cap = c;
    }
    // Function ptr
    type = type_ptr_of(&parser->type_table, type);

    var_id id = quads->vars.size;
    quads->vars.data[id].name = name;
    quads->vars.data[id].datatype = VARTYPE_UINT;
    quads->vars.data[id].alloc_type = ALLOC_NONE;
    quads->vars.data[id].reads = 0;
    quads->vars.data[id].writes = 0;
    quads->vars.data[id].kind = VAR_GLOBAL;

    quads->vars.size += 1;

    AllocInfo i = type_allocation(&parser->type_table, type);
    quads->vars.data[id].byte_size = i.size;
    quads->vars.data[id].alignment = i.alignment;

    parser->name_table.data[name].has_var = true;
    parser->name_table.data[name].variable = id;

    return id;
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
    quads->vars.data[id].datatype = quad_datatype(parser, type);
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
        parser->name_table.data[name].has_var = true;
        parser->name_table.data[name].variable = id;
        LOG_DEBUG("'%.*s' is %llu", parser->name_table.data[name].name_len, 
                  parser->name_table.data[name].name, id);
    }
    AllocInfo alloc = type_allocation(&parser->type_table, type);
    quads->vars.data[id].alignment = alloc.alignment;
    quads->vars.data[id].byte_size = alloc.size;

    quads->vars.size += 1;
    return id;
}

void QuadList_add_string(QuadList* quads, Parser* parser, StringLiteral* literal) {
    type_id type = type_array_of(&parser->type_table, TYPE_ID_UINT8, literal->len + 1);
    var_id var = QuadList_addvar(quads, NAME_ID_INVALID, type, parser);
    quads->vars.data[var].kind = VAR_ARRAY_GLOBAL;
    literal->var = var;
}

label_id QuadList_addlabel(QuadList* quads) {
    quads->label_count += 1;
    return quads->label_count - 1;
}

VarData* QuadList_getvar(QuadList* quads, var_id id) {
    return &quads->vars.data[id];
}

var_id quads_unop(Parser* parser, Expression* expr, QuadList* quads,
                  var_id dest) {
    var_id v = expr->unop.expr->var;

    if (dest == VAR_ID_INVALID) {
        if (expr->unop.op == UNOP_POSITIVE || expr->unop.op == UNOP_PAREN) {
            // Positive and paren is a no-op if no move is needed
            return v;
        }
        dest = QuadList_addvar(quads, NAME_ID_INVALID, expr->type, parser);
    }

    enum QuadType quad;
    switch (expr->unop.op) {
    case UNOP_ADDROF:
    case UNOP_ADDROF2:
        quad = QUAD_ADDROF;
        break;
    case UNOP_DEREF:
        quad = QUAD_DEREF;
        break;
    case UNOP_BITNOT:
        quad = QUAD_BIT_NOT;
        break;
    case UNOP_BOOLNOT:
        quad = QUAD_BOOL_NOT;
        break;
    case UNOP_NEGATIVE:
        if (expr->type == TYPE_ID_FLOAT32 || expr->type == TYPE_ID_FLOAT64) {
            quad = QUAD_FNEGATE;
        } else {
            quad = QUAD_NEGATE;
        }
        break;
    case UNOP_PAREN:
    case UNOP_POSITIVE:
        quad = QUAD_MOVE;
        break;
    default:
        fatal_error(parser, PARSE_ERROR_INTERNAL, expr->line);
        break;
    }
    Quad* q = QuadList_addquad(quads, quad, dest);
    q->op1.var = v;
    return dest;
}

bool get_power_of_two(uint64_t size, uint64_t* power) {
    if (size == 0 || size >= 0x7fffffffffffffff) {
        return false;
    }
    *power = 1;
    while (size < *power) {
        *power = (*power) * 2;
    }
    return *power == size;
}

var_id quads_binop(Parser* parser, Expression* expr, QuadList* quads,
                   var_id dest, label_id label) {
    if (dest == VAR_ID_INVALID) {
        dest = QuadList_addvar(quads, NAME_ID_INVALID, expr->type, parser);
    }
    if (expr->binop.op == BINOP_BOOL_AND || expr->binop.op == BINOP_BOOL_OR) {
        assert(QuadList_getvar(quads, dest)->datatype == VARTYPE_BOOL);
        if (label != LABEL_ID_INVALID) {
            Quad* qw = QuadList_addquad(quads, QUAD_LABEL, VAR_ID_INVALID);
            qw->op1.label = label;
        }
        return dest;
    }
    assert(label == LABEL_ID_INVALID);
    var_id lhs = expr->binop.lhs->var;
    var_id rhs = expr->binop.rhs->var;
    type_id ltype = expr->binop.lhs->type;
    type_id rtype = expr->binop.rhs->type;

    assert(parser->type_table.data[rtype].kind != TYPE_PTR);

    enum QuadType quad;
    enum QuadScale scale = QUADSCALE_NONE;

    switch (expr->binop.op) {
    case BINOP_DIV:
        if (ltype == TYPE_ID_FLOAT32 || ltype == TYPE_ID_FLOAT64) {
            quad = QUAD_FDIV;
        } else {
            quad = QUAD_DIV;
        }
        break;
    case BINOP_MUL:
        if (ltype == TYPE_ID_FLOAT32 || ltype == TYPE_ID_FLOAT64) {
            quad = QUAD_FMUL;
        } else {
            quad = QUAD_MUL;
        }
        break;
    case BINOP_MOD:
        quad = QUAD_MOD;
        break;
    case BINOP_SUB:
        if (parser->type_table.data[ltype].kind == TYPE_PTR) {
            type_id parent = parser->type_table.data[ltype].parent;
            uint64_t size = type_allocation(&parser->type_table, parent).size;
            if (parser->type_table.data[rtype].kind == TYPE_PTR && size != 1) {
                // uint64 = ptr - ptr;
                var_id tmp = QuadList_addvar(quads, NAME_ID_INVALID,
                                             expr->type, parser);
                Quad* q = QuadList_addquad(quads, QUAD_SUB, tmp);
                q->op1.var = lhs;
                q->op2 = rhs;
                uint64_t pow;
                if (get_power_of_two(size, &pow)) {
                    var_id constant = QuadList_addvar(quads, NAME_ID_INVALID,
                                                      TYPE_ID_UINT8, parser);
                    q = QuadList_addquad(quads, QUAD_CREATE, constant);
                    q->op1.uint64 = pow;

                    q = QuadList_addquad(quads, QUAD_RSHIFT, dest);
                    q->op1.var = tmp;
                    q->op2 = constant;
                } else {
                    assert(expr->type == TYPE_ID_UINT64);
                    var_id constant = QuadList_addvar(quads, NAME_ID_INVALID,
                                                      expr->type, parser);
                    q = QuadList_addquad(quads, QUAD_CREATE, constant);
                    q->op1.uint64 = size;

                    q = QuadList_addquad(quads, QUAD_DIV, dest);
                    q->op1.var = tmp;
                    q->op2 = constant;
                }
                return dest;
            } else if (size == 2 || size == 4 || size == 8) {
                // ptr = ptr - n, n = 2, 4, 8
                var_id tmp = QuadList_addvar(quads, NAME_ID_INVALID, rtype, parser);
                Quad* q = QuadList_addquad(quads, QUAD_NEGATE, tmp);
                q->op1.var = rhs;
                rhs = tmp;
                scale = QUADSCALE_1 * size;
                quad = QUAD_ADD;
            } else if (size != 1) {
                var_id tmp = QuadList_addvar(quads, NAME_ID_INVALID,
                                             rtype, parser);
                uint64_t pow;
                if (get_power_of_two(size, &pow)) {
                    var_id constant = QuadList_addvar(quads, NAME_ID_INVALID,
                                                      TYPE_ID_UINT8, parser);
                    Quad* q = QuadList_addquad(quads, QUAD_CREATE, constant);
                    q->op1.uint64 = pow;

                    q = QuadList_addquad(quads, QUAD_LSHIFT, tmp);
                    q->op1.var = rhs;
                    q->op2 = constant;
                } else {
                    assert(rtype == TYPE_ID_UINT64);
                    var_id constant = QuadList_addvar(quads, NAME_ID_INVALID,
                                                      rtype, parser);
                    Quad* q = QuadList_addquad(quads, QUAD_CREATE, constant);
                    q->op1.uint64 = size;

                    q = QuadList_addquad(quads, QUAD_MUL, tmp);
                    q->op1.var = rhs;
                    q->op2 = constant;
                }
                rhs = tmp;
                quad = QUAD_SUB;
            } else {
                quad = QUAD_SUB;
            }
        } else if (ltype == TYPE_ID_FLOAT32 || ltype == TYPE_ID_FLOAT64) {
            quad = QUAD_FSUB;
        } else {
            quad = QUAD_SUB;
        }
        break;
    case BINOP_ADD:
        if (parser->type_table.data[ltype].kind == TYPE_PTR) {
            type_id parent = parser->type_table.data[ltype].parent;
            uint64_t size = type_allocation(&parser->type_table, parent).size;
            if (size == 2 || size == 4 || size == 8) {
                scale = (QUADSCALE_1 * size);
            } else if (size != 1) {
                var_id tmp = QuadList_addvar(quads, NAME_ID_INVALID,
                                             rtype, parser);
                uint64_t pow;
                if (get_power_of_two(size, &pow)) {
                    var_id constant = QuadList_addvar(quads, NAME_ID_INVALID, 
                                                      TYPE_ID_UINT8, parser);
                    Quad* q = QuadList_addquad(quads, QUAD_CREATE, constant);
                    q->op1.uint64 = pow;

                    q = QuadList_addquad(quads, QUAD_LSHIFT, tmp);
                    q->op1.var = rhs;
                    q->op2 = constant;
                } else {
                    assert(rtype == TYPE_ID_UINT64);
                    var_id constant = QuadList_addvar(quads, NAME_ID_INVALID, 
                                                      rtype, parser);
                    Quad* q = QuadList_addquad(quads, QUAD_CREATE, constant);
                    q->op1.uint64 = size;

                    q = QuadList_addquad(quads, QUAD_MUL, tmp);
                    q->op1.var = rhs;
                    q->op2 = constant;
                }
                rhs = tmp;
            }
            quad = QUAD_ADD;
        } else if (ltype == TYPE_ID_FLOAT32 || ltype == TYPE_ID_FLOAT64) {
            quad = QUAD_FADD;
        } else {
            quad = QUAD_ADD;
        }
        break;
    case BINOP_BIT_LSHIFT:
        quad = QUAD_LSHIFT;
        break;
    case BINOP_BIT_RSHIFT:
        quad = QUAD_RSHIFT;
        break;
    case BINOP_CMP_LE:
        quad = QUAD_CMP_LE;
        break;
    case BINOP_CMP_GE:
        quad = QUAD_CMP_GE;
        break;
    case BINOP_CMP_G:
        quad = QUAD_CMP_G;
        break;
    case BINOP_CMP_L:
        quad = QUAD_CMP_L;
        break;
    case BINOP_CMP_EQ:
        quad = QUAD_CMP_EQ;
        break;
    case BINOP_CMP_NEQ:
        quad = QUAD_CMP_NEQ;
        break;
    case BINOP_BIT_AND:
        quad = QUAD_BIT_AND;
        break;
    case BINOP_BIT_XOR:
        quad = QUAD_BIT_XOR;
        break;
    case BINOP_BIT_OR:
        quad = QUAD_BIT_OR;
        break;
    default:
        fatal_error(parser, PARSE_ERROR_INTERNAL, expr->line);
    }
    Quad* q = QuadList_addquad(quads, quad, dest);
    assert(scale == QUADSCALE_NONE || quad == QUAD_ADD);
    q->scale = scale;
    q->op1.var = lhs;
    q->op2 = rhs;
    return dest;
}

var_id quads_cast(Parser* parser, Expression* expr, QuadList* quads,
                  var_id dest) {
    type_id src_type = expr->cast.e->type;
    if (dest == VAR_ID_INVALID) {
        if (parser->type_table.data[expr->type].kind == TYPE_PTR &&
            src_type == TYPE_ID_NULL) {
            // null to ptr: no-op
            return expr->cast.e->var;
        }
        if (src_type == expr->type) {
            // cast to same type: no-op
            return expr->cast.e->var;
        }
        dest = QuadList_addvar(quads, NAME_ID_INVALID, expr->type, parser);
    }
    Quad* q;
    if (src_type == expr->type) {
        q = QuadList_addquad(quads, QUAD_MOVE, dest);
    } else if (expr->type == TYPE_ID_BOOL) {
        q = QuadList_addquad(quads, QUAD_CAST_TO_BOOL, dest);
    } else if (expr->type < TYPE_ID_NUMBER_COUNT) {
        static const enum QuadType map[TYPE_ID_NUMBER_COUNT] = {
            QUAD_CAST_TO_UINT64, QUAD_CAST_TO_UINT32,
            QUAD_CAST_TO_UINT16, QUAD_CAST_TO_UINT8,
            QUAD_CAST_TO_INT64, QUAD_CAST_TO_INT32,
            QUAD_CAST_TO_INT16, QUAD_CAST_TO_INT8,
            QUAD_CAST_TO_FLOAT64, QUAD_CAST_TO_FLOAT32
        };
        q = QuadList_addquad(quads, map[expr->type], dest);
    } else if (parser->type_table.data[expr->type].kind == TYPE_PTR) { 
        if (src_type == TYPE_ID_NULL) {
            q = QuadList_addquad(quads, QUAD_MOVE, dest);
        } else {
            q = QuadList_addquad(quads, QUAD_ARRAY_TO_PTR, dest);
        }
    } else {
        fatal_error(parser, PARSE_ERROR_INTERNAL, expr->line);
        return dest;
    }
    q->op1.var = expr->cast.e->var;
    assert(q->op1.var != VAR_ID_INVALID);
    return dest;
}

var_id quads_arrayindex(Parser* parser, Expression* expr, QuadList* quads,
                        var_id dest) {
    if (dest == VAR_ID_INVALID) {
        dest = QuadList_addvar(quads, NAME_ID_INVALID, expr->type, parser);
    }
    type_id ix_type = expr->array_index.index->type;
    var_id ix_var = expr->array_index.index->var;

    enum QuadType quad;
    type_id elem_type;
    if (expr->array_index.get_addr) {
        assert(parser->type_table.data[expr->type].kind == TYPE_PTR);
        elem_type = parser->type_table.data[expr->type].parent;
        quad = QUAD_CALC_ARRAY_ADDR;
    } else {
        elem_type = expr->type;
        quad = QUAD_GET_ARRAY_ADDR;
    }

    AllocInfo i = type_allocation(&parser->type_table, elem_type);
    enum QuadScale scale;
    if (i.size == 1) {
        scale = QUADSCALE_1;
    } else if (i.size == 2) {
        scale = QUADSCALE_2;
    } else if (i.size == 4) {
        scale = QUADSCALE_4;
    } else if (i.size == 8) {
        scale = QUADSCALE_8;
    } else {
        scale = QUADSCALE_1;
        uint64_t power;

        var_id scaled = QuadList_addvar(quads, NAME_ID_INVALID,
                                        ix_type, parser);

        if (get_power_of_two(i.size, &power)) {
            var_id constant = QuadList_addvar(quads, NAME_ID_INVALID,
                                              TYPE_ID_UINT8, parser);
            Quad* q = QuadList_addquad(quads, QUAD_CREATE, constant);
            q->op1.uint64 = power;

            q = QuadList_addquad(quads, QUAD_LSHIFT, scaled);
            q->op1.var = ix_var;
            q->op2 = constant;
        } else {
            var_id constant = QuadList_addvar(quads, NAME_ID_INVALID,
                                              ix_type, parser);
            Quad* q = QuadList_addquad(quads, QUAD_CREATE, constant);
            if (ix_type == TYPE_ID_UINT64) {
                q->op1.uint64 = i.size;
            } else if (ix_type == TYPE_ID_INT64) {
                q->op1.sint64 = i.size;
            } else {
                assert(false);
            }

            q = QuadList_addquad(quads, QUAD_MUL, scaled);
            q->op1.var = ix_var;
            q->op2 = constant;
        }
        ix_var = scaled;
    }

    Quad* q = QuadList_addquad(quads, quad, dest);
    q->op1.var = expr->array_index.array->var;
    q->op2 = ix_var;
    q->scale = scale;

    return dest;
}

void quads_ptr_copy(Parser* parser, type_id type, var_id dst, var_id src,
                    QuadList* quads);

var_id quads_call(Parser* parser, Expression* expr, QuadList* quads,
                  var_id dest) {
    var_id* args = Mem_alloc(expr->call.arg_count * sizeof(var_id));
    if (args == NULL) {
        out_of_memory(NULL);
    }

    for (uint64_t ix = 0; ix < expr->call.arg_count; ++ix) {
        type_id t = expr->call.args[ix].e->type;
        args[ix] = expr->call.args[ix].e->var; 

        if (!expr->call.args[ix].ptr_copy) {
            continue;
        }

        assert(parser->type_table.data[t].kind == TYPE_PTR);

        if (expr->call.args[ix].e->kind == EXPRESSION_CALL) {
            assert(expr->call.args[ix].e->call.ptr_return);
            assert(expr->call.args[ix].e->call.get_addr);
            // No need to copy return value
            continue;
        }
        type_id parent = parser->type_table.data[t].parent;
        var_id dst = QuadList_addvar(quads, NAME_ID_INVALID,
                                     parent, parser);
        var_id dst_addr = QuadList_addvar(quads, NAME_ID_INVALID,
                                          t, parser);
        Quad* q = QuadList_addquad(quads, QUAD_ADDROF, dst_addr);
        q->op1.var = dst;
        quads_ptr_copy(parser, t, dst_addr, args[ix], quads);
        args[ix] = dst_addr;
    }
    uint64_t offset = 0;

    var_id dst;
    if (expr->call.ptr_return) {
        assert(expr->call.get_addr);
        offset = 1;
        if (dest != VAR_ID_INVALID) {
            Quad* q = QuadList_addquad(quads, QUAD_PUT_ARG, VAR_ID_INVALID);
            q->op1.uint64 = 0;
            q->op2 = dest;
        } else {
            type_id parent = parser->type_table.data[expr->type].parent;
            dst = QuadList_addvar(quads, NAME_ID_INVALID, parent, parser);
            var_id dst_addr = QuadList_addvar(quads, NAME_ID_INVALID, 
                                              expr->type, parser);
            Quad* q = QuadList_addquad(quads, QUAD_ADDROF, dst_addr);
            q->op1.var = dst;

            q = QuadList_addquad(quads, QUAD_PUT_ARG, VAR_ID_INVALID);
            q->op1.uint64 = 0;
            q->op2 = dst_addr;
        }
    }

    for (uint64_t ix = 0; ix < expr->call.arg_count; ++ix) {
        Quad* q = QuadList_addquad(quads, QUAD_PUT_ARG, VAR_ID_INVALID);
        q->op1.uint64 = ix + offset;
        q->op2 = args[ix];
    }

    Mem_free(args);

    var_id f = expr->call.function->var;
    if (quads->vars.data[f].kind == VAR_FUNCTION) {
        Quad* q = QuadList_addquad(quads, QUAD_CALL, VAR_ID_INVALID);
        q->op1.var = f;
    } else {
        // Call to function ptr (or extern fn)
        Quad* q = QuadList_addquad(quads, QUAD_CALL_PTR, VAR_ID_INVALID);
        q->op1.var = f;
    }

    // On ptr_return return a new copy of the pointer
    // This means the old copy does not have to be saved on chained returns
    if (dest == VAR_ID_INVALID || expr->call.ptr_return) {
        dest = QuadList_addvar(quads, NAME_ID_INVALID, expr->type, parser);
    }
    QuadList_addquad(quads, QUAD_GET_RET, dest);

    return dest;
}

var_id quads_access_member(Parser* parser, Expression* expr, QuadList* quads,
                           var_id dest) {
    if (dest == VAR_ID_INVALID) {
        dest = QuadList_addvar(quads, NAME_ID_INVALID, expr->type, parser);
    }
    type_id s = expr->member_access.structexpr->type;
    assert(parser->type_table.data[s].type_def->kind == TYPEDEF_STRUCT);

    uint32_t offset = expr->member_access.member_offset;

    var_id addr;
    if (expr->member_access.get_addr) {
        addr = dest;
    } else {
        type_id ptr_t = type_ptr_of(&parser->type_table, expr->type);
        addr = QuadList_addvar(quads, NAME_ID_INVALID, ptr_t, parser);
    }

    if (expr->member_access.via_ptr) {
        assert(parser->type_table.data[s].kind == TYPE_PTR);
        var_id tmp = QuadList_addvar(quads, NAME_ID_INVALID, 
                                     TYPE_ID_UINT64, parser);
        Quad* q = QuadList_addquad(quads, QUAD_CREATE, tmp);
        q->op1.uint64 = offset;

        q = QuadList_addquad(quads, QUAD_ADD, addr);
        q->op1.var = expr->member_access.structexpr->var;
        q->op2 = tmp;
    }  else {
        assert(parser->type_table.data[s].kind == TYPE_NORMAL);
        Quad* q = QuadList_addquad(quads, QUAD_STRUCT_ADDR, addr);
        q->op1.uint64 = offset;
        q->op2 = expr->member_access.structexpr->var;
    }

    if (expr->member_access.get_addr) {
        return dest;
    }

    assert(quads->vars.data[dest].byte_size <= 8);

    Quad* q = QuadList_addquad(quads, QUAD_DEREF, dest);
    q->op1.var = addr;

    return dest;
}

// Generate quads for an Expression.
// <jmp_label> is a label to jump to with <jmp_quad> after the expression
// Set to LABEL_ID_INVALID and 0 to do no jump
// <dest> is the variable the result should end up in.
// <needs_var> says if the result of the expression needs to end up in a variable
// set <needs_var> to true and <dest> to VAR_ID_INVALID to create a new variable.
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
        Expression* subexpr = get_subexpression(e, 0);
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
            if (dest == VAR_ID_INVALID) {
                dest = QuadList_addvar(quads, NAME_ID_INVALID, e->type, parser);
            }
            Quad* q;
            switch(e->kind) {
            case EXPRESSION_LITERAL_BOOL:
                q = QuadList_addquad(quads, QUAD_CREATE, dest);
                q->op1.uint64 = e->literal.uint;
                break;
            case EXPRESSION_LITERAL_NULL:
                q = QuadList_addquad(quads, QUAD_CREATE, dest);
                q->op1.uint64 = 0;
                break;
            case EXPRESSION_LITERAL_INT:
                q = QuadList_addquad(quads, QUAD_CREATE, dest);
                q->op1.sint64 = e->literal.iint;
                break;
            case EXPRESSION_LITERAL_UINT:
                q = QuadList_addquad(quads, QUAD_CREATE, dest);
                q->op1.uint64 = e->literal.uint;
                break;
            case EXPRESSION_LITERAL_FLOAT:
                q = QuadList_addquad(quads, QUAD_FCREATE, dest);
                q->op1.float64 = e->literal.float64;
                break;
            case EXPRESSION_STRING:
                q = QuadList_addquad(quads, QUAD_ARRAY_TO_PTR, dest);
                q->op1.var = e->string.str.var;
                assert(q->op1.var != VAR_ID_INVALID);
                break;
            default:
                assert("Unkown expression" && false);
                break;
            }
        } else {
            name_id name = e->variable.ix;
            type_id type = parser->name_table.data[name].type;
            var_id id;
            if (!parser->name_table.data[name].has_var) {
                id = QuadList_addvar(quads, name, type, parser);
            } else {
                id = parser->name_table.data[name].variable;
            }
            if (dest == VAR_ID_INVALID) {
                dest = id;
            } else {
                Quad* q = QuadList_addquad(quads, QUAD_MOVE, dest);
                q->op1.var = id;
            }
        }
        e->var = dest;
        if (jmp_quad == QUAD_JMP_TRUE || jmp_quad == QUAD_JMP_FALSE) {
            assert(jmp_label != LABEL_ID_INVALID);
            assert(QuadList_getvar(quads, dest)->datatype == VARTYPE_BOOL);
            Quad* q = QuadList_addquad(quads, jmp_quad, VAR_ID_INVALID);

            q->op1.label = jmp_label;
            q->op2 = dest;
        }

        while (stack_size > 0) {
            Expression* top = stack[stack_size - 1].e;
            Expression* subexpr = get_subexpression(top, stack[stack_size - 1].ix);
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
            } else if (e->kind == EXPRESSION_ACCESS_MEMBER) {
                dest = quads_access_member(parser, e, quads, dest);
            } else {
                assert("Unkown expression" && false);
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

// type is ptr type, not parent
void quads_ptr_copy(Parser* parser, type_id type, var_id dst, var_id src,
                    QuadList* quads) {
    assert(parser->type_table.data[type].kind == TYPE_PTR);
    type_id parent = parser->type_table.data[type].parent;
    AllocInfo i = type_allocation(&parser->type_table, parent);

    // TODO: This can generate unaligned reads / writes
    while (1) {
        assert(i.size != 0);
        uint32_t datasize;
        type_id datatype;
        if (i.size >= 8) {
            datasize = 8;
            datatype = TYPE_ID_UINT64;
        } else if (i.size >= 4) {
            datasize = 4;
            datatype = TYPE_ID_UINT32;
        } else if (i.size >= 2) {
            datasize = 2;
            datatype = TYPE_ID_UINT16;
        } else {
            datasize = 1;
            datatype = TYPE_ID_UINT8;
        }

        var_id tmp = QuadList_addvar(quads, NAME_ID_INVALID, datatype,
                                     parser);
        Quad* q = QuadList_addquad(quads, QUAD_DEREF, tmp);
        q->op1.var = src;

        q = QuadList_addquad(quads, QUAD_SET_ADDR, VAR_ID_INVALID);
        q->op1.var = dst;
        q->op2 = tmp;
        if (i.size == datasize) {
            return;
        }
        i.size -= datasize;
        var_id inc = QuadList_addvar(quads, NAME_ID_INVALID, TYPE_ID_UINT64,
                                     parser);
        var_id new_rval = QuadList_addvar(quads, NAME_ID_INVALID, 
                                          type, parser);
        q = QuadList_addquad(quads, QUAD_CREATE, inc);
        q->op1.uint64 = datasize;

        q = QuadList_addquad(quads, QUAD_ADD, new_rval);
        q->op1.var = src;
        q->op2 = inc;
        src = new_rval;

        inc = QuadList_addvar(quads, NAME_ID_INVALID, TYPE_ID_UINT64,
                              parser);
        var_id new_lval = QuadList_addvar(quads, NAME_ID_INVALID,
                                          TYPE_ID_UINT64, parser);
        q = QuadList_addquad(quads, QUAD_CREATE, inc);
        q->op1.uint64 = datasize;

        q = QuadList_addquad(quads, QUAD_ADD, new_lval);
        q->op1.var = dst;
        q->op2 = inc;
        dst = new_lval;
    }

}

void quads_assign_ptr_copy(Parser* parser, Expression* lhs, Expression* rhs,
                           QuadList* quads) {
    if (rhs->kind == EXPRESSION_CALL) {
        assert(rhs->call.ptr_return);
        assert(rhs->call.get_addr);
        var_id lval = quads_expression(parser, lhs, quads, LABEL_ID_INVALID, 
                                       0, VAR_ID_INVALID, true);
        quads_expression(parser, rhs, quads, LABEL_ID_INVALID, 
                         0, lval, true);
        return;
    }
    var_id rval = quads_expression(parser, rhs, quads, LABEL_ID_INVALID, 
                                   0, VAR_ID_INVALID, true);
    var_id lval = quads_expression(parser, lhs, quads, LABEL_ID_INVALID, 
                                   0, VAR_ID_INVALID, true);
    quads_ptr_copy(parser, lhs->type, lval, rval, quads);
}

void quads_assign_access_member(Parser* parser, Expression* lhs,
                                Expression* rhs, QuadList* quads) {
    var_id rval = quads_expression(parser, rhs, quads, LABEL_ID_INVALID, 
                                   0, VAR_ID_INVALID, true);
    Expression* structexp = lhs->member_access.structexpr;
    var_id s_var = quads_expression(parser, structexp, quads,
                                    LABEL_ID_INVALID, 0, 
                                    VAR_ID_INVALID, true);

    type_id s = lhs->member_access.structexpr->type;
    assert(parser->type_table.data[s].type_def->kind == TYPEDEF_STRUCT);

    type_id ptr_t = type_ptr_of(&parser->type_table, rhs->type);
    var_id addr = QuadList_addvar(quads, NAME_ID_INVALID, 
                                  ptr_t, parser);
    uint32_t offset = lhs->member_access.member_offset;

    if (lhs->member_access.via_ptr) {
        assert(parser->type_table.data[s].kind == TYPE_PTR);
        var_id tmp = QuadList_addvar(quads, NAME_ID_INVALID, 
                                     TYPE_ID_UINT64, parser);
        Quad* q = QuadList_addquad(quads, QUAD_CREATE, tmp);
        q->op1.uint64 = offset;

        q = QuadList_addquad(quads, QUAD_ADD, addr);
        q->op1.var = s_var;
        q->op2 = tmp;
    } else {
        assert(parser->type_table.data[s].kind == TYPE_NORMAL);
        Quad* q = QuadList_addquad(quads, QUAD_STRUCT_ADDR, addr);
        q->op1.uint64 = offset;
        q->op2 = s_var;
    }

    Quad* q = QuadList_addquad(quads, QUAD_SET_ADDR, VAR_ID_INVALID);
    q->op1.var = addr;
    q->op2 = rval;
}

void quads_assign(Parser* parser, QuadList* quads, Statement* s) {
    Expression* lhs = s->assignment.lhs;
    Expression* rhs = s->assignment.rhs;
    if (s->assignment.ptr_copy) {
        quads_assign_ptr_copy(parser, lhs, rhs, quads);
    } else if (lhs->kind == EXPRESSION_VARIABLE) {
        if (!parser->name_table.data[lhs->variable.ix].has_var) {
            QuadList_addvar(quads, lhs->variable.ix, lhs->type, parser);
        }
        var_id v = parser->name_table.data[lhs->variable.ix].variable;
        lhs->var = v;
        quads_expression(parser, rhs, quads, LABEL_ID_INVALID, 0, v, true);
    } else if (lhs->kind == EXPRESSION_ARRAY_INDEX) {
        assert(!lhs->array_index.get_addr);
        type_id rhs_type = rhs->type;
        var_id rval = quads_expression(parser, rhs, quads,
                                       LABEL_ID_INVALID, 0,
                                       VAR_ID_INVALID, true);
        var_id ix = quads_expression(parser, lhs->array_index.index, 
                                     quads, LABEL_ID_INVALID, 
                                     0, VAR_ID_INVALID, true);
        type_id arr_type = lhs->array_index.array->type;
        var_id arr = quads_expression(parser,
                         lhs->array_index.array, quads, 
                         LABEL_ID_INVALID, 0, VAR_ID_INVALID, true);
        type_id elem_type = parser->type_table.data[arr_type].parent;
        type_id elem_ptr = type_ptr_of(&parser->type_table, elem_type);

        var_id a = QuadList_addvar(quads, NAME_ID_INVALID, 
                                   elem_ptr, parser);
        lhs->var = a;
        Quad* q = QuadList_addquad(quads, QUAD_CALC_ARRAY_ADDR, a);
        q->scale = quad_scale(parser, rhs_type);
        q->op1.var = arr;
        q->op2 = ix;
        q = QuadList_addquad(quads, QUAD_SET_ADDR, VAR_ID_INVALID);
        q->op1.var = a;
        q->op2 = rval;
    } else if (lhs->kind == EXPRESSION_UNOP && lhs->unop.op == UNOP_DEREF) {
        var_id rval = quads_expression(parser, rhs, quads, LABEL_ID_INVALID, 0,
                                       VAR_ID_INVALID, true);
        var_id ptr = quads_expression(parser, lhs->unop.expr, quads,
                                      LABEL_ID_INVALID, 0, VAR_ID_INVALID, true);
        Quad* q = QuadList_addquad(quads, QUAD_SET_ADDR, VAR_ID_INVALID);
        q->op1.var = ptr;
        q->op2 = rval;
    } else if (lhs->kind == EXPRESSION_ACCESS_MEMBER) {
        quads_assign_access_member(parser, lhs, rhs, quads);
    } else {
        assert("Illegal assigment" && false);
    }
}

void quads_return(Parser* parser, QuadList* quads, 
                  Statement* s, var_id retvar) {
    Expression* val = s->return_.return_value;
    if (!s->return_.ptr_return) {
        var_id v = quads_expression(parser, val, quads,
                LABEL_ID_INVALID, 0, VAR_ID_INVALID, true);
        Quad* q = QuadList_addquad(quads, QUAD_RETURN, VAR_ID_INVALID);
        q->op1.var = v;
        return;
    }

    // ptr_returns means we return ptr to value, actual value in <retvar>
    // For x64 this is done with structs that cannot be passed in registers.
    if (val->kind == EXPRESSION_CALL) {
        // If the call was not ptr_return, it would be wrapper with UNOP_ADDROF2
        // Also, chained return types without casts must be exactly the same
        assert(val->call.ptr_return);
        assert(val->call.get_addr);
        var_id v = quads_expression(parser, val, quads, LABEL_ID_INVALID, 0, 
                                    retvar, true);
        Quad* q = QuadList_addquad(quads, QUAD_RETURN, VAR_ID_INVALID);
        q->op1.var = v;
    } else {
        var_id v = quads_expression(parser, val, quads, LABEL_ID_INVALID,
                                    0, VAR_ID_INVALID, true);
        quads_ptr_copy(parser, val->type, retvar, v, quads);
        Quad* q = QuadList_addquad(quads, QUAD_RETURN, VAR_ID_INVALID);
        q->op1.var = retvar;
    }
}

// retvar is VAR_ID_INVALID unless pointer return is used
void quads_statements(Parser* parser, QuadList* quads, Statement** statements,
                      uint64_t count, var_id retvar) {
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
            quads_assign(parser, quads, s);
            continue;
        } else if (s->type == STATEMENT_EXPRESSION) {
            quads_expression(parser, s->expression, quads, LABEL_ID_INVALID,
                             0, VAR_ID_INVALID, false);
            continue;
        } else if (s->type == STATEMENT_RETURN) {
            quads_return(parser, quads, s, retvar);
            continue;
        }
    }
}


void Quad_update_live(const Quad* q, VarSet* live) {
    switch (q->type) {
    case QUAD_DIV:
    case QUAD_MUL:
    case QUAD_MOD:
    case QUAD_SUB:
    case QUAD_ADD:
    case QUAD_FADD:
    case QUAD_FSUB:
    case QUAD_FMUL:
    case QUAD_FDIV:
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
    case QUAD_GET_ARRAY_ADDR:
    case QUAD_CALC_ARRAY_ADDR:
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
    case QUAD_CALL_PTR:
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
    case QUAD_FCREATE:
        VarSet_clear(live, q->dest);
        break;
    case QUAD_NEGATE:
    case QUAD_FNEGATE:
    case QUAD_BOOL_NOT:
    case QUAD_BIT_NOT:
    case QUAD_CAST_TO_FLOAT64:
    case QUAD_CAST_TO_FLOAT32:
    case QUAD_CAST_TO_INT64:
    case QUAD_CAST_TO_INT32:
    case QUAD_CAST_TO_INT16:
    case QUAD_CAST_TO_INT8:
    case QUAD_CAST_TO_UINT64:
    case QUAD_CAST_TO_UINT32:
    case QUAD_CAST_TO_UINT16:
    case QUAD_CAST_TO_UINT8:
    case QUAD_CAST_TO_BOOL:
    case QUAD_ARRAY_TO_PTR:
    case QUAD_MOVE:
    case QUAD_DEREF:
    case QUAD_ADDROF:
        VarSet_clear(live, q->dest);
        VarSet_set(live, q->op1.var);
        break;
    case QUAD_STRUCT_ADDR:
        VarSet_clear(live, q->dest);
        VarSet_set(live, q->op2);
        break;
    default:
        assert(false);
    }
}

void Quad_add_usages(const Quad* q, VarSet* use, VarSet* define, VarList* vars) {
    switch (q->type) {
    case QUAD_DIV:
    case QUAD_MUL:
    case QUAD_MOD:
    case QUAD_SUB:
    case QUAD_ADD:
    case QUAD_FADD:
    case QUAD_FSUB:
    case QUAD_FDIV:
    case QUAD_FMUL:
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
    case QUAD_GET_ARRAY_ADDR:
    case QUAD_CALC_ARRAY_ADDR:
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
    case QUAD_CALL_PTR:
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
    case QUAD_FCREATE:
        ++vars->data[q->dest].writes;
        VarSet_set(define, q->dest);
        break;
    case QUAD_NEGATE:
    case QUAD_FNEGATE:
    case QUAD_BOOL_NOT:
    case QUAD_BIT_NOT:
    case QUAD_CAST_TO_FLOAT64:
    case QUAD_CAST_TO_FLOAT32:
    case QUAD_CAST_TO_INT64:
    case QUAD_CAST_TO_INT32:
    case QUAD_CAST_TO_INT16:
    case QUAD_CAST_TO_INT8:
    case QUAD_CAST_TO_UINT64:
    case QUAD_CAST_TO_UINT32:
    case QUAD_CAST_TO_UINT16:
    case QUAD_CAST_TO_UINT8:
    case QUAD_CAST_TO_BOOL:
    case QUAD_ARRAY_TO_PTR:
    case QUAD_MOVE:
    case QUAD_ADDROF:
    case QUAD_DEREF:
        ++vars->data[q->op1.var].reads;
        ++vars->data[q->dest].writes;
        if (!VarSet_contains(define, q->op1.var)) {
            VarSet_set(use, q->op1.var);
        }
        VarSet_set(define, q->dest);
        break;
    case QUAD_STRUCT_ADDR:
        ++vars->data[q->op2].reads;
        ++vars->data[q->dest].writes;
        if (!VarSet_contains(define, q->op2)) {
            VarSet_set(use, q->op2);
        }
        VarSet_set(define, q->dest);
        break;
    default:
        assert(false);
    }
}

void Quad_GenerateQuads(Parser* parser, Quads* quads, Arena* arena) {
    VarList v = {0, 8, Mem_alloc(8 * sizeof(VarData))};

    QuadList list = {NULL, NULL, 0, 0, arena, v};
    if (v.data == NULL) {
        out_of_memory(parser);
    }
    assert(parser->name_table.scope_count);

    for (func_id ix = 0; ix < parser->externs.size; ++ix) {
        FunctionDef* def = parser->externs.data[ix];
        type_id t = parser->name_table.data[def->name].type;
        QuadList_add_extern_func(&list, def->name, t, parser);
    }

    for (func_id ix = 0; ix < parser->function_table.size; ++ix) {
        FunctionDef* def = parser->function_table.data[ix];
        type_id t = parser->name_table.data[def->name].type;
        QuadList_addvar(&list, def->name, t, parser);
    }
    StringLiteral* literal = parser->first_str;
    while (literal != NULL) {
        QuadList_add_string(&list, parser, literal);
        literal = literal->next;
    }
    uint64_t global_count = list.vars.size;

    for (func_id ix = 0; ix < parser->function_table.size; ++ix) {
        FunctionDef* def = parser->function_table.data[ix];

        label_id l = QuadList_addlabel(&list);
        Quad* q = QuadList_addquad(&list, QUAD_LABEL, VAR_ID_INVALID);
        def->quad_start = q;
        q->op1.label = l;

        type_id rettype = def->return_type;
        AllocInfo info = type_allocation(&parser->type_table, rettype);
        var_id retvar = VAR_ID_INVALID;
        if (Backend_return_as_ptr(info)) {
            rettype = type_ptr_of(&parser->type_table, rettype);
            retvar = QuadList_addvar(&list, NAME_ID_INVALID, rettype, parser);
            Quad* q = QuadList_addquad(&list, QUAD_GET_ARG, retvar);
            q->op1.uint64 = 0;
        }

        for (uint64_t i = 0; i < def->arg_count; ++i) {
            // Add all arguments
            type_id var_t = parser->name_table.data[def->args[i].name].type;
            assert(!parser->name_table.data[def->args[i].name].has_var);
            var_id var = QuadList_addvar(&list, def->args[i].name, var_t, parser);
            QuadList_mark_arg(&list, var);

            Quad* q = QuadList_addquad(&list, QUAD_GET_ARG, var);
            q->op1.uint64 = (retvar == VAR_ID_INVALID) ? i : i + 1;
        }

        quads_statements(parser, &list, def->statements, def->statement_count,
                         retvar);

        label_id end_label = QuadList_addlabel(&list);

        def->quad_end = list.tail;
        def->end_label = end_label;

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
