#include "format.h"

const static wchar_t* quadnames[] = {
    L"QDIV", L"QMUL", L"QMOD", L"QSUB", L"QADD",
    L"QRSHIFT", L"QLSHIFT", L"QNEGATE", L"QCMP_EQ",
    L"QCMP_NEQ", L"QCMP_G", L"QCMP_L", L"QCMP_GE",
    L"QCMP_LE", L"QJMP", L"QJMP_FALSE", L"QJMP_TRUE",
    L"QLABEL", L"QBOOL_AND", L"QBOOL_OR", L"QBOOL_NOT",
    L"QBIT_AND", L"QBIT_OR", L"QBIT_XOR", L"QBIT_NOT",
    L"QCAST_TO_FLOAT64", L"QCAST_TO_INT64", L"QCAST_TO_UINT64",
    L"QCAST_TO_BOOL", L"QPUT_ARG",  L"QCALL", L"QRETURN",
    L"QGET_RET", L"QMOVE", L"QGET_ADDR", L"QCALC_ADDR",
    L"QSET_ADDR", L"QCREATE"
};

void fmt_quad(const Quad* quad, WString* dest) {
    enum QuadType type = quad->type & QUAD_TYPE_MASK;
    if (type >= 0 && type < 38) {
        WString_extend(dest, quadnames[type]);
    } else {
        WString_extend(dest, L"QUNKOWN");
        return;
    }
    WString_format_append(dest, L" ");
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
        WString_format_append(dest, L"%llu %llu -> %llu", quad->op1.var, quad->op2,
                              quad->dest);
        break;
    case QUAD_JMP:
    case QUAD_LABEL:
        WString_format_append(dest, L"%llu", quad->op1.label);
        break;
    case QUAD_JMP_FALSE:
    case QUAD_JMP_TRUE:
        WString_format_append(dest, L"%llu %llu", quad->op1.label, quad->op2);
        break;
    case QUAD_PUT_ARG:
    case QUAD_RETURN:
        WString_format_append(dest, L"%llu", quad->op1.var);
        break;
    case QUAD_CALL:
        WString_format_append(dest, L"$%llu", quad->op1.var);
        break;
    case QUAD_GET_RET:
        WString_format_append(dest, L"-> %llu", quad->dest);
        break;
    case QUAD_SET_ADDR:
        WString_format_append(dest, L"%llu %llu", quad->op1.var, quad->op2);
        break;
    case QUAD_CREATE:
        WString_format_append(dest, L"[] -> %llu", quad->dest);
        break;
    case QUAD_NEGATE:
    case QUAD_BOOL_NOT:
    case QUAD_BIT_NOT:
    case QUAD_CAST_TO_FLOAT64:
    case QUAD_CAST_TO_INT64:
    case QUAD_CAST_TO_UINT64:
    case QUAD_CAST_TO_BOOL:
    case QUAD_MOVE:
        WString_format_append(dest, L"%llu -> %llu", quad->op1.var, quad->dest);
        break;
    }
    if (quad->type & QUAD_UINT) {
        WString_extend(dest, L" UINT");
    } else if (quad->type & QUAD_SINT) {
        WString_extend(dest, L" SINT");
    } else if (quad->type & QUAD_FLOAT) {
        WString_extend(dest, L" FLOAT");
    } else if (quad->type & QUAD_BOOL) {
        WString_extend(dest, L" BOOL");
    }
    if (quad->type & QUAD_64_BIT) {
        WString_extend(dest, L" 64");
    } else if (quad->type & QUAD_32_BIT) {
        WString_extend(dest, L" 32");
    } else if (quad->type & QUAD_16_BIT) {
        WString_extend(dest, L" 16");
    } else if (quad->type & QUAD_8_BIT) {
        WString_extend(dest, L" 8");
    }
    if (quad->type & QUAD_SCALE_8) {
        WString_extend(dest, L" SCALE 8");
    } else if (quad->type & QUAD_SCALE_4) {
        WString_extend(dest, L" SCALE 4");
    } else if (quad->type & QUAD_SCALE_2) {
        WString_extend(dest, L" SCALE 2");
    } else if (quad->type & QUAD_SCALE_1) {
        WString_extend(dest, L" SCALE 1");
    }
}

void fmt_quads(const Quads* quads, WString* dest) {
    for (uint64_t ix = 0; ix < quads->quads_count; ++ix) {
        fmt_quad(&quads->quads[ix], dest);
        WString_append(dest, L'\n');
    } 
}

void fmt_functiondef(const FunctionDef* def, const Parser* parser, WString* dest) {
    WString_extend(dest, L"FunctionDef ");
    fmt_name(def->name, parser, dest);
    WString_append(dest, L'(');
    for (uint64_t i = 0; i < def->arg_count; ++i) {
        fmt_type(def->args[i].type, parser, dest);
        WString_append(dest, L' ');
        fmt_name(def->args[i].name, parser, dest);
        if (i + 1 != def->arg_count) {
            WString_append_count(dest, L", ", 2);
        }
    }
    WString_extend(dest, L") -> ");
    fmt_type(def->return_type, parser, dest);
    WString_append_count(dest, L" {\n", 3);
    fmt_statements(def->statements, def->statement_count, parser, dest);
    WString_append_count(dest, L"}\n", 2);
}

void fmt_name(name_id name, const Parser* parser, WString* dest) {
    if (name == NAME_ID_INVALID) {
        WString_extend(dest, L"<Invalid name>");
        return;
    }
    const uint8_t* b = parser->name_table.data[name].name;
    uint32_t len = parser->name_table.data[name].name_len;
    WString_append_utf8_bytes(dest, (const char*)b, len);
    WString_format_append(dest, L"#%llu", name);
}

void fmt_type(type_id type, const Parser* parser, WString* dest) {
    if (type == TYPE_ID_INVALID) {
        WString_extend(dest, L"<Invalid type>");
        return;
    }
    const TypeDef* def = parser->type_table.data[type].type_def;
    name_id name;
    if (def->kind == TYPEDEF_STRUCT) {
        WString_extend(dest, L"<Struct '");
        name = def->struct_.name;
    } else if (def->kind == TYPEDEF_FUNCTION) {
        WString_extend(dest, L"<Function type '");
        name = def->func->name;
    } else {
        name = def->name;
    }
    fmt_name(name, parser, dest);
    if (def->kind == TYPEDEF_STRUCT || def->kind == TYPEDEF_FUNCTION) {
        WString_append_count(dest, L"'>", 2);
    }
    while (parser->type_table.data[type].kind == TYPE_ARRAY) {
        WString_append_count(dest, L"[]", 2);
        type = parser->type_table.data[type].parent;
    }
}

void fmt_errors(const Parser* parser, WString* dest) {
    ParseError* err = parser->first_error;
    while (err != NULL) {
        if (err->kind == PARSE_ERROR_NONE) {
            continue;
        }
        switch (err->kind) {
        case PARSE_ERROR_NONE:
            break;
        case PARSE_ERROR_BAD_TYPE:
            WString_extend(dest, L"Error: bad type");
            break;
        case PARSE_ERROR_EOF:
            WString_extend(dest, L"Error: unexpecetd end of file");
            break;
        case PARSE_ERROR_OUTOFMEMORY:
            WString_extend(dest, L"Error: Out of memory");
            break;
        case PARSE_ERROR_INVALID_ESCAPE:
            WString_extend(dest, L"Error: invalid escape code");
            break;
        case PARSE_ERROR_INVALID_CHAR:
            WString_extend(dest, L"Error: bad character");
            break;
        case PARSE_ERROR_RESERVED_NAME:
            WString_extend(dest, L"Error: illegal name");
            break;
        case PARSE_ERROR_BAD_NAME:
            WString_extend(dest, L"Error: bad identifier");
            break;
        case PARSE_ERROR_INVALID_LITERAL:
            WString_extend(dest, L"Error: invalid literal");
            break;
        case PARSE_ERROR_REDEFINITION:
            WString_extend(dest, L"Error: redefinition");
            break;
        case PARSE_ERROR_BAD_ARRAY_SIZE:
            WString_extend(dest, L"Error: Bad array size");
            break;
        case PARSE_ERROR_INTERNAL:
            WString_extend(dest, L"Internal compiler error");
            break;
        case TYPE_ERROR_ILLEGAL_TYPE:
            WString_extend(dest, L"Type Error: Illegal type");
            break;
        case TYPE_ERROR_ILLEGAL_CAST:
            WString_extend(dest, L"Type Error: Illegal cast");
            break;
        case TYPE_ERROR_WRONG_ARG_COUNT:
            WString_extend(dest, L"Type Error: Wrong number of arguments");
        }
        uint64_t row, col;
        find_row_col(parser, err->pos.start, &row, &col);
        ++row;
        ++col;
        WString_format_append(dest, L" at row %llu collum %llu [%S:%llu]\n",
                              row, col, err->file, err->internal_line);
        err = err->next;
    }
}

void fmt_unary_operator(enum UnaryOperator op, WString *dest) {
    switch(op) {
    case UNOP_BITNOT:
        WString_append(dest, '~');
        return;
    case UNOP_BOOLNOT:
        WString_append(dest, '!');
        return;
    case UNOP_NEGATVIE:
        WString_append(dest, '-');
        return;
    case UNOP_POSITIVE:
        WString_append(dest, '+');
        return;
    case UNOP_PAREN:
        return;
    }
}

void fmt_binary_operator(enum BinaryOperator op, WString* dest) {
    switch (op) {
        case BINOP_DIV:
            WString_append(dest, L'/');
            return;
        case BINOP_MUL:
            WString_append(dest, L'*');
            return;
        case BINOP_MOD:
            WString_append(dest, L'%');
            return;
        case BINOP_SUB:
            WString_append(dest, L'-');
            return;
        case BINOP_ADD:
            WString_append(dest, L'+');
            return;
        case BINOP_BIT_LSHIFT:
            WString_append_count(dest, L"<<", 2);
            return;
        case BINOP_BIT_RSHIFT:
            WString_append_count(dest, L">>", 2);
            return;
        case BINOP_CMP_LE:
            WString_append_count(dest, L"<=", 2);
            return;
        case BINOP_CMP_GE:
            WString_append_count(dest, L">=", 2);
            return;
        case BINOP_CMP_G:
            WString_append(dest, L'>');
            return;
        case BINOP_CMP_L:
            WString_append(dest, L'<');
            return;
        case BINOP_CMP_EQ:
            WString_append_count(dest, L"==", 2);
            return;
        case BINOP_CMP_NEQ:
            WString_append_count(dest, L"!=", 2);
            return;
        case BINOP_BIT_XOR:
            WString_append(dest, L'^');
            return;
        case BINOP_BOOL_AND:
            WString_append(dest, L'&');
        case BINOP_BIT_AND:
            WString_append(dest, L'&');
            return;
        case BINOP_BOOL_OR:
            WString_append(dest, L'|');
        case BINOP_BIT_OR:
            WString_append(dest, L'|');
            return;
    }
}

void fmt_double(double d, WString* dest) {
    if (d == 0.0) {
        WString_append_count(dest, L"0.0", 3);
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
        WString_append(dest, L'-');
    }
    WString_format_append(dest, L"%llu.", whole_part);
    for (uint64_t i = 0; i < leading_zeroes; ++i) {
        WString_append(dest, L'0');
    }
    WString_format_append(dest, L"%llu", frac_part);
    if (exponent != 0) {
        WString_format_append(dest, L"e%lld", exponent);
    }
}

void fmt_expression(const Expression* expr, const Parser* parser, WString* dest) {
    switch (expr->kind) {
        case EXPRESSION_CALL:
            WString_append(dest, L'(');
            fmt_expression(expr->call.function, parser, dest);
            WString_append(dest, L'(');
            for (uint64_t ix = 0; ix < expr->call.arg_count; ++ix) {
                fmt_expression(expr->call.args[ix], parser, dest);
                if (ix < expr->call.arg_count - 1) {
                    WString_append_count(dest, L", ", 2);
                }
            }
            WString_append_count(dest, L"))", 2);
            return;
        case EXPRESSION_BINOP:
            WString_append(dest, L'(');
            fmt_expression(expr->binop.lhs, parser, dest);
            WString_append(dest, L' ');
            fmt_binary_operator(expr->binop.op, dest);
            WString_append(dest, L' ');
            fmt_expression(expr->binop.rhs, parser, dest);
            WString_append(dest, L')');
            return;
        case EXPRESSION_UNOP:
            WString_append(dest, L'(');
            fmt_unary_operator(expr->unop.op, dest);
            fmt_expression(expr->unop.expr, parser, dest);
            WString_append(dest, L')');
            return;
        case EXPRESSION_LITERAL_BOOL:
            if (expr->literal.uint != 0) {
                WString_extend(dest, L"{true}");
            } else {
                WString_extend(dest, L"{false}");
            }
            return;
        case EXPRESSION_LITERAL_INT:
            WString_format_append(dest, L"{%lld}", expr->literal.iint);
            return;
        case EXPRESSION_LITERAL_UINT:
            WString_format_append(dest, L"{%llu}", expr->literal.uint);
            return;
        case EXPRESSION_LITERAL_FLOAT:
            WString_append(dest, L'{');
            fmt_double(expr->literal.float64, dest);
            WString_append(dest, L'}');
            return;
        case EXPRESSION_LITERAL_STRING:
            WString s;
            WString_append(dest, '"');
            WString_append_utf8_bytes(dest, (const char*)expr->literal.string.bytes,
                                      expr->literal.string.len);
            WString_append(dest, '"');
            return;
        case EXPRESSION_VARIABLE:
            fmt_name(expr->variable.ix, parser, dest);
            return;
        case EXPRESSION_ARRAY_INDEX:
            WString_append(dest, '(');
            fmt_expression(expr->array_index.array, parser, dest);
            WString_append(dest, '[');
            fmt_expression(expr->array_index.index, parser, dest);
            WString_append(dest, ']');
            WString_append(dest, ')');
            return;
        case EXPRESSION_CAST:
            WString_append(dest, '(');
            fmt_type(expr->cast.type, parser, dest);
            WString_append(dest, ')');
            WString_append(dest, '(');
            fmt_expression(expr->cast.e, parser, dest);
            WString_append(dest, ')');
            return;
        default:
            WString_extend(dest, L"UNKOWN");
            return;
    }
}

void fmt_statement(const Statement* statement, const Parser* parser, WString* dest) {
    switch (statement->type) {
    case STATEMENT_RETURN:
        WString_extend(dest, L"return ");
        fmt_expression(statement->return_.return_value, parser, dest);
        WString_append(dest, L';');
        WString_append(dest, L'\n');
        return;
    case STATEMENT_ASSIGN:
        fmt_expression(statement->assignment.lhs, parser, dest);
        WString_extend(dest, L" = ");
        fmt_expression(statement->assignment.rhs, parser, dest);
        WString_append(dest, L';');
        WString_append(dest, L'\n');
        return;
    case STATEMENT_EXPRESSION:
        fmt_expression(statement->expression, parser, dest);
        WString_append(dest, L';');
        WString_append(dest, L'\n');
        return;
    case STATEMENT_IF:
        if (statement->if_.condition) {
            WString_extend(dest, L"if ");
            fmt_expression(statement->if_.condition, parser, dest);
            WString_append(dest, L' ');
        }
        WString_extend(dest, L"{\n");
        for (uint64_t ix = 0; ix < statement->if_.statement_count; ++ix) {
            fmt_statement(statement->if_.statements[ix], parser, dest);
        }
        WString_extend(dest, L"}");
        if (statement->if_.else_branch != NULL) {
            WString_extend(dest, L" else ");
            fmt_statement(statement->if_.else_branch, parser, dest);
        } else {
            WString_append(dest, '\n');
        }
        return;
    case STATEMENT_WHILE:
        WString_extend(dest, L"while ");
        fmt_expression(statement->if_.condition, parser, dest);
        WString_extend(dest, L" {\n");
        for (uint64_t ix = 0; ix < statement->if_.statement_count; ++ix) {
            fmt_statement(statement->if_.statements[ix], parser, dest);
        }
        WString_extend(dest, L"}\n");
        return;
    case STATEMET_INVALID:
        WString_extend(dest, L"BAD STATEMENT");
        break;
    }
}

void fmt_statements(Statement* const* s, uint64_t count, const Parser* parser, WString* dest) {
    for (uint64_t ix = 0; ix < count; ++ix) {
        fmt_statement(s[ix], parser, dest);
    }
}
