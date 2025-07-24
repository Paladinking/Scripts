#include "parser.h"
#include "glob.h"
#include "printf.h"
#include "format.h"
#include "parse.h"
#include "type_checker.h"

#define PARSER(ctx) (((struct Tokenizer*)ctx)->parser)

struct Tokenizer {
    Parser* parser;
    Token last_token;
    uint64_t start;
    uint64_t end;
};

static inline Statement* create_stmt(Parser* p, enum StatementKind kind, uint64_t start,
                                     uint64_t end) {
    Statement* s = Arena_alloc_type(&p->arena, Statement);
    s->type = kind;
    s->line = (LineInfo){true, start, end};
    return s;
}

static inline Expression* create_expr(Parser* p, enum ExpressionKind kind, uint64_t start,
                                      uint64_t end) {

    Expression* e = Arena_alloc_type(&p->arena, Expression);
    e->type = TYPE_ID_INVALID;
    e->kind = kind;
    e->line = (LineInfo){true, start, end};
    return e;
}

Expression* OnCond(void* ctx, uint64_t start, uint64_t end, Expression* c) {
    begin_scope(PARSER(ctx));
    return c;
}

void* OnElseHead(void* ctx, uint64_t start, uint64_t end, name_id kw) {
    begin_scope(PARSER(ctx));
    return NULL;
}

name_id OnFnHead(void* ctx, uint64_t start, uint64_t end, name_id kw, StrWithLength s) {
    Parser* p = PARSER(ctx);
    name_id name = find_name(p, s.str, s.len);
    LineInfo l  = {true, start, end};
    if (name == NAME_ID_INVALID) {
        fatal_error(p, PARSE_ERROR_INTERNAL, l);
        return NAME_ID_INVALID;
    }

    if (p->name_table.data[name].kind != NAME_FUNCTION) {
        add_error(p, PARSE_ERROR_BAD_NAME, l);
    }
    begin_scope(p);
    return name;
}

Statement* OnWhile(void* ctx, uint64_t start, uint64_t end, name_id kw,
                   Expression* cond, Statements statements) {
    Statement* s = create_stmt(PARSER(ctx), STATEMENT_WHILE, start, end);
    s->while_.statements = statements.statements;
    s->while_.statement_count = statements.count;
    s->while_.condition = cond;
    return s;
}

Statement* OnIf(void* ctx, uint64_t start, uint64_t end, name_id kw,
                Expression* cond, Statements statements, Statement* else_) {
    Statement* s = create_stmt(PARSER(ctx), STATEMENT_IF, start, end);
    s->if_.condition = cond;
    s->if_.statement_count = statements.count;
    s->if_.statements = statements.statements;
    s->if_.else_branch = else_;
    return s;
}

Statement* OnElse(void* ctx, uint64_t start, uint64_t end, void* kw, Statements statements) {
    Statement* s = create_stmt(PARSER(ctx), STATEMENT_IF, start, end);
    s->if_.condition = NULL;
    s->if_.else_branch = NULL;
    s->if_.statements = statements.statements;
    s->if_.statement_count = statements.count;
    return s;
}

StatementList* OnStatementList(void* ctx, uint64_t start, uint64_t end, StatementList* list,
                               Statement* s) {
    if (s == NULL) {
        return list;
    }
    StatementList* l = Arena_alloc_type(&PARSER(ctx)->arena, StatementList);
    l->statement = s;
    l->next = list;
    return l;
}

Statement* OnAssign(void* ctx, uint64_t start, uint64_t end, Expression* lhs, Expression* rhs) {
    Statement* s = create_stmt(PARSER(ctx), STATEMENT_ASSIGN, start, end);
    s->assignment.lhs = lhs;
    s->assignment.rhs = rhs;
    return s;
}

Statement* OnReturn(void* ctx, uint64_t start, uint64_t end, name_id kw, Expression* e) {
    Statement* s = create_stmt(PARSER(ctx), STATEMENT_RETURN, start, end);
    s->return_.return_value = e;
    return s;
}

Statement* OnExprStatement(void* ctx, uint64_t start, uint64_t end, Expression* e) {
    Statement* s = create_stmt(PARSER(ctx), STATEMENT_EXPRESSION, start, end);
    s->expression = e;
    return s;
}

Statements OnStatements(void *ctx, uint64_t start, uint64_t end, StatementList * statements) {
    Statements s;
    uint64_t statement_count = 0;
    StatementList* sl = statements;
    while (sl != NULL) {
        ++statement_count;
        sl = sl->next;
    }

    s.count = statement_count;
    s.statements = Arena_alloc_count(&PARSER(ctx)->arena, Statement*, statement_count);
    sl = statements;
    for (uint64_t ix = 0; ix < statement_count; ++ix) {
        s.statements[statement_count - 1 - ix] = sl->statement;
        sl = sl->next;
    }
    end_scope(PARSER(ctx));
    return s;
}


ArgList* OnArgList(void* ctx, uint64_t start, uint64_t end, type_id type, StrWithLength s) {
    Parser* p = PARSER(ctx);
    name_id name = insert_variable_name(p, s.str, s.len, type, start, FUNC_ID_GLOBAL);
    if (name == NAME_ID_INVALID) {
        return NULL;
    }
    ArgList* l = Arena_alloc_type(&p->arena, ArgList);
    l->name = name;
    l->line = (LineInfo){true, start, end};
    l->next = NULL;
    return l;
}

ArgList* OnAddArgList(void* ctx, uint64_t start, uint64_t end, ArgList* l,
                      type_id type, StrWithLength s) {
    if (l == NULL) {
        return OnArgList(ctx, start, end, type, s);
    }
    Parser* p = PARSER(ctx);
    name_id name = insert_variable_name(p, s.str, s.len, type, start, FUNC_ID_GLOBAL);
    if (name == NAME_ID_INVALID) {
        return l;
    }
    ArgList* l2 = Arena_alloc_type(&p->arena, ArgList);
    l2->next = l;
    l2->line = (LineInfo){true, start, end};
    l2->name = name;
    return l2;
}


FunctionDef* OnFunction(void* ctx, uint64_t start, uint64_t end,
                        name_id fn_name, ArgList* arglist, type_id ret_type, 
                        Statements statements) {
    Parser* p = PARSER(ctx);
    if (fn_name == NAME_ID_INVALID) {
        return NULL;
    }
    LineInfo l = {true, start, end};
    if (p->name_table.data[fn_name].kind != NAME_FUNCTION) {
        return NULL;
    }

    FunctionDef* f = p->name_table.data[fn_name].func_def;
    func_id id = p->function_table.size;

    uint64_t arg_count = 0;
    ArgList* a = arglist;
    while (a != NULL) {
        ++arg_count;
        a = a->next;
    }

    f->arg_count = arg_count;
    a = arglist;
    f->args = Arena_alloc_count(&p->arena, CallArg, f->arg_count);
    for (uint64_t ix = 0; ix < arg_count; ++ix) {
        f->args[arg_count - 1 - ix].name = a->name;
        f->args[arg_count - 1 - ix].type = p->name_table.data[a->name].type;
        f->args[arg_count - 1 - ix].line = a->line;
        a = a->next;
    }

    f->statements = statements.statements;
    f->statement_count = statements.count;
    f->return_type = ret_type;

    if (p->function_table.size == p->function_table.capacity) {
        uint64_t c = p->function_table.size * 2;
        FunctionDef** ptr = Mem_realloc(p->function_table.data, c * sizeof(FunctionDef*));
        if (ptr == NULL) {
            out_of_memory(p);
            return NULL;
        }
        p->function_table.data = ptr;
        p->function_table.capacity = c;
    }

    p->function_table.data[p->function_table.size++] = f;
    return f;
}

Statement* OnDecl(void* ctx, uint64_t start, uint64_t end, type_id t, ArraySizes* arr,
                  StrWithLength ident, Expression* value) {
    Parser* p = PARSER(ctx);
    while (arr != NULL) {
        t = array_of(p, t, arr->size);
        arr = arr->next;
    }
    name_id name = insert_variable_name(p, ident.str, ident.len, t, start,
                                        p->function_table.size);
    if (name == NAME_ID_INVALID) {
        LineInfo l = {true, start, end};
        add_error(p, PARSE_ERROR_BAD_NAME, l);
    }
    if (value == NULL) {
        return NULL;
    }

    Statement* s = create_stmt(p, STATEMENT_ASSIGN, start, end);
    Expression* lhs = create_expr(p, EXPRESSION_VARIABLE, start, end);
    lhs->variable.ix = name;
    s->assignment.lhs = lhs;
    s->assignment.rhs = value;

    return s;
}

ArraySizes* OnArrayDecl(void* ctx, uint64_t start, uint64_t end, ArraySizes* sizes, uint64_t i) {
    ArraySizes* s = Arena_alloc_type(&PARSER(ctx)->arena, ArraySizes);
    s->size = i;
    s->next = sizes;
    return s;
}

Expression* OnTrue(void* ctx, uint64_t start, uint64_t end, name_id name) {
    Expression* e = create_expr(PARSER(ctx), EXPRESSION_LITERAL_BOOL, start, end);
    e->literal.uint = 1;
    return e;
}

Expression* OnFalse(void* ctx, uint64_t start, uint64_t end, name_id name) {
    Expression* e = create_expr(PARSER(ctx), EXPRESSION_LITERAL_BOOL, start, end);
    e->literal.uint = 0;
    return e;
}


Expression* OnInt(void* ctx, uint64_t start, uint64_t end, uint64_t i) {
    Expression* e = create_expr(PARSER(ctx), EXPRESSION_LITERAL_UINT, start, end);
    e->literal.uint = i;
    return e;
}

Expression* OnReal(void* ctx, uint64_t start, uint64_t end, double r) {
    Expression* e = create_expr(PARSER(ctx), EXPRESSION_LITERAL_FLOAT, start, end);
    e->literal.uint = r;
    return e;
}

Expression* OnString(void* ctx, uint64_t start, uint64_t end, StrWithLength s) {
    Expression* e = create_expr(PARSER(ctx), EXPRESSION_LITERAL_STRING, start, end);
    uint8_t* bytes = Arena_alloc_count(&PARSER(ctx)->arena, uint8_t, s.len);
    e->literal.string.len = s.len;
    e->literal.string.bytes = bytes;
    return e;
}

Expression* OnIdentExpr(void* ctx, uint64_t start, uint64_t end, StrWithLength ident) {
    Parser* p = PARSER(ctx);
    name_id name = find_name(p, ident.str, ident.len);
    LineInfo l = {true, start, end};

    Expression* e = create_expr(PARSER(ctx), EXPRESSION_VARIABLE, start, end);
    if (name == NAME_ID_INVALID) {
        add_error(p, PARSE_ERROR_BAD_NAME, e->line);
        e->kind = EXPRESSION_INVALID;
    } else {
        e->variable.ix = name;
    }
    return e;
}

Expression* OnParen(void* ctx, uint64_t start, uint64_t end, Expression* child) {
    Expression* e = create_expr(PARSER(ctx), EXPRESSION_UNOP, start, end);
    e->unop.op = UNOP_PAREN;
    e->unop.expr = child;
    return e;
}

ExpressionList* OnParamList(void* ctx, uint64_t start, uint64_t end, Expression* e) {
    ExpressionList* l = Arena_alloc_type(&PARSER(ctx)->arena, ExpressionList);
    l->expr = e;
    l->next = NULL;
    return l;
}

ExpressionList* OnAddParamList(void* ctx, uint64_t start, uint64_t end,
                               ExpressionList* l, Expression* e) {

    ExpressionList* l2 = Arena_alloc_type(&PARSER(ctx)->arena, ExpressionList);
    l2->expr = e;
    l2->next = l;
    return l2;
}

Expression* OnCall(void* ctx, uint64_t start, uint64_t end, Expression* f, ExpressionList* l) {
    Expression* e = create_expr(PARSER(ctx), EXPRESSION_CALL, start, end);
    e->kind = EXPRESSION_CALL;
    e->call.function = f;

    // TODO: Use linked list everywhere?
    uint64_t arg_count = 0;
    ExpressionList* list = l;
    while (list != NULL) {
        ++arg_count;
        list = list->next;
    }

    e->call.arg_count = arg_count;
    e->call.args = Arena_alloc_count(&PARSER(ctx)->arena, Expression*, arg_count);
    list = l;
    for (uint64_t ix = 0; ix < arg_count; ++ix) {
        e->call.args[arg_count - 1 - ix] = list->expr;
        list = list->next;
    }

    return e;
}


Expression* OnArrayIndex(void* ctx, uint64_t start, uint64_t end,
                         Expression* arr, Expression* ix) {
    Expression* e = create_expr(PARSER(ctx), EXPRESSION_ARRAY_INDEX, start, end);
    e->array_index.array = arr;
    e->array_index.index = ix;
    return e;
}

Expression* OnBinop1(void* ctx, uint64_t start, uint64_t end, Expression* e1,
                     enum BinaryOperator op, Expression* e2) {
    Expression* e = create_expr(PARSER(ctx), EXPRESSION_BINOP, start, end);
    e->binop.lhs = e1;
    e->binop.op = op;
    e->binop.rhs = e2;

    Expression* root = e;
    if (e1->kind == EXPRESSION_BINOP && 
           binop_precedence(e1->binop.op) > binop_precedence(op)) {
        Expression* rhs = e1->binop.rhs;
        e1->binop.rhs = e;
        e->binop.lhs = rhs;
        e->line.start = e->binop.lhs->line.start;
        e1->line.end = e->binop.rhs->line.end;
        root = e1;

        e1 = e->binop.lhs;
        while (e1->kind == EXPRESSION_BINOP &&
              binop_precedence(e1->binop.op > binop_precedence(e->binop.op))) {
            rhs = e1->binop.rhs;
            e1->binop.rhs = e;
            e->binop.lhs = rhs;
            e->line.start = e->binop.lhs->line.start;
            e1->line.end = e->binop.rhs->line.end;
            e1 = e->binop.lhs;
        }
    }

    return root;
}

Expression* OnBinop2(void* ctx, uint64_t start, uint64_t end, Expression* e1,
                     enum BinaryOperator op, Expression* e2) {
    return OnBinop1(ctx, start, end, e1, op, e2);
}

Expression* OnUnop1(void* ctx, uint64_t start, uint64_t end,
                    enum UnaryOperator op, Expression* child) {
    Expression* e = create_expr(PARSER(ctx), EXPRESSION_UNOP, start, end);
    e->unop.op = op;
    e->unop.expr = child;
    return e;
}

Expression* OnUnop2(void* ctx, uint64_t start, uint64_t end,
                    enum UnaryOperator op, Expression* child) {
    return OnUnop1(ctx, start, end, op, child);

}


bool is_pair(uint8_t c1, uint8_t c2) {
    if (c1 == '=' || c1 == '!') {
        return c2 == '=';
    }
    if (c1 == '<' || c1 == '>') {
        return c2 == c1 || c2 == '=';
    }
    if (c1 == '|' || c1 == '&') {
        return c2 == c1;
    }
    if (c1 == '-') {
        return c2 == '>';
    }
    return false;
}

Token peek_token(void *ctx, uint64_t *start, uint64_t *end) {
    struct Tokenizer* t = ctx;
    *start = t->start;
    *end = t->end;
    return t->last_token;
}

void parser_error(void *ctx, SyntaxError e) {
    struct Tokenizer* t = ctx;
    Parser* p = t->parser;
    LineInfo l = {true, e.start, e.end};
    add_error(p, PARSE_ERROR_INVALID_CHAR, l);
}

void dump_errors(Parser* parser) {
    String s;
    if (String_create(&s)) {
        fmt_errors(parser, &s);
        LOG_DEBUG("%s", s.buffer);
        outputUtf8_e(s.buffer, s.length);
        parser->first_error = NULL;
        parser->last_error = NULL;
        String_free(&s);
    }
}

void consume_token(void* ctx, uint64_t* start, uint64_t* end) {
    struct Tokenizer* t = ctx;
    *start = t->start;
    *end = t->end;
    Parser* p = t->parser;
    skip_spaces(p);
    if (p->pos >= p->input_size) {
        t->last_token.id = TOKEN_END;
        t->start = p->pos;
        t->end = p->pos;
        return;
    }
    t->start = p->pos;
    uint8_t c = p->indata[p->pos];
    if (is_identifier_start(c)) {
        uint32_t len;
        const uint8_t* ident = parse_name(p, &len);

        t->end = p->pos;
        name_id name = find_name(p, ident, len);
        if (name == NAME_ID_INVALID || 
            p->name_table.data[name].kind == NAME_VARIABLE ||
            p->name_table.data[name].kind == NAME_FUNCTION) {
            // NAME_VARIABLE and NAME_FUNCTION can be shadowed, so yeild string
            // instead of id.
            t->last_token.id = TOKEN_IDENTIFIER;
            t->last_token.identifier.str = ident;
            t->last_token.identifier.len = len;
        } else if (name < NAME_ID_BUILTIN_COUNT) {
            static enum TokenType map[] = {
                TOKEN_KFN, TOKEN_KSTRUCT, TOKEN_KIF, TOKEN_KWHILE, TOKEN_KELSE, 
                TOKEN_KRETURN, TOKEN_KTRUE, TOKEN_KFALSE
            };
            t->last_token.id = map[name];
            t->last_token.kFn = name;
        } else if (p->name_table.data[name].kind == NAME_TYPE) {
            t->last_token.id = TOKEN_TYPEID;
            t->last_token.typeid = p->name_table.data[name].type;
        } else {
            LineInfo l = {false, 0, 0};
            fatal_error(p, PARSE_ERROR_INTERNAL, l);
        }
        return;
    }
    if (c == '"') {
        uint64_t len;
        uint8_t* str = parse_string_literal(p, &len);
        t->end = p->pos;
        t->last_token.id = TOKEN_STRING;
        t->last_token.string.str = str;
        t->last_token.string.len = len;
        return;
    }
    if (c == '.' || (c >= '0' && c <= '9')) {
        uint64_t i;
        double d;
        bool is_int;
        if (parse_number_literal(p, &i, &d, &is_int)) {
            if (is_int) {
                t->last_token.id = TOKEN_INTEGER;
                t->last_token.integer = i;
            } else {
                t->last_token.id = TOKEN_REAL;
                t->last_token.real = d;
            }
        } else {
            LineInfo i = {true, p->pos, p->pos};
            add_error(p, PARSE_ERROR_INVALID_LITERAL, i);
            t->last_token.id = TOKEN_INTEGER;
            t->last_token.integer = 0;
        }
        t->end = p->pos;
        return;
    }
    ++p->pos;
    if (p->pos < p->input_size && is_pair(c, p->indata[p->pos])) {
        ++p->pos;
    }
    t->end = p->pos;
    t->last_token.id = TOKEN_LITERAL;
    t->last_token.literal = (const char*)p->indata + t->start;
}


int compiler(int argc, char** argv) {
    Parser parser;
    if (!Parser_create(&parser)) {
        LOG_ERROR("Failed creating parser");
        return 1;
    }

    String s;
    if (!read_text_file(&s, oL("src\\compiler\\program.txt"))) {
        LOG_ERROR("Failed reading program.txt\n");
        return 1;
    }

    scan_program(&parser, &s);
    if (parser.first_error != NULL) {
        dump_errors(&parser);
        return 1;
    }

    parser.pos = 0;
    parser.indata = s.buffer;
    parser.input_size = s.length;
    struct Tokenizer t;
    t.parser = &parser;
    t.start = 0;
    t.end = 0;
    uint64_t start, end;
    consume_token(&t, &start, &end);

    parse(&t);
    if (parser.first_error != NULL) {
        dump_errors(&parser);
        return 1;
    }

    if (parser.first_error == NULL) {
        String s;
        if (String_create(&s)) {
            for (uint64_t i = 0; i < parser.function_table.size; ++i) {
                String_clear(&s);
                FunctionDef* f = parser.function_table.data[i];
                fmt_functiondef(f, &parser, &s);
                outputUtf8(s.buffer, s.length);
            }
            String_free(&s);
        }
        Quads q;
        q.quads_count = 0;
        Quad_GenerateQuads(&parser, &q, &parser.arena);

        if (parser.first_error != NULL) {
            String s;
            if (String_create(&s)) {
                fmt_quads(&q, &s);
                outputUtf8(s.buffer, s.length);
                String_free(&s);
            }
        }
    }


    int status = 0;
    if (parser.first_error != NULL) {
        dump_errors(&parser);
        status = 1;
    }

    String_free(&s);

    return status;
}

#ifdef WIN32
int main() {
    // Obtain utf8 command line

    wchar_t* cmd = GetCommandLineW();
    String argbuf;
    String_create(&argbuf);
    if (!String_from_utf16_str(&argbuf, cmd)) {
        return 1;
    }
    int argc;
    char** argv = parse_command_line(argbuf.buffer, &argc);
    String_free(&argbuf);
    Log_Init();
    int status = compiler(argc, argv);
    Log_Shutdown();
    Mem_free(argv);

    // Force shutdown to avoid potential threadpool deadlock
    ExitProcess(status);
}

#else
int main(char** argv, int argc) {
    Log_Init();
    int status = compiler(argv, argc);
    Log_Shutdown();
    return status;
}
#endif

