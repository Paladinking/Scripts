#include "format.h"
#include "tokenizer.h"

const static char* quadnames[] = {
    "QDIV", "QMUL", "QMOD", "QSUB", "QADD",
    "QRSHIFT", "QLSHIFT", "QNEGATE", "QCMP_EQ",
    "QCMP_NEQ", "QCMP_G", "QCMP_L", "QCMP_GE",
    "QCMP_LE", "QJMP", "QJMP_FALSE", "QJMP_TRUE",
    "QLABEL", "QBOOL_AND", "QBOOL_OR", "QBOOL_NOT",
    "QBIT_AND", "QBIT_OR", "QBIT_XOR", "QBIT_NOT",
    "QCAST_TO_FLOAT64", "QCAST_TO_FLOAT32", 
    "QCAST_TO_INT64", "QCAST_TO_INT32",
    "QCAST_TO_INT16", "QCAST_TO_INT8",
    "QCAST_TO_UINT64", "QCAST_TO_UINT32",
    "QCAST_TO_UINT16", "QCAST_TO_UINT8",
    "QCAST_TO_BOOL", "QARRAY_TO_PTR", "QPUT_ARG", 
    "QCALL", "QRETURN", "QGET_RET", "QGET_ARG",
    "QMOVE", "QGET_ARRAY_ADDR", "QCALC_ARRAY_ADDR",
    "QSET_ADDR", "QCREATE", "QADDROF", "QDEREF"
};

void fmt_quad(const Quad* quad, String* dest) {
    enum QuadType type = quad->type & QUAD_TYPE_MASK;
    if (type >= 0 && type < QUAD_COUNT) {
        String_extend(dest, quadnames[type]);
    } else {
        String_extend(dest, "QUNKOWN");
        return;
    }
    String_append(dest, ' ');
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
    case QUAD_GET_ARRAY_ADDR:
    case QUAD_CALC_ARRAY_ADDR:
        String_format_append(dest, "<%llu> <%llu> -> <%llu>", quad->op1.var, quad->op2,
                             quad->dest);
        break;
    case QUAD_JMP:
    case QUAD_LABEL:
        String_format_append(dest, "L%llu", quad->op1.label);
        break;
    case QUAD_JMP_FALSE:
    case QUAD_JMP_TRUE:
        String_format_append(dest, "L%llu <%llu>", quad->op1.label, quad->op2);
        break;
    case QUAD_PUT_ARG:
        String_format_append(dest, "<%llu>", quad->op2);
        break;
    case QUAD_RETURN:
        String_format_append(dest, "<%llu>", quad->op1.var);
        break;
    case QUAD_CALL:
        String_format_append(dest, "$%llu", quad->op1.var);
        break;
    case QUAD_GET_RET:
    case QUAD_GET_ARG:
        String_format_append(dest, "-> <%llu>", quad->dest);
        break;
    case QUAD_SET_ADDR:
        String_format_append(dest, "<%llu> <%llu>", quad->op1.var, quad->op2);
        break;
    case QUAD_CREATE:
        String_format_append(dest, "[] -> <%llu>", quad->dest);
        break;
    case QUAD_NEGATE:
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
        String_format_append(dest, "<%llu> -> <%llu>", quad->op1.var, quad->dest);
        break;
    }
    if (quad->type & QUAD_UINT) {
        String_extend(dest, " UINT");
    } else if (quad->type & QUAD_SINT) {
        String_extend(dest, " SINT");
    } else if (quad->type & QUAD_FLOAT) {
        String_extend(dest, " FLOAT");
    } else if (quad->type & QUAD_BOOL) {
        String_extend(dest, " BOOL");
    }
    if (quad->type & QUAD_64_BIT) {
        String_extend(dest, " 64");
    } else if (quad->type & QUAD_32_BIT) {
        String_extend(dest, " 32");
    } else if (quad->type & QUAD_16_BIT) {
        String_extend(dest, " 16");
    } else if (quad->type & QUAD_8_BIT) {
        String_extend(dest, " 8");
    }
    if (quad->type & QUAD_SCALE_8) {
        String_extend(dest, " SCALE 8");
    } else if (quad->type & QUAD_SCALE_4) {
        String_extend(dest, " SCALE 4");
    } else if (quad->type & QUAD_SCALE_2) {
        String_extend(dest, " SCALE 2");
    } else if (quad->type & QUAD_SCALE_1) {
        String_extend(dest, " SCALE 1");
    }
}

void fmt_quads(const Quads* quads, String* dest) {
    for (const Quad* q = quads->head; q != quads->tail->next_quad; q = q->next_quad) {
        fmt_quad(q, dest);
        String_append(dest, '\n');
    } 
}

void fmt_functiondef(const FunctionDef* def, const Parser* parser, String* dest) {
    String_extend(dest, "FunctionDef ");
    fmt_name(def->name, parser, dest);
    String_append(dest, '(');
    for (uint64_t i = 0; i < def->arg_count; ++i) {
        fmt_type(def->args[i].type, parser, dest);
        String_append(dest, ' ');
        fmt_name(def->args[i].name, parser, dest);
        if (i + 1 != def->arg_count) {
            String_append_count(dest, ", ", 2);
        }
    }
    String_extend(dest, ") -> ");
    fmt_type(def->return_type, parser, dest);
    String_append_count(dest, " {\n", 3);
    fmt_statements(def->statements, def->statement_count, parser, dest);
    String_append_count(dest, "}\n", 2);
}

void fmt_name(name_id name, const Parser* parser, String* dest) {
    if (name == NAME_ID_INVALID) {
        String_extend(dest, "<Invalid name>");
        return;
    }
    const uint8_t* b = parser->name_table.data[name].name;
    uint32_t len = parser->name_table.data[name].name_len;
    String_append_count(dest, (const char*)b, len);
    String_format_append(dest, "#%llu", name);
}

void fmt_type(type_id type, const Parser* parser, String* dest) {
    if (type == TYPE_ID_INVALID) {
        String_extend(dest, "<Invalid type>");
        return;
    }
    const TypeDef* def = parser->type_table.data[type].type_def;
    name_id name;
    if (def->kind == TYPEDEF_STRUCT) {
        String_extend(dest, "<Struct '");
        name = def->struct_.name;
    } else if (def->kind == TYPEDEF_FUNCTION) {
        String_extend(dest, "<Function type '");
        name = def->func->name;
    } else {
        name = def->builtin.name;
    }
    fmt_name(name, parser, dest);
    if (def->kind == TYPEDEF_STRUCT || def->kind == TYPEDEF_FUNCTION) {
        String_append_count(dest, "'>", 2);
    }
    while (parser->type_table.data[type].kind == TYPE_ARRAY) {
        String_append_count(dest, "[]", 2);
        type = parser->type_table.data[type].parent;
    }
    while (parser->type_table.data[type].kind == TYPE_PTR) {
        String_append(dest, '*');
        type = parser->type_table.data[type].parent;
    }
}

void fmt_errors(const Parser* parser, String* dest) {
    Error* err = parser->first_error;
    while (err != NULL) {
        if (err->kind == PARSE_ERROR_NONE) {
            continue;
        }
        switch (err->kind) {
        case PARSE_ERROR_NONE:
            break;
        case PARSE_ERROR_BAD_TYPE:
            String_extend(dest, "Error: bad type");
            break;
        case PARSE_ERROR_EOF:
            String_extend(dest, "Error: unexpecetd end of file");
            break;
        case PARSE_ERROR_OUTOFMEMORY:
            String_extend(dest, "Error: Out of memory");
            break;
        case PARSE_ERROR_INVALID_ESCAPE:
            String_extend(dest, "Error: invalid escape code");
            break;
        case PARSE_ERROR_INVALID_CHAR:
            String_extend(dest, "Error: bad character");
            break;
        case PARSE_ERROR_RESERVED_NAME:
            String_extend(dest, "Error: illegal name");
            break;
        case PARSE_ERROR_BAD_NAME:
            String_extend(dest, "Error: bad identifier");
            break;
        case PARSE_ERROR_INVALID_LITERAL:
            String_extend(dest, "Error: invalid literal");
            break;
        case PARSE_ERROR_REDEFINITION:
            String_extend(dest, "Error: redefinition");
            break;
        case PARSE_ERROR_BAD_ARRAY_SIZE:
            String_extend(dest, "Error: Bad array size");
            break;
        case PARSE_ERROR_INTERNAL:
            String_extend(dest, "Internal compiler error");
            break;
        case TYPE_ERROR_ILLEGAL_TYPE:
            String_extend(dest, "Type Error: Illegal type");
            break;
        case TYPE_ERROR_ILLEGAL_CAST:
            String_extend(dest, "Type Error: Illegal cast");
            break;
        case TYPE_ERROR_WRONG_ARG_COUNT:
            String_extend(dest, "Type Error: Wrong number of arguments");
            break;
        case ASM_ERROR_MISSING_ENCODING:
            String_extend(dest, "Assembly Error: Missing encoding");
        }
        uint64_t row, col;
        parser_row_col(parser, err->pos.start, &row, &col);
        ++row;
        ++col;
        String_format_append(dest, " at row %llu collum %llu [%s:%llu]\n",
                              row, col, err->file, err->internal_line);
        err = err->next;
    }
}

void fmt_unary_operator(enum UnaryOperator op, String *dest) {
    switch(op) {
    case UNOP_BITNOT:
        String_append(dest, '~');
        return;
    case UNOP_BOOLNOT:
        String_append(dest, '!');
        return;
    case UNOP_NEGATIVE:
        String_append(dest, '-');
        return;
    case UNOP_POSITIVE:
        String_append(dest, '+');
        return;
    case UNOP_DEREF:
        String_append(dest, '*');
        return;
    case UNOP_ADDROF:
        String_append(dest, '&');
        return;
    case UNOP_PAREN:
        return;
    }
}

void fmt_binary_operator(enum BinaryOperator op, String* dest) {
    switch (op) {
        case BINOP_DIV:
            String_append(dest, L'/');
            return;
        case BINOP_MUL:
            String_append(dest, L'*');
            return;
        case BINOP_MOD:
            String_append(dest, L'%');
            return;
        case BINOP_SUB:
            String_append(dest, L'-');
            return;
        case BINOP_ADD:
            String_append(dest, L'+');
            return;
        case BINOP_BIT_LSHIFT:
            String_append_count(dest, "<<", 2);
            return;
        case BINOP_BIT_RSHIFT:
            String_append_count(dest, ">>", 2);
            return;
        case BINOP_CMP_LE:
            String_append_count(dest, "<=", 2);
            return;
        case BINOP_CMP_GE:
            String_append_count(dest, ">=", 2);
            return;
        case BINOP_CMP_G:
            String_append(dest, '>');
            return;
        case BINOP_CMP_L:
            String_append(dest, '<');
            return;
        case BINOP_CMP_EQ:
            String_append_count(dest, "==", 2);
            return;
        case BINOP_CMP_NEQ:
            String_append_count(dest, "!=", 2);
            return;
        case BINOP_BIT_XOR:
            String_append(dest, '^');
            return;
        case BINOP_BOOL_AND:
            String_append(dest, '&');
        case BINOP_BIT_AND:
            String_append(dest, '&');
            return;
        case BINOP_BOOL_OR:
            String_append(dest, '|');
        case BINOP_BIT_OR:
            String_append(dest, '|');
            return;
    }
}

void fmt_double(double d, String* dest) {
    if (d == 0.0) {
        String_append_count(dest, "0.0", 3);
        return;
    }
    bool negative = d < 0;
    if (d < 0) {
        d = -d;
    }
    int64_t exponent = 0;
    if (d < 0.000001) {
        while (d < 1.0) {
            d *= 10.0;
            --exponent;
        }
    } else if (d > 1000000) {
        while (d > 10.0) {
            d /= 10.0;
            ++exponent;
        }
    }
    uint64_t whole_part = (uint64_t)d;
    d = d - whole_part;
    uint64_t leading_zeroes = 0;
    uint64_t frac_part = 0;
    while (d > 0.000000000001 && d < 0.1) {
        ++leading_zeroes;
        d *= 10;
    }
    if (d >= 0.1) {
        for (uint64_t j = 0; j < 12; ++j) {
            d = d * 10;
            uint64_t i = (uint64_t) d;
            frac_part = frac_part * 10 + i;
            d = d - i;
        }
        if (d > 0.5) {
            frac_part += 1;
        }
        while (frac_part % 10 == 0) {
            frac_part = frac_part / 10;
        }
    }
    if (negative) {
        String_append(dest, '-');
    }
    String_format_append(dest, "%llu.", whole_part);
    for (uint64_t i = 0; i < leading_zeroes; ++i) {
        String_append(dest, '0');
    }
    String_format_append(dest, "%llu", frac_part);
    if (exponent != 0) {
        String_format_append(dest, "e%lld", exponent);
    }
}

void fmt_expression(const Expression* expr, const Parser* parser, String* dest) {
    switch (expr->kind) {
        case EXPRESSION_CALL:
            String_append(dest, L'(');
            fmt_expression(expr->call.function, parser, dest);
            String_append(dest, L'(');
            for (uint64_t ix = 0; ix < expr->call.arg_count; ++ix) {
                fmt_expression(expr->call.args[ix], parser, dest);
                if (ix < expr->call.arg_count - 1) {
                    String_append_count(dest, ", ", 2);
                }
            }
            String_append_count(dest, "))", 2);
            return;
        case EXPRESSION_BINOP:
            String_append(dest, '(');
            fmt_expression(expr->binop.lhs, parser, dest);
            String_append(dest, ' ');
            fmt_binary_operator(expr->binop.op, dest);
            String_append(dest, ' ');
            fmt_expression(expr->binop.rhs, parser, dest);
            String_append(dest, ')');
            return;
        case EXPRESSION_UNOP:
            String_append(dest, '(');
            fmt_unary_operator(expr->unop.op, dest);
            fmt_expression(expr->unop.expr, parser, dest);
            String_append(dest, ')');
            return;
        case EXPRESSION_LITERAL_BOOL:
            if (expr->literal.uint != 0) {
                String_extend(dest, "{true}");
            } else {
                String_extend(dest, "{false}");
            }
            return;
        case EXPRESSION_LITERAL_INT:
            String_format_append(dest, "{%lld}", expr->literal.iint);
            return;
        case EXPRESSION_LITERAL_UINT:
            String_format_append(dest, "{%llu}", expr->literal.uint);
            return;
        case EXPRESSION_LITERAL_FLOAT:
            String_append(dest, '{');
            fmt_double(expr->literal.float64, dest);
            String_append(dest, '}');
            return;
        case EXPRESSION_STRING:
            WString s;
            String_append(dest, '"');
            String_append_count(dest, (const char*)expr->string.str.bytes,
                                      expr->string.str.len);
            String_append(dest, '"');
            return;
        case EXPRESSION_VARIABLE:
            fmt_name(expr->variable.ix, parser, dest);
            return;
        case EXPRESSION_ARRAY_INDEX:
            String_append(dest, '(');
            fmt_expression(expr->array_index.array, parser, dest);
            String_append(dest, '[');
            fmt_expression(expr->array_index.index, parser, dest);
            String_append(dest, ']');
            String_append(dest, ')');
            return;
        case EXPRESSION_CAST:
            String_append(dest, '(');
            fmt_type(expr->cast.type, parser, dest);
            String_append(dest, ')');
            String_append(dest, '(');
            fmt_expression(expr->cast.e, parser, dest);
            String_append(dest, ')');
            return;
        default:
            String_extend(dest, "UNKOWN");
            return;
    }
}

void fmt_statement(const Statement* statement, const Parser* parser, String* dest) {
    switch (statement->type) {
    case STATEMENT_RETURN:
        String_extend(dest, "return ");
        fmt_expression(statement->return_.return_value, parser, dest);
        String_append(dest, ';');
        String_append(dest, '\n');
        return;
    case STATEMENT_ASSIGN:
        fmt_expression(statement->assignment.lhs, parser, dest);
        String_extend(dest, " = ");
        fmt_expression(statement->assignment.rhs, parser, dest);
        String_append(dest, ';');
        String_append(dest, '\n');
        return;
    case STATEMENT_EXPRESSION:
        fmt_expression(statement->expression, parser, dest);
        String_append(dest, ';');
        String_append(dest, '\n');
        return;
    case STATEMENT_IF:
        if (statement->if_.condition) {
            String_extend(dest, "if ");
            fmt_expression(statement->if_.condition, parser, dest);
            String_append(dest, ' ');
        }
        String_extend(dest, "{\n");
        for (uint64_t ix = 0; ix < statement->if_.statement_count; ++ix) {
            fmt_statement(statement->if_.statements[ix], parser, dest);
        }
        String_extend(dest, "}");
        if (statement->if_.else_branch != NULL) {
            String_extend(dest, " else ");
            fmt_statement(statement->if_.else_branch, parser, dest);
        } else {
            String_append(dest, '\n');
        }
        return;
    case STATEMENT_WHILE:
        String_extend(dest, "while ");
        fmt_expression(statement->if_.condition, parser, dest);
        String_extend(dest, " {\n");
        for (uint64_t ix = 0; ix < statement->if_.statement_count; ++ix) {
            fmt_statement(statement->if_.statements[ix], parser, dest);
        }
        String_extend(dest, "}\n");
        return;
    case STATEMET_INVALID:
        String_extend(dest, "BAD STATEMENT\n");
        break;
    }
}

void fmt_statements(Statement* const* s, uint64_t count, const Parser* parser, String* dest) {
    for (uint64_t ix = 0; ix < count; ++ix) {
        fmt_statement(s[ix], parser, dest);
    }
}
