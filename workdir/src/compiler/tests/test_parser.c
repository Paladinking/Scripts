#include "../parser.h"
#include "../format.h"
#include <printf.h>

#define abs(a) ((a) < 0 ? (a) : (-a))

int main() {
    Parser parser;
    if (!Parser_create(&parser)) {
        _wprintf_e(L"Failed creating parser\n");
        return 1;
    }

    //                   (1 && (2 - (3 / 4)))
    const char * data = "1 && 2 - 3 / 4";
    parser.indata = (uint8_t*)data;
    parser.input_size = strlen(data);
    Expression* expr = parse_expression(&parser);
    WString output;
    WString_create(&output);
    fmt_expression(expr, &parser, &output);

    assert(expr->kind == EXPRESSION_BINOP);
    assert(expr->binop.op == BINOP_BOOL_AND);
    assert(expr->line.start == 0);
    assert(expr->line.end == 14);

    assert(expr->binop.lhs->kind == EXPRESSION_LITERAL_UINT);
    assert(expr->binop.lhs->literal.uint == 1);
    assert(expr->binop.lhs->line.start == 0);
    assert(expr->binop.lhs->line.end == 1);

    assert(expr->binop.rhs->kind == EXPRESSION_BINOP);
    assert(expr->binop.rhs->binop.op == BINOP_SUB);
    assert(expr->binop.rhs->line.start == 5);
    assert(expr->binop.rhs->line.end == 14);

    assert(expr->binop.rhs->binop.lhs->kind == EXPRESSION_LITERAL_UINT);
    assert(expr->binop.rhs->binop.lhs->literal.uint == 2);
    assert(expr->binop.rhs->binop.lhs->line.start == 5);
    assert(expr->binop.rhs->binop.lhs->line.end == 6);

    assert(expr->binop.rhs->binop.rhs->kind == EXPRESSION_BINOP);
    assert(expr->binop.rhs->binop.rhs->binop.op == BINOP_DIV);
    assert(expr->binop.rhs->binop.rhs->line.start == 9);
    assert(expr->binop.rhs->binop.rhs->line.end == 14);

    assert(expr->binop.rhs->binop.rhs->binop.lhs->kind == EXPRESSION_LITERAL_UINT);
    assert(expr->binop.rhs->binop.rhs->binop.lhs->literal.uint == 3);
    assert(expr->binop.rhs->binop.rhs->binop.lhs->line.start == 9);
    assert(expr->binop.rhs->binop.rhs->binop.lhs->line.end == 10);

    assert(expr->binop.rhs->binop.rhs->binop.rhs->kind == EXPRESSION_LITERAL_UINT);
    assert(expr->binop.rhs->binop.rhs->binop.rhs->literal.uint == 4);
    assert(expr->binop.rhs->binop.rhs->binop.rhs->line.start == 13);
    assert(expr->binop.rhs->binop.rhs->binop.rhs->line.end == 14);
    assert(parser.first_error == NULL);

    // ((1 - 2) && (3 / 4))
    data = "1.2e-4 - .2e20 && 3e1 / 4";
    parser.indata = (uint8_t*)data;
    parser.input_size = strlen(data);
    parser.pos = 0;
    expr = parse_expression(&parser);
    WString_clear(&output);
    fmt_expression(expr, &parser, &output);

    if (parser.first_error != NULL) {
        _wprintf_e(L"Error: %d: %S line %llu\n", parser.first_error->kind,
                parser.first_error->file, parser.first_error->internal_line);
    }


    assert(expr->kind == EXPRESSION_BINOP);
    assert(expr->binop.op == BINOP_BOOL_AND);
    assert(expr->line.start == 0);
    assert(expr->line.end == 25);

    assert(expr->binop.lhs->kind == EXPRESSION_BINOP);
    assert(expr->binop.lhs->binop.op == BINOP_SUB);
    assert(expr->binop.lhs->line.start == 0);
    assert(expr->binop.lhs->line.end == 14);

    assert(expr->binop.lhs->binop.lhs->kind == EXPRESSION_LITERAL_FLOAT);
    assert(abs(expr->binop.lhs->binop.lhs->literal.float64 - 1.2e-4) < 1e-20);
    assert(expr->binop.lhs->binop.lhs->line.start == 0);
    assert(expr->binop.lhs->binop.lhs->line.end == 6);

    assert(expr->binop.lhs->binop.rhs->kind == EXPRESSION_LITERAL_FLOAT);
    assert(abs(expr->binop.lhs->binop.rhs->literal.float64 - .2e20) < 1e-20);
    assert(expr->binop.lhs->binop.rhs->line.start == 9);
    assert(expr->binop.lhs->binop.rhs->line.end == 14);

    assert(expr->binop.rhs->kind == EXPRESSION_BINOP);
    assert(expr->binop.rhs->binop.op == BINOP_DIV);
    assert(expr->binop.rhs->line.start == 18);
    assert(expr->binop.rhs->line.end == 25);

    assert(expr->binop.rhs->binop.lhs->kind == EXPRESSION_LITERAL_FLOAT);
    assert(abs(expr->binop.rhs->binop.lhs->literal.float64 - 3e1) < 1e-20);
    assert(expr->binop.rhs->binop.lhs->line.start == 18);
    assert(expr->binop.rhs->binop.lhs->line.end == 21);

    assert(expr->binop.rhs->binop.rhs->kind == EXPRESSION_LITERAL_UINT);
    assert(expr->binop.rhs->binop.rhs->literal.uint == 4);
    assert(expr->binop.rhs->binop.rhs->line.start == 24);
    assert(expr->binop.rhs->binop.rhs->line.end == 25);
    assert(parser.first_error == NULL);

    // (((1 / 2) - 3) && 4)
    data = "1 / 2 - 3 && 4";
    parser.indata = (uint8_t*)data;
    parser.input_size = strlen(data);
    parser.pos = 0;
    expr = parse_expression(&parser);
    WString_clear(&output);
    fmt_expression(expr, &parser, &output);
    assert(WString_equals_str(&output, L"((({1} / {2}) - {3}) && {4})"));

    data = "1() / 2(\"a\", \"b\\x3b\\\"c\") - 3() && 4()";
    parser.indata = (uint8_t*)data;
    parser.input_size = strlen(data);
    parser.pos = 0;
    expr = parse_expression(&parser);
    WString_clear(&output);
    fmt_expression(expr, &parser, &output);
    assert(WString_equals_str(&output, L"(((({1}()) / ({2}(\"a\", \"b;\"c\"))) - ({3}())) && ({4}()))"));
    assert(parser.first_error == NULL);

    data = "1() / 2(\"a\", \"b\") - 3(!!!5 + +-6[1+-5] + ~!7) && 4()";
    parser.indata = (uint8_t*)data;
    parser.input_size = strlen(data);
    parser.pos = 0;
    expr = parse_expression(&parser);
    WString_clear(&output);
    fmt_expression(expr, &parser, &output);
    assert(WString_equals_str(&output, L"(((({1}()) / ({2}(\"a\", \"b\"))) - ({3}(((!(!(!{5}))) + ((+(-({6}[({1} + (-{5}))]))) + (~(!{7}))))))) && ({4}()))"));
    assert(parser.first_error == NULL);

    uint64_t count;
    data = "uint64 a = 10; int64 __bee1 = 2 * 2; a >> __bee1; if (a > __bee1) { float64 x = 10; }"
           "else if (__bee1 >= a) { float64 x = 11; while (x != 0.0) { x = x - 1; } } else { a = 5; return a; }"
           "uint64[10] array; array[0] = 123; }";
    parser.indata = (uint8_t*)data;
    parser.input_size = strlen(data);
    parser.pos = 0;
    Statement** statements = parse_statements(&parser, &count);
    WString_clear(&output);
    assert(parser.first_error == NULL);
    assert(statements != NULL);
    assert(count == 5);
    assert(statements[0]->type == STATEMENT_ASSIGN);
    assert(statements[0]->assignment.lhs->kind == EXPRESSION_VARIABLE);
    name_id a = statements[0]->assignment.lhs->variable.ix;
    type_id a_type = type_of(&parser, a);
    assert(parser.name_table.data[a].kind == NAME_VARIABLE);
    assert(a_type == TYPE_ID_UINT64);
    assert(parser.type_table.data[a_type].kind == TYPE_NORMAL);
    assert(parser.type_table.data[a_type].type_def->kind == TYPEDEF_UINT64);
    const uint8_t* name = parser.name_table.data[a].name;
    uint32_t len = parser.name_table.data[a].name_len;
    assert(len == 1);
    assert(name[0] == L'a');
    assert(statements[0]->assignment.rhs->kind == EXPRESSION_LITERAL_UINT);
    assert(statements[0]->assignment.rhs->literal.uint == 10);
    assert(statements[1]->type == STATEMENT_ASSIGN);
    assert(statements[1]->assignment.lhs->kind == EXPRESSION_VARIABLE);
    name_id b = statements[1]->assignment.lhs->variable.ix;
    assert(b != a);
    assert(parser.name_table.data[b].kind == NAME_VARIABLE);
    assert(type_of(&parser, b) == TYPE_ID_INT64);
    name = parser.name_table.data[b].name;
    len = parser.name_table.data[b].name_len;
    assert(len == 6);
    assert(memcmp(name, "__bee1", 6) == 0);
    assert(statements[1]->assignment.rhs->kind == EXPRESSION_BINOP);
    assert(statements[1]->assignment.rhs->binop.op == BINOP_MUL);
    assert(statements[1]->assignment.rhs->binop.lhs->kind == EXPRESSION_LITERAL_UINT);
    assert(statements[1]->assignment.rhs->binop.rhs->kind == EXPRESSION_LITERAL_UINT);
    assert(statements[1]->assignment.rhs->binop.lhs->literal.uint == 2);
    assert(statements[1]->assignment.rhs->binop.rhs->literal.uint == 2);
    assert(statements[2]->type == STATEMENT_EXPRESSION);
    assert(statements[2]->expression->kind == EXPRESSION_BINOP);
    assert(statements[2]->expression->binop.op == BINOP_BIT_RSHIFT);
    assert(statements[2]->expression->binop.lhs->kind == EXPRESSION_VARIABLE);
    assert(statements[2]->expression->binop.lhs->variable.ix == a);
    assert(statements[2]->expression->binop.rhs->kind == EXPRESSION_VARIABLE);
    assert(statements[2]->expression->binop.rhs->variable.ix == b);
    assert(statements[3]->type == STATEMENT_IF);
    assert(statements[3]->if_.condition != NULL);
    assert(statements[3]->if_.condition->kind == EXPRESSION_BINOP);
    assert(statements[3]->if_.condition->binop.op == BINOP_CMP_G);
    assert(statements[3]->if_.condition->binop.lhs->kind == EXPRESSION_VARIABLE);
    assert(statements[3]->if_.condition->binop.rhs->kind == EXPRESSION_VARIABLE);
    assert(statements[3]->if_.condition->binop.lhs->variable.ix == a);
    assert(statements[3]->if_.condition->binop.rhs->variable.ix == b);
    assert(statements[3]->if_.statement_count == 1);
    assert(statements[3]->if_.statements[0]->type == STATEMENT_ASSIGN);
    assert(statements[3]->if_.statements[0]->assignment.lhs->kind == EXPRESSION_VARIABLE);
    name_id x = statements[3]->if_.statements[0]->assignment.lhs->variable.ix;
    assert(x != a);
    assert(x != b);
    assert(parser.name_table.data[x].kind == NAME_VARIABLE);
    assert(type_of(&parser, x) == TYPE_ID_FLOAT64);
    name = parser.name_table.data[x].name;
    len = parser.name_table.data[x].name_len;
    assert(len == 1);
    assert(name[0] == 'x');
    assert(statements[3]->if_.statements[0]->assignment.rhs->kind == EXPRESSION_LITERAL_UINT);
    assert(statements[3]->if_.statements[0]->assignment.rhs->literal.uint == 10);
    assert(statements[3]->if_.else_branch != NULL);
    Statement* el = statements[3]->if_.else_branch;
    assert(el->type == STATEMENT_IF);
    assert(el->if_.condition != NULL);
    assert(el->if_.condition->kind == EXPRESSION_BINOP);
    assert(el->if_.condition->binop.op == BINOP_CMP_GE);
    assert(el->if_.condition->binop.lhs->kind == EXPRESSION_VARIABLE);
    assert(el->if_.condition->binop.rhs->kind == EXPRESSION_VARIABLE);
    assert(el->if_.condition->binop.lhs->variable.ix == b);
    assert(el->if_.condition->binop.rhs->variable.ix == a);
    assert(el->if_.statement_count == 2);
    assert(el->if_.statements[0]->type == STATEMENT_ASSIGN);
    assert(el->if_.statements[0]->assignment.lhs->kind == EXPRESSION_VARIABLE);
    name_id x2 = el->if_.statements[0]->assignment.lhs->variable.ix;
    assert(x2 != x);
    assert(x2 != a);
    assert(x2 != b);
    assert(parser.name_table.data[x2].kind == NAME_VARIABLE);
    assert(type_of(&parser, x2) == TYPE_ID_FLOAT64);
    name = parser.name_table.data[x2].name;
    len = parser.name_table.data[x2].name_len;
    assert(len == 1);
    assert(name[0] == 'x');
    assert(el->if_.statements[0]->assignment.rhs->kind == EXPRESSION_LITERAL_UINT);
    assert(el->if_.statements[0]->assignment.rhs->literal.uint == 11);
    Statement* wl = el->if_.statements[1];
    assert(wl->type == STATEMENT_WHILE);
    assert(wl->while_.condition != NULL);
    assert(wl->while_.condition->kind == EXPRESSION_BINOP);
    assert(wl->while_.condition->binop.op == BINOP_CMP_NEQ);
    assert(wl->while_.condition->binop.lhs->kind == EXPRESSION_VARIABLE);
    assert(wl->while_.condition->binop.lhs->variable.ix == x2);
    assert(wl->while_.condition->binop.rhs->kind == EXPRESSION_LITERAL_FLOAT);
    assert(wl->while_.condition->binop.rhs->literal.float64 == 0.0);
    assert(wl->while_.statement_count == 1);
    assert(wl->while_.statements[0]->type == STATEMENT_ASSIGN);
    assert(wl->while_.statements[0]->assignment.lhs->kind == EXPRESSION_VARIABLE);
    assert(wl->while_.statements[0]->assignment.lhs->variable.ix == x2);
    assert(wl->while_.statements[0]->assignment.rhs->kind == EXPRESSION_BINOP);
    assert(wl->while_.statements[0]->assignment.rhs->binop.op == BINOP_SUB);
    assert(wl->while_.statements[0]->assignment.rhs->binop.lhs->kind == EXPRESSION_VARIABLE);
    assert(wl->while_.statements[0]->assignment.rhs->binop.lhs->variable.ix == x2);
    assert(wl->while_.statements[0]->assignment.rhs->binop.rhs->kind == EXPRESSION_LITERAL_UINT);
    assert(wl->while_.statements[0]->assignment.rhs->binop.rhs->literal.uint == 1);
    assert(el->if_.else_branch != NULL);
    el = el->if_.else_branch;
    assert(el->type == STATEMENT_IF);
    assert(el->if_.condition == NULL);
    assert(el->if_.statement_count == 2);
    assert(el->if_.statements[0]->type == STATEMENT_ASSIGN);
    assert(el->if_.statements[0]->assignment.lhs->kind == EXPRESSION_VARIABLE);
    assert(el->if_.statements[0]->assignment.lhs->variable.ix == a);
    assert(el->if_.statements[0]->assignment.rhs->kind == EXPRESSION_LITERAL_UINT);
    assert(el->if_.statements[0]->assignment.rhs->literal.uint == 5);
    assert(el->if_.statements[1]->type == STATEMENT_RETURN);
    assert(el->if_.statements[1]->return_.return_value->kind == EXPRESSION_VARIABLE);
    assert(el->if_.statements[1]->return_.return_value->variable.ix == a);

    assert(statements[4]->type == STATEMENT_ASSIGN);
    assert(statements[4]->assignment.lhs->kind == EXPRESSION_ARRAY_INDEX);
    assert(statements[4]->assignment.lhs->array_index.index->kind == EXPRESSION_LITERAL_UINT);
    assert(statements[4]->assignment.lhs->array_index.index->literal.uint == 0);
    assert(statements[4]->assignment.lhs->array_index.array->kind == EXPRESSION_VARIABLE);
    name_id arr = statements[4]->assignment.lhs->array_index.array->variable.ix;
    assert(arr != a);
    assert(arr != b);
    assert(arr != x);
    assert(arr != x2);
    type_id t = type_of(&parser, arr);
    assert(t == array_of(&parser, TYPE_ID_UINT64));
    name = parser.name_table.data[arr].name;
    len = parser.name_table.data[arr].name_len;
    assert(len == 5);
    assert(memcmp(name, "array", 5) == 0);
    assert(parser.name_table.data[arr].kind == NAME_VARIABLE);
    assert(parser.type_table.data[t].kind == TYPE_ARRAY);
    assert(parser.type_table.data[t].type_def->kind == TYPEDEF_UINT64);
    assert(parser.type_table.data[t].parent == TYPE_ID_UINT64);
    assert(parser.name_table.data[arr].array_size != NULL);
    assert(parser.name_table.data[arr].array_size->size == 10);
    assert(parser.name_table.data[arr].array_size->next == NULL);
    assert(statements[4]->assignment.rhs->kind == EXPRESSION_LITERAL_UINT);
    assert(statements[4]->assignment.rhs->literal.uint == 123);
    _wprintf_e(L"All tests passed\n");

    return 0;
}
