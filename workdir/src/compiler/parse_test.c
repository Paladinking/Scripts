#include "parser.h"
#include "glob.h"
#include "printf.h"
#include "format.h"
#include "parse.h"

struct Tokenizer {
    Parser* parser;
    Token last_token;
    uint64_t start;
    uint64_t end;
};

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
        _printf_e("%s", s.buffer);
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
    t.start = 0;
    t.end = 0;
    uint64_t start, end;
    consume_token(&t, &start, &end);

    parse(&t);

    if (parser.first_error != NULL) {
        dump_errors(&parser);
    }

    Log_Shutdown();
}
