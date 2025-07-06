#include "parser.h"
#include "glob.h"
#include "printf.h"
#include "format.h"
#include "scan.h"

struct Tokenizer {
    Parser* parser;
    Token last_token;
    uint64_t start;
    uint64_t end;
};

void skip_spaces(Parser* parser);
void skip_statement(Parser* parser);

static inline bool is_identifier(uint8_t c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
             c == '_' || (c >= '0' && c <= '9');
}

static inline bool is_identifier_start(uint8_t c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c == '_');
}

const uint8_t* parse_name(Parser* parser, uint32_t* len);

name_id find_name(Parser* parser, const uint8_t* name, uint32_t len);

Token scanner_peek_token(void* ctx, uint64_t* start, uint64_t* end) {
    struct Tokenizer* t = ctx;
    *start = t->start;
    *end = t->end;
    return t->last_token;
}

void scanner_error(void *ctx, SyntaxError e) {
    struct Tokenizer *t = ctx;
    Parser* parser = t->parser;
    LineInfo l;
    l.start = e.start;
    l.end = e.end;
    add_error(parser, PARSE_ERROR_INVALID_CHAR, l);
}

void scanner_consume_token(void* ctx, uint64_t* start, uint64_t* end) {
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
    if (p->indata[p->pos] == '{') {
        skip_statement(p);
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
        const uint8_t* ident = parse_name(p, &len);

        t->end = p->pos;
        name_id name = find_name(p, ident, len);
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

FunctionDef* OnFunction(void* ctx, uint64_t start, uint64_t end, StrWithLength name, 
                        uint64_t arg_count, int64_t ret, int64_t statements) {
    struct Tokenizer *t = ctx;
    Parser* parser = t->parser;
    uint64_t len = end - start;
    _printf("Parsed function %.*s (%llu - %llu) with %llu args\n", 
            name.len, name.str, start, end, arg_count);

    FunctionDef* func = Arena_alloc_type(&parser->arena, FunctionDef);
    func->arg_count = arg_count;
    func->return_type = TYPE_ID_INVALID;
    func->line.start = start;
    func->line.end = end;

    name_id id = insert_function_name(parser, name.str, name.len, func);
    if (id == NAME_ID_INVALID) {
        LineInfo l = {true, start, end};
        add_error(parser, PARSE_ERROR_BAD_NAME, l);
        return false;
    }

    func->name = id;
    return func;
}

int64_t OnProgram(void* ctx, uint64_t start, uint64_t end, int64_t i, FunctionDef* f) {
    return 0;
}



void dump_errors(Parser* parser) {
    String s;
    if (String_create(&s)) {
        fmt_errors(parser, &s);
        _printf_e("%s", s.buffer);
        parser->first_error = NULL;
        parser->last_error = NULL;
        String_free(&s);
    }
}

int main() {
    Log_Init();

    Parser parser;
    if (!Parser_create(&parser)) {
        _printf_e("Failed creating parser\n");
        return 1;
    }

    String s;
    if (!read_text_file(&s, oL("src\\compiler\\program.txt"))) {
        _printf_e("Failed to read program.txt\n");
        return 1;
    }

    parser.indata = s.buffer;
    parser.input_size = s.length;

    struct Tokenizer t;
    t.parser = &parser;
    uint64_t start, end;
    scanner_consume_token(&t, &start, &end);

    scanner_parse(&t);

    if (parser.first_error != NULL) {
        dump_errors(&parser);
    }

    Log_Shutdown();
}
