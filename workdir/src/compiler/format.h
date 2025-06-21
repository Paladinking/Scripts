#ifndef PARSER_FMT_H
#define PARSER_FMT_H

#include "parser.h"
#include "quads.h"
#include "dynamic_string.h"

void fmt_name(name_id name, const Parser* parser, WString* dest);

void fmt_type(type_id type, const Parser* parser, WString* dest);

void fmt_errors(const Parser* parser, WString* dest);

void fmt_statements(Statement* const* s, uint64_t count,
                    const Parser* parser, WString* dest);

void fmt_statement(const Statement* statement, const Parser* parser, WString* dest);

void fmt_expression(const Expression* expr, const Parser* parser, WString* dest);

void fmt_unary_operator(enum UnaryOperator op, WString *dest);

void fmt_binary_operator(enum BinaryOperator op, WString* dest);

void fmt_double(double d, WString* dest);

void fmt_functiondef(const FunctionDef* def, const Parser* parser, WString* dest);

void fmt_quad(const Quad* quad, WString* dest);

void fmt_quads(const Quads* quads, WString* dest);

#endif
