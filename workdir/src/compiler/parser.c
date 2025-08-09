#include "language/parse.h"
#include "tokenizer.h"

const LineInfo LINE_INFO_NONE = {-1, -1};

void parser_add_keyword(Parser* parser, const char* keyword, name_id id) {
    StrWithLength s = {(const uint8_t*)keyword, strlen(keyword)};
    name_id n = name_insert(&parser->name_table, s, TYPE_ID_INVALID,
                            NAME_KEYWORD, &parser->arena);
    if (n != id) {
        fatal_error(parser, PARSE_ERROR_INTERNAL, LINE_INFO_NONE);
    }
}

void parser_add_builtin(Parser* parser, type_id id, uint32_t size, uint32_t align,
                        const char* name) {
    TypeDef* def = Arena_alloc_type(&parser->arena, TypeDef);
    def->kind = TYPEDEF_BUILTIN;
    def->builtin.byte_size = size;
    def->builtin.byte_alignment = align;
    parser->type_table.data[id].kind = TYPE_NORMAL;
    parser->type_table.data[id].array_size = 0;
    parser->type_table.data[id].parent = TYPE_ID_INVALID;
    parser->type_table.data[id].type_def = def;
    parser->type_table.data[id].ptr_type = TYPE_ID_INVALID;

    StrWithLength str = {(const uint8_t*)name, strlen(name)};

    name_id n = name_type_insert(&parser->name_table, str, id, def, &parser->arena);
    def->builtin.name = n;
}

bool Parser_create(Parser* parser) {
    LOG_DEBUG("Creating parser");
    if (!Arena_create(&parser->arena, 0x7fffffff, out_of_memory,
                      parser)) {
        return false;
    }
    FunctionDef** function_data = Mem_alloc(16 * sizeof(FunctionDef*));
    if (function_data == NULL) {
        Arena_free(&parser->arena);
        return false;
    }
    parser->function_table.data = function_data;
    parser->function_table.size = 0;
    parser->function_table.capacity = 16;

    FunctionDef** extern_data = Mem_alloc(16 * sizeof(FunctionDef*));
    if (extern_data == NULL) {
        Arena_free(&parser->arena);
        Mem_free(function_data);
        return false;
    }
    parser->externs.data = extern_data;
    parser->externs.size = 0;
    parser->externs.capacity = 16;


    TypeData* type_data = Mem_alloc((TYPE_ID_BUILTIN_COUNT + 16) * sizeof(TypeData));
    if (type_data == NULL) {
        Arena_free(&parser->arena);
        Mem_free(function_data);
        Mem_free(extern_data);
        return false;
    }

    parser->type_table.capacity = TYPE_ID_BUILTIN_COUNT + 16;
    parser->type_table.size = TYPE_ID_BUILTIN_COUNT;
    parser->type_table.data = type_data;
    NameData* name_data = Mem_alloc(16 * sizeof(NameData));
    if (name_data == NULL) {
        Arena_free(&parser->arena);
        Mem_free(function_data);
        Mem_free(extern_data);
        Mem_free(type_data);
        return false;
    }
    parser->name_table.capacity = 16;
    parser->name_table.size = 0;
    parser->name_table.data = name_data;

    for (uint64_t ix = 0; ix < NAME_TABLE_HASH_ENTRIES; ++ix) {
        parser->name_table.hash_map[ix] = NAME_ID_INVALID;
    }

    parser->name_table.scope_stack = Mem_alloc(8 * sizeof(name_id));
    if (parser->name_table.scope_stack == NULL) {
        Arena_free(&parser->arena);
        Mem_free(function_data);
        Mem_free(extern_data);
        Mem_free(type_data);
        Mem_free(name_data);
        return false;
    }
    parser->name_table.scope_count = 1;
    parser->name_table.scope_capacity = 8;

    parser->pos = 0;
    parser->input_size = 0;
    parser->indata = NULL;

    parser->first_error = NULL;
    parser->last_error = NULL;

    parser->first_str = NULL;
    parser->last_str = NULL;

    parser_add_keyword(parser, "fn", NAME_ID_FN);
    parser_add_keyword(parser, "struct", NAME_ID_STRUCT);
    parser_add_keyword(parser, "if", NAME_ID_IF);
    parser_add_keyword(parser, "while", NAME_ID_WHILE);
    parser_add_keyword(parser, "else", NAME_ID_ELSE);
    parser_add_keyword(parser, "return", NAME_ID_RETURN);
    parser_add_keyword(parser, "true", NAME_ID_TRUE);
    parser_add_keyword(parser, "false", NAME_ID_FALSE);
    parser_add_keyword(parser, "extern", NAME_ID_EXTERN);
    parser_add_keyword(parser, "null", NAME_ID_NULL);

    parser_add_builtin(parser, TYPE_ID_UINT64, 8, 8, "uint64");
    parser_add_builtin(parser, TYPE_ID_UINT32, 4, 4, "uint32");
    parser_add_builtin(parser, TYPE_ID_UINT16, 2, 2, "uint16");
    parser_add_builtin(parser, TYPE_ID_UINT8, 1, 1, "uint8");
    parser_add_builtin(parser, TYPE_ID_INT64, 8, 8, "int64");
    parser_add_builtin(parser, TYPE_ID_INT32, 4, 4, "int32");
    parser_add_builtin(parser, TYPE_ID_INT16, 2, 2, "int16");
    parser_add_builtin(parser, TYPE_ID_INT8, 1, 1, "int8");
    parser_add_builtin(parser, TYPE_ID_FLOAT64, 8, 8, "float64");
    parser_add_builtin(parser, TYPE_ID_FLOAT32, 4, 4, "float32");
    parser_add_builtin(parser, TYPE_ID_NONE, 0, 1, "<None Type>");
    parser_add_builtin(parser, TYPE_ID_BOOL, 1, 1, "bool");
    parser_add_builtin(parser, TYPE_ID_NULL, 8, 8, "<Null Type>");

    parser->name_table.scope_stack[0] = parser->name_table.size - 1;

    return true;
}

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
    s->line = (LineInfo){start, end};
    return s;
}

static inline Expression* create_expr(Parser* p, enum ExpressionKind kind, uint64_t start,
                                      uint64_t end) {

    Expression* e = Arena_alloc_type(&p->arena, Expression);
    e->type = TYPE_ID_INVALID;
    e->var = VAR_ID_INVALID;
    e->kind = kind;
    e->line = (LineInfo){start, end};
    return e;
}


uint32_t binop_precedence(enum BinaryOperator op) {
    switch (op) {
    case BINOP_DIV:
    case BINOP_MUL:
    case BINOP_MOD:
        return 0;
    case BINOP_SUB:
    case BINOP_ADD:
        return 1;
    case BINOP_BIT_LSHIFT:
    case BINOP_BIT_RSHIFT:
        return 2;
    case BINOP_CMP_LE:
    case BINOP_CMP_GE:
    case BINOP_CMP_G:
    case BINOP_CMP_L:
        return 3;
    case BINOP_CMP_EQ:
    case BINOP_CMP_NEQ:
        return 4;
    case BINOP_BIT_AND:
        return 5;
    case BINOP_BIT_XOR:
        return 6;
    case BINOP_BIT_OR:
        return 7;
    case BINOP_BOOL_AND:
        return 8;
    case BINOP_BOOL_OR:
        return 9;
    }
    return 100;
}

Expression* OnCond(void* ctx, uint64_t start, uint64_t end, Expression* c) {
    name_scope_begin(&PARSER(ctx)->name_table);
    return c;
}

void* OnElseHead(void* ctx, uint64_t start, uint64_t end, name_id kw) {
    name_scope_begin(&PARSER(ctx)->name_table);
    return NULL;
}

name_id OnFnHead(void* ctx, uint64_t start, uint64_t end, name_id kw, StrWithLength s) {
    Parser* p = PARSER(ctx);
    name_id name = name_find(&p->name_table, s);
    LineInfo l  = {start, end};
    if (name == NAME_ID_INVALID) {
        fatal_error(p, PARSE_ERROR_INTERNAL, l);
        return NAME_ID_INVALID;
    }

    if (p->name_table.data[name].kind != NAME_FUNCTION) {
        add_error(p, PARSE_ERROR_BAD_NAME, l);
    }
    name_scope_begin(&p->name_table);
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
    s->assignment.ptr_copy = false;
    return s;
}

Statement* OnReturn(void* ctx, uint64_t start, uint64_t end, name_id kw, Expression* e) {
    Statement* s = create_stmt(PARSER(ctx), STATEMENT_RETURN, start, end);
    s->return_.return_value = e;
    s->return_.ptr_return = false;
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
    name_scope_end(&PARSER(ctx)->name_table);
    return s;
}

Statement* declare_variable(Parser* p, uint64_t start, uint64_t end,
                      StrWithLength ident, type_id type, Expression* val) {
    LineInfo l = {start, end};
    name_id name = name_variable_insert(&p->name_table, ident, type,
                                        p->function_table.size, &p->arena);
    if (name == NAME_ID_INVALID) {
        name_id prev = name_find(&p->name_table, ident);
        assert(prev != NAME_ID_INVALID);
        if (p->name_table.data[prev].kind == NAME_KEYWORD) {
            add_error(p, PARSE_ERROR_RESERVED_NAME, l);
        } else {
            add_error(p, PARSE_ERROR_REDEFINITION, l);
        }
    }
    if (val == NULL) {
        return NULL;
    }

    Statement* s = create_stmt(p, STATEMENT_ASSIGN, start, end);
    Expression* lhs = create_expr(p, EXPRESSION_VARIABLE, start, end);
    lhs->variable.ix = name;
    s->assignment.lhs = lhs;
    s->assignment.rhs = val;
    s->assignment.ptr_copy = false;

    return s;
}

Arg* OnArg(void* ctx, uint64_t start, uint64_t end, type_id type, StrWithLength s) {
    Parser* p = PARSER(ctx);
    LineInfo pos = {start, end};
    name_id name = name_variable_insert(&p->name_table, s, type, p->function_table.size,
                                        &p->arena);
    if (name == NAME_ID_INVALID) {
        name_id prev = name_find(&p->name_table, s);
        assert(prev != NAME_ID_INVALID);
        if (p->name_table.data[prev].kind == NAME_KEYWORD) {
            add_error(p, PARSE_ERROR_RESERVED_NAME, pos);
        } else {
            add_error(p, PARSE_ERROR_REDEFINITION, pos);
        }
        return NULL;
    }
    Arg* arg = Arena_alloc_type(&p->arena, Arg);
    arg->name = name;
    arg->line = pos;
    return arg;
}

ArgList* OnArgList(void* ctx, uint64_t start, uint64_t end, Arg* arg) {
    Parser* p = PARSER(ctx);
    if (arg == NULL) {
        return NULL;
    }
    ArgList* l = Arena_alloc_type(&p->arena, ArgList);
    l->arg = *arg;
    l->next = NULL;
    return l;
}

ArgList* OnAddArgList(void* ctx, uint64_t start, uint64_t end, ArgList* l,
                      Arg* arg) {
    if (l == NULL) {
        return OnArgList(ctx, start, end, arg);
    }
    if (arg == NULL) {
        return l;
    }
    ArgList* l2 = Arena_alloc_type(&PARSER(ctx)->arena, ArgList);
    l2->arg = *arg;
    l2->next = l;
    return l2;
}

void add_func(Parser* p, ArgList* args, func_id id, FunctionDef* f,
              FunctionTable* table, Statement** statements, uint64_t statement_count,
              type_id ret_type) {
    uint64_t arg_count = 0;
    ArgList* a = args;
    while (a != NULL) {
        ++arg_count;
        a = a->next;
    }

    f->arg_count = arg_count;
    f->statements = statements;
    f->statement_count = statement_count;
    f->return_type = ret_type;
    a = args;

    f->args = Arena_alloc_count(&p->arena, FunctionArg, f->arg_count);
    for (uint64_t ix = 0; ix < arg_count; ++ix) {
        f->args[arg_count - 1 - ix].name = a->arg.name;
        f->args[arg_count - 1 - ix].type = p->name_table.data[a->arg.name].type;
        f->args[arg_count - 1 - ix].line = a->arg.line;
        p->name_table.data[a->arg.name].function = id;
        a = a->next;
    }

    if (table->size == table->capacity) {
        uint64_t c = table->size * 2;
        FunctionDef** ptr = Mem_realloc(table->data, c * sizeof(FunctionDef*));
        if (ptr == NULL) {
            out_of_memory(p);
            return;
        }
        table->data = ptr;
        table->capacity = c;
    }

    table->data[table->size++] = f;
}


FunctionDef* OnExternFunction(void* ctx, uint64_t start, uint64_t end, name_id kwExtr,
                              name_id fn_name, ArgList* arglist,
                              type_id ret_type) {
    Parser* p = PARSER(ctx);
    LineInfo l  = {start, end};

    if (p->name_table.data[fn_name].kind != NAME_FUNCTION) {
        return NULL;
    }

    FunctionDef* f = p->name_table.data[fn_name].func_def;
    add_func(p, arglist, FUNC_ID_NONE, f, &p->externs, NULL, 0, ret_type);

    name_scope_end(&p->name_table);

    return f;
}

type_id OnType(void* ctx, uint64_t start, uint64_t end, type_id type, 
               ArraySizes* sizes) {
    Parser* p = PARSER(ctx);
    while (sizes != NULL) {
        type = type_array_of(&p->type_table, type, sizes->size);
        sizes = sizes->next;
    }

    return type;
}

type_id OnPtrType(void* ctx, uint64_t start, uint64_t end, type_id type,
                  uint64_t ptr_count, ArraySizes* sizes) {
    Parser* p = PARSER(ctx);
    for (uint64_t i = 0; i < ptr_count; ++i) {
        type = type_ptr_of(&p->type_table, type);
    }
    return OnType(ctx, start, end, type, sizes);
}

type_id OnStruct(void* ctx, uint64_t start, uint64_t end, name_id kwStruct,
                 type_id type, FieldList* fields) {
    Parser* p = PARSER(ctx);
    uint64_t field_count = 0;
    FieldList* f = fields;
    while (f != NULL) {
        ++field_count;
        f = f->next;
    }
    // Sanity check
    assert(field_count < 0xffffff);

    StructMember* members = Arena_alloc_count(&p->arena, StructMember, field_count);

    f = fields;
    for (uint32_t ix = 0; ix < field_count; ++ix) {
        members[field_count - ix - 1] = f->field;
        f = f->next;
    }

    assert(type != TYPE_ID_INVALID);
    TypeDef* def = p->type_table.data[type].type_def;
    def->struct_.line.start = start;
    def->struct_.line.end = start;
    def->struct_.fields = members;
    def->struct_.field_count = field_count;
    // Cannot initialize byte_size and byte_alignment yet
    // in case this struct contains another struct defined later

    return type;
}

FieldList* OnField(void* ctx, uint64_t start, uint64_t end, FieldList* fields, 
                   type_id type, StrWithLength ident) {
    FieldList* f = Arena_alloc_type(&PARSER(ctx)->arena, FieldList);
    f->field.type = type;
    f->field.name = ident;
    f->field.line = (LineInfo){start, end};
    f->field.offset = 0;
    f->next = fields;
    return f;
}


FunctionDef* OnFunction(void* ctx, uint64_t start, uint64_t end,
                        name_id fn_name, ArgList* arglist, type_id ret_type, 
                        Statements statements) {
    Parser* p = PARSER(ctx);
    if (fn_name == NAME_ID_INVALID) {
        return NULL;
    }
    LineInfo l = {start, end};
    if (p->name_table.data[fn_name].kind != NAME_FUNCTION) {
        return NULL;
    }

    FunctionDef* f = p->name_table.data[fn_name].func_def;
    func_id id = p->function_table.size;
    add_func(p, arglist, id, f, &p->function_table, statements.statements, 
             statements.count, ret_type);

    return f;
}

Statement* OnDecl(void* ctx, uint64_t start, uint64_t end, type_id type, 
                  StrWithLength ident, Expression* value) {
    Parser* p = PARSER(ctx);
    return declare_variable(p, start, end, ident, type, value);
}

ArraySizes* OnArrayDecl(void* ctx, uint64_t start, uint64_t end, ArraySizes* sizes, uint64_t i) {
    ArraySizes* s = Arena_alloc_type(&PARSER(ctx)->arena, ArraySizes);
    s->size = i;
    s->next = sizes;
    return s;
}

Expression* OnNull(void* ctx, uint64_t start, uint64_t end, name_id name) {
    Expression* e = create_expr(PARSER(ctx), EXPRESSION_LITERAL_NULL, start, end);
    e->literal.uint = 0;
    return e;
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
    Parser* parser = PARSER(ctx);
    Expression* e = create_expr(parser, EXPRESSION_STRING, start, end);
    uint8_t* bytes = Arena_alloc_count(&parser->arena, uint8_t, s.len + 1);
    memcpy(bytes, s.str, s.len);
    bytes[s.len] = '\0';
    e->string.str.bytes = bytes;
    e->string.str.len = s.len;
    e->string.str.next = NULL;
    e->string.str.var = VAR_ID_INVALID;

    if (parser->first_str == NULL) {
        parser->first_str = &e->string.str;
        parser->last_str = &e->string.str;
    } else {
        parser->last_str->next = &e->string.str;
    }

    return e;
}

Expression* OnIdentExpr(void* ctx, uint64_t start, uint64_t end, StrWithLength ident) {
    Parser* p = PARSER(ctx);
    name_id name = name_find(&p->name_table, ident);
    LineInfo l = {start, end};

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

Expression* OnMemberRef(void* ctx, uint64_t start, uint64_t end, 
                        Expression* structvar, StrWithLength ident) {
    Parser* p = PARSER(ctx);
    Expression* e = create_expr(p, EXPRESSION_ACCESS_MEMBER, start, end);
    e->member_access.structexpr = structvar;
    e->member_access.member_offset = 0;
    uint8_t* member = Arena_alloc_count(&p->arena, uint8_t, ident.len);
    memcpy(member, ident.str, ident.len);
    e->member_access.member.str = member;
    e->member_access.member.len = ident.len;
    e->member_access.get_addr = false;
    e->member_access.via_ptr = false;
    return e;
}

Expression* OnCall(void* ctx, uint64_t start, uint64_t end, Expression* f, ExpressionList* l) {
    Expression* e = create_expr(PARSER(ctx), EXPRESSION_CALL, start, end);
    e->call.function = f;
    e->call.ptr_return = false;
    e->call.get_addr = false;

    // TODO: Use linked list everywhere?
    uint64_t arg_count = 0;
    ExpressionList* list = l;
    while (list != NULL) {
        ++arg_count;
        list = list->next;
    }

    e->call.arg_count = arg_count;
    e->call.args = Arena_alloc_count(&PARSER(ctx)->arena, CallArg, arg_count);
    list = l;
    for (uint64_t ix = 0; ix < arg_count; ++ix) {
        e->call.args[arg_count - 1 - ix].e = list->expr;
        e->call.args[arg_count - 1 - ix].ptr_copy = false;
        list = list->next;
    }

    return e;
}


Expression* OnArrayIndex(void* ctx, uint64_t start, uint64_t end,
                         Expression* arr, Expression* ix) {
    Expression* e = create_expr(PARSER(ctx), EXPRESSION_ARRAY_INDEX, start, end);
    e->array_index.array = arr;
    e->array_index.index = ix;
    e->array_index.get_addr = false;
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
    LineInfo l = {e.start, e.end};
    add_error(p, PARSE_ERROR_INVALID_CHAR, l);
}

void consume_token(void* ctx, uint64_t* start, uint64_t* end) {
    struct Tokenizer* t = ctx;
    *start = t->start;
    *end = t->end;
    Parser* p = t->parser;
    parser_skip_spaces(p);
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
        const uint8_t* ident = parser_read_name(p, &len);

        t->end = p->pos;
        StrWithLength s = {ident, len};
        name_id name = name_find(&p->name_table, s);
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
                TOKEN_KWFN, TOKEN_KWSTRUCT, TOKEN_KWIF, TOKEN_KWWHILE, TOKEN_KWELSE,
                TOKEN_KWRETURN, TOKEN_KWTRUE, TOKEN_KWFALSE, TOKEN_KWEXTERN,
                TOKEN_KWNULL
            };
            t->last_token.id = map[name];
            t->last_token.kwFn = name;
        } else if (p->name_table.data[name].kind == NAME_TYPE) {
            t->last_token.id = TOKEN_TYPEID;
            t->last_token.typeid = p->name_table.data[name].type;
        } else {
            fatal_error(p, PARSE_ERROR_INTERNAL, LINE_INFO_NONE);
        }
        return;
    }
    if (c == '"') {
        uint64_t len;
        uint8_t* str = parser_read_string(p, &len);
        if (str != NULL) {
            t->last_token.string.str = str;
            t->last_token.string.len = len;
        } else {
            t->last_token.string.str = NULL;
            t->last_token.string.len = 0;
        }
        t->end = p->pos;
        t->last_token.id = TOKEN_STRING;
        return;
    }

    bool leading_dec_point = c == '.' && p->pos + 1 < p->input_size &&
            p->indata[p->pos + 1] >= '0' && p->indata[p->pos + 1] <= '9';

    if (leading_dec_point || (c >= '0' && c <= '9')) {
        uint64_t i;
        double d;
        bool is_int;
        if (parser_read_number(p, &i, &d, &is_int)) {
            if (is_int) {
                t->last_token.id = TOKEN_INTEGER;
                t->last_token.integer = i;
            } else {
                t->last_token.id = TOKEN_REAL;
                t->last_token.real = d;
            }
        } else {
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

void parse_program(Parser* parser, String *indata) {
    parser_set_input(parser, indata);
    struct Tokenizer t;
    t.parser = parser;
    t.start = 0;
    t.end = 0;
    uint64_t start, end;
    consume_token(&t, &start, &end);

    parse(&t);
}

