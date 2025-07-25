#include "language/scan.h"
#include "tokenizer.h"

struct Tokenizer {
    Parser* parser;
    Token last_token;
    uint64_t start;
    uint64_t end;
};

Token scanner_peek_token(void* ctx, uint64_t* start, uint64_t* end) {
    struct Tokenizer* t = ctx;
    *start = t->start;
    *end = t->end;
    return t->last_token;
}

void scanner_error(void *ctx, SyntaxError e) {
    struct Tokenizer *t = ctx;
    Parser* parser = t->parser;
    LineInfo l = {e.start, e.end};
    add_error(parser, PARSE_ERROR_INVALID_CHAR, l);
}

void scanner_consume_token(void* ctx, uint64_t* start, uint64_t* end) {
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
    if (p->indata[p->pos] == '{') {
        parser_skip_statement(p);
        if (p->pos >= p->input_size) {
            t->last_token.id = TOKEN_END;
            t->start = p->pos;
            t->end = p->pos;
            return;
        }
        t->end = p->pos;
        t->last_token.id = TOKEN_STATEMENTS;
        return;
    }
    if (is_identifier_start(p->indata[p->pos])) {
        uint32_t len;
        const uint8_t* ident = parser_read_name(p, &len);

        t->end = p->pos;
        StrWithLength s = {ident, len};
        name_id name = name_find(&p->name_table, s);
        // The scanner considers builtin names as literals
        if (name != NAME_ID_INVALID && name < NAME_ID_BUILTIN_COUNT) {
            t->last_token.literal = (const char*) ident;
            t->last_token.id = TOKEN_LITERAL;
        } else {
            t->last_token.id = TOKEN_IDENTIFIER;
            t->last_token.identifier.str = ident;
            t->last_token.identifier.len = len;
        }
        return;
    }
    if (p->pos + 1 < p->input_size && p->indata[p->pos] == '-' &&
        p->indata[p->pos + 1] == '>') {
        p->pos += 2;
    } else {
        p->pos += 1;
    }
    t->end = p->pos;
    t->last_token.id = TOKEN_LITERAL;
    t->last_token.literal = (const char*)p->indata + t->start;
}

FunctionDef* OnScanFunction(void* ctx, uint64_t start, uint64_t end, StrWithLength name, 
                        uint64_t arg_count, int64_t ret, int64_t statements) {
    struct Tokenizer *t = ctx;
    Parser* parser = t->parser;
    uint64_t len = end - start;
    LOG_DEBUG("Scanned function %.*s (%llu - %llu) with %llu args\n", 
              name.len, name.str, start, end, arg_count);

    FunctionDef* func = Arena_alloc_type(&parser->arena, FunctionDef);
    func->arg_count = arg_count;
    func->return_type = TYPE_ID_INVALID;
    func->line.start = start;
    func->line.end = end;

    name_id id = name_function_insert(&parser->name_table, name, &parser->type_table,
                                      func, &parser->arena);
    if (id == NAME_ID_INVALID) {
        LineInfo l = {start, end};
        add_error(parser, PARSE_ERROR_BAD_NAME, l);
        return false;
    }

    func->name = id;
    return func;
}

int64_t OnScanProgram(void* ctx, uint64_t start, uint64_t end, int64_t i, FunctionDef* f) {
    return 0;
}

void scan_program(Parser* parser, String* indata) {
    parser_set_input(parser, indata);

    struct Tokenizer t;
    t.parser = parser;
    uint64_t start, end;
    scanner_consume_token(&t, &start, &end);

    scanner_parse(&t);
}
