#include "type_checker.h"
#include <mem.h>

static inline type_id require_number(Parser* parser, Expression* e) {
    if (e->type == TYPE_ID_INT64 || e->type == TYPE_ID_UINT64 ||
        e->type == TYPE_ID_FLOAT64) {
        return e->type;
    }
    add_error(parser, TYPE_ERROR_ILLEGAL_TYPE, e->line);
    return TYPE_ID_INT64;
}

static inline type_id require_interger(Parser* parser, Expression* e, bool sign, bool unsign) {
    if (sign && e->type == TYPE_ID_INT64) {
        return e->type;
    }
    if (unsign && e->type == TYPE_ID_UINT64) {
        return e->type;
    }
    add_error(parser, TYPE_ERROR_ILLEGAL_TYPE, e->line);
    if (sign) {
        return TYPE_ID_INT64;
    } else {
        return TYPE_ID_UINT64;
    }
}

static inline type_id require_bool(Parser* parser, Expression* e) {
    if (e->type == TYPE_ID_BOOL) {
        return e->type;
    }
    add_error(parser, TYPE_ERROR_ILLEGAL_TYPE, e->line);
    return TYPE_ID_BOOL;
}

type_id typecheck_cast(Parser* parser, Expression* op) {
    type_id type = op->cast.type;
    type_id t = op->cast.e->type;
    if (type == TYPE_ID_FLOAT64 || type == TYPE_ID_INT64 ||
        type == TYPE_ID_UINT64 || type == TYPE_ID_BOOL) {
        if (t == TYPE_ID_FLOAT64 || t == TYPE_ID_INT64 ||
            t == TYPE_ID_UINT64 || t == TYPE_ID_BOOL) {
            return type;
        }
    }
    add_error(parser, TYPE_ERROR_ILLEGAL_CAST, op->line);
    return type;
}

void cast_to(Parser* parser, Expression* e, type_id type) {
    Expression* new = Arena_alloc_type(&parser->arena, Expression);
    *new = *e;
    e->kind = EXPRESSION_CAST;
    e->type = type;
    e->cast.type = type;
    e->cast.e = new;
}

type_id merge_numbers(Parser* parser, type_id a, type_id b, Expression* expr_a, Expression* expr_b) {
    if (a == b) {
        return a;
    }
    if (a == TYPE_ID_FLOAT64) {
        cast_to(parser, expr_b, TYPE_ID_FLOAT64);
        return TYPE_ID_FLOAT64;
    }
    if (b == TYPE_ID_FLOAT64) {
        cast_to(parser, expr_a, TYPE_ID_FLOAT64);
        return TYPE_ID_FLOAT64;
    }
    if (a == TYPE_ID_INT64) {
        cast_to(parser, expr_b, TYPE_ID_INT64);
        return TYPE_ID_INT64;
    }
    if (b == TYPE_ID_INT64) {
        cast_to(parser, expr_a, TYPE_ID_INT64);
        return TYPE_ID_INT64;
    }
    fatal_error(parser, PARSE_ERROR_INTERNAL, expr_a->line);
    return TYPE_ID_UINT64;
}

type_id typecheck_unop(Parser* parser, Expression* op) {
    switch(op->unop.op) {
    case UNOP_PAREN:
        *op = *op->unop.expr;
        return op->type;
    case UNOP_BITNOT:
        return require_interger(parser, op->unop.expr, true, true);
    case UNOP_NEGATVIE:
        // TODO: add warning on unsigned
        return require_number(parser, op->unop.expr);
    case UNOP_POSITIVE: {
        type_id t = require_number(parser, op->unop.expr);
        *op = *op->unop.expr;
        return t;

    }
    case UNOP_BOOLNOT:
        return require_bool(parser, op->unop.expr);
    }
    fatal_error(parser, PARSE_ERROR_INTERNAL, op->line);
    return TYPE_ID_INVALID;
}

type_id typecheck_binop(Parser* parser, Expression* op) {
    type_id a, b;
    switch(op->binop.op) {
    case BINOP_SUB:
    case BINOP_ADD:
    case BINOP_MUL:
    case BINOP_DIV:
        a = require_number(parser, op->binop.lhs);
        b = require_number(parser, op->binop.rhs);
        return merge_numbers(parser, a, b, op->binop.lhs, op->binop.rhs);
    case BINOP_MOD:
    case BINOP_BIT_LSHIFT:
    case BINOP_BIT_RSHIFT:
    case BINOP_BIT_AND:
    case BINOP_BIT_XOR:
    case BINOP_BIT_OR:
        a = require_interger(parser, op->binop.lhs, true, true);
        b = require_interger(parser, op->binop.rhs, true, true);
        return merge_numbers(parser, a, b, op->binop.lhs, op->binop.rhs);
    case BINOP_CMP_LE:
    case BINOP_CMP_GE:
    case BINOP_CMP_G:
    case BINOP_CMP_L:
    case BINOP_CMP_EQ:
    case BINOP_CMP_NEQ:
        if (op->binop.lhs->type == TYPE_ID_BOOL ||
            op->binop.rhs->type == TYPE_ID_BOOL) {
            a = require_bool(parser, op->binop.lhs);
            b = require_bool(parser, op->binop.rhs);
        } else {
            a = require_number(parser, op->binop.lhs);
            b = require_number(parser, op->binop.rhs);
            merge_numbers(parser, a, b, op->binop.lhs, op->binop.rhs);
        }
        return TYPE_ID_BOOL;
    case BINOP_BOOL_AND:
    case BINOP_BOOL_OR:
        a = require_bool(parser, op->binop.lhs);
        b = require_bool(parser, op->binop.rhs);
        return TYPE_ID_BOOL;
    }
    fatal_error(parser, PARSE_ERROR_INTERNAL, op->line);
    return TYPE_ID_INVALID;
}

type_id typecheck_array_index(Parser* parser, Expression* e) {
    type_id ix = require_interger(parser, e->array_index.index, true, true);
    if (ix != TYPE_ID_UINT64) {
        cast_to(parser, e->array_index.index, TYPE_ID_UINT64);
        // No need to check validity of cast, require_interger makes sure it's valid.
    }
    type_id a = e->array_index.array->type;
    if (parser->type_table.data[a].kind != TYPE_ARRAY) {
        add_error(parser, TYPE_ERROR_ILLEGAL_TYPE, e->line);
        return a;
    }
    return parser->type_table.data[a].parent;
}

type_id typecheck_call(Parser* parser, Expression* e) {
    type_id f = e->call.function->type;
    if (parser->type_table.data[f].kind != TYPE_NORMAL) {
        add_error(parser, TYPE_ERROR_ILLEGAL_TYPE, e->line);
        return TYPE_ID_INT64;
    }
    TypeDef* def = parser->type_table.data[f].type_def;
    if (def->kind != TYPEDEF_FUNCTION) {
        add_error(parser, TYPE_ERROR_ILLEGAL_TYPE, e->line);
        return TYPE_ID_INT64;
    }
    FunctionDef* func = def->func;

    if (func->arg_count != e->call.arg_count) {
        add_error(parser, TYPE_ERROR_WRONG_ARG_COUNT, e->line);
        return func->return_type;
    }

    for (uint64_t ix = 0; ix < func->arg_count; ++ix) {
        if (func->args[ix].type != e->call.args[ix]->type) {
            cast_to(parser, e->call.args[ix], func->args[ix].type);
            typecheck_cast(parser, e->call.args[ix]);
        }
    }

    return func->return_type;
}

type_id typecheck_expression(Parser* parser, Expression* e) {

    struct Node {
        Expression* e;
        uint64_t ix;
    };

    struct Node* stack = Mem_alloc(8 * sizeof(struct Node));
    if (stack == NULL) {
        out_of_memory(parser);
    }
    uint64_t stack_size = 0;
    uint64_t stack_cap = 8;

    while (1) {
loop:
        if (stack_size == stack_cap) {
            stack_cap *= 2;
            struct Node* new_stack = Mem_realloc(stack, stack_cap * sizeof(struct Node));
            if (new_stack == NULL) {
                out_of_memory(parser);
            }
            stack = new_stack;
        }
        type_id type = TYPE_ID_INVALID;
        switch (e->kind) {
        case EXPRESSION_INVALID:
            fatal_error(parser, PARSE_ERROR_INTERNAL, e->line);
            break;
        case EXPRESSION_VARIABLE:
            type = type_of(parser, e->variable.ix);
            break;
        case EXPRESSION_LITERAL_INT:
            type = TYPE_ID_INT64;
            break;
        case EXPRESSION_LITERAL_BOOL:
            type = TYPE_ID_BOOL;
            break;
        case EXPRESSION_LITERAL_UINT:
            type = TYPE_ID_UINT64;
            break;
        case EXPRESSION_LITERAL_FLOAT:
            type = TYPE_ID_FLOAT64;
            break;
        case EXPRESSION_LITERAL_STRING:
            type = TYPE_ID_CSTRING;
            break;
        case EXPRESSION_CAST:
            stack[stack_size++] = (struct Node){e, 0};
            e = e->cast.e;
            goto loop;
        case EXPRESSION_UNOP:
            stack[stack_size++] = (struct Node){e, 0};
            e = e->unop.expr;
            goto loop;
        case EXPRESSION_BINOP:
            stack[stack_size++] = (struct Node){e, 0};
            e = e->binop.lhs;
            goto loop;
        case EXPRESSION_ARRAY_INDEX:
            stack[stack_size++] = (struct Node){e, 0};
            e = e->array_index.array;
            goto loop;
        case EXPRESSION_CALL:
            stack[stack_size++] = (struct Node){e, 0};
            e = e->call.function;
            goto loop;
        }
        e->type = type;

        while (stack_size > 0) {
            Expression* prev = stack[stack_size - 1].e;
            if (prev->kind == EXPRESSION_UNOP) {
                type = typecheck_unop(parser, prev);
            } else if (prev->kind == EXPRESSION_CAST) { 
                type = typecheck_cast(parser, prev);
            } else if (prev->kind == EXPRESSION_BINOP) {
                if (stack[stack_size - 1].ix == 0) {
                    e = prev->binop.rhs;
                    stack[stack_size - 1].ix = 1;
                    goto loop;
                }
                type = typecheck_binop(parser, prev);
            } else if (prev->kind == EXPRESSION_ARRAY_INDEX) {
                if (stack[stack_size - 1].ix == 0) {
                    e = prev->array_index.index;
                    stack[stack_size  -1].ix = 1;
                    goto loop;
                }
                type = typecheck_array_index(parser, prev);
            } else if (prev->kind == EXPRESSION_CALL) {
                if (stack[stack_size - 1].ix < prev->call.arg_count) {
                    e = prev->call.args[stack[stack_size - 1].ix];
                    stack[stack_size - 1].ix += 1;
                    goto loop;
                }
                type = typecheck_call(parser, prev);
            } else {
                fatal_error(parser, PARSE_ERROR_INTERNAL, prev->line);
            }
            prev->type = type;
            e = prev;
            --stack_size;
        }
        Mem_free(stack);
        return type;
    }
}

Statement** add_statement(Parser* parser, Statement** stack, uint64_t* stack_size, uint64_t* stack_cap, Statement* s) {
    if (*stack_size == *stack_cap) {
        *stack_cap *= 2;
        Statement** new_s = Mem_realloc(stack, *stack_cap * sizeof(Statement*));
        if (new_s == NULL) {
            out_of_memory(parser);
        }
        stack = new_s;
    }
    stack[*stack_size] = s;
    *stack_size += 1;
    return stack;
}

void typecheck_statements(Parser* parser, Statement** statements, uint64_t statement_count, type_id rettype) {
    Statement** stack = Mem_alloc((statement_count + 8) * sizeof(Statement*));
    if (stack == NULL) {
        out_of_memory(parser);
    }
    uint64_t stack_size = statement_count;
    uint64_t stack_cap = statement_count + 8;
    memcpy(stack, statements, statement_count * sizeof(Statement*));

    while (stack_size > 0) {
        Statement* top = stack[stack_size - 1];
        --stack_size;

        Statement** new_statements = NULL;
        uint64_t new_statement_count = 0;
        switch (top->type) {
        case STATEMENT_ASSIGN: {
            type_id src = typecheck_expression(parser, top->assignment.rhs);
            type_id dst = typecheck_expression(parser, top->assignment.lhs);
            if (top->assignment.lhs->kind != EXPRESSION_VARIABLE &&
                top->assignment.lhs->kind != EXPRESSION_ARRAY_INDEX) {
                add_error(parser, TYPE_ERROR_ILLEGAL_TYPE, top->assignment.rhs->line);
            } else if (src != dst) {
                cast_to(parser, top->assignment.rhs, dst);
                typecheck_cast(parser, top->assignment.rhs);
            }
            break;
        }
        case STATEMENT_EXPRESSION:
            typecheck_expression(parser, top->expression);
            break;
        case STATEMENT_RETURN:
            if (typecheck_expression(parser, top->return_.return_value) != rettype) {
                cast_to(parser, top->return_.return_value, rettype);
                typecheck_cast(parser, top->return_.return_value);
            }
            break;
        case STATEMENT_IF: {
            if (top->if_.condition != NULL) {
                type_id cond = typecheck_expression(parser, top->if_.condition);
                if (cond != TYPE_ID_BOOL) {
                    add_error(parser, TYPE_ERROR_ILLEGAL_TYPE, top->if_.condition->line);
                }
            }
            if (top->if_.else_branch != NULL) {
                stack = add_statement(parser, stack, &stack_size, &stack_cap, top->if_.else_branch);
            }
            for (uint64_t ix = 0; ix < top->if_.statement_count; ++ix) {
                stack = add_statement(parser, stack, &stack_size, &stack_cap, top->if_.statements[ix]);
            }
            break;
        }
        case STATEMENT_WHILE:
            type_id cond = typecheck_expression(parser, top->while_.condition);
            if (cond != TYPE_ID_BOOL) {
                add_error(parser, TYPE_ERROR_ILLEGAL_TYPE, top->while_.condition->line);
            }
            for (uint64_t ix = 0; ix < top->while_.statement_count; ++ix) {
                stack = add_statement(parser, stack, &stack_size, &stack_cap, top->while_.statements[ix]);
            }
            break;
        case STATEMET_INVALID:
            fatal_error(parser, PARSE_ERROR_INTERNAL, top->line);
            break;
        }
    }
    Mem_free(stack);
}


bool TypeChecker_run(Parser* parser) {
    NameTable* name_table = &parser->name_table;
    TypeTable* type_table = &parser->type_table;

    FunctionTable* funcs = &parser->function_table;

    for (uint64_t ix = 0; ix < funcs->size; ++ix) {
        type_id rettype = funcs->data[ix]->return_type;
        typecheck_statements(parser, funcs->data[ix]->statements,
                funcs->data[ix]->statement_count, rettype);
    }

    return parser->first_error == NULL;
}
