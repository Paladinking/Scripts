#include "parser.h"
#include <printf.h>
#include <dynamic_string.h>
#include <mem.h>


static hash_id hash(const uint8_t* str, uint32_t len) {
    uint64_t hash = 5381;
    int c;
    for (uint32_t ix = 0; ix < len; ++ix) {
        c = str[ix];
        hash = ((hash << 5) + hash) + c;
    }
    return hash % NAME_TABLE_HASH_ENTRIES;
}


void find_row_col(Parser* parser, uint64_t pos, uint64_t* row, uint64_t* col) {
    uint64_t ix = 0;
    *row = 0;
    *col = 0;
    if (pos > parser->input_size) {
        pos = parser->input_size;
        return;
    }
    while (ix < pos) {
        uint8_t c = parser->indata[ix];
        if (c == '\n') {
            if (ix == 0 || parser->indata[ix - 1] != '\r') {
                *col = 0;
                *row += 1;
            }
        } else if (c == '\r') {
            *col = 0;
            *row += 1;
        } else {
            *col += 1;
        }
        ++ix;
    }
}

void fatal_error_cb(Parser* parser, enum ParseErrorKind error, LineInfo info,
                    const char* file, uint64_t line) {
    uint64_t row, col;
    if (info.real) {
        find_row_col(parser, info.start, &row, &col);
    } else {
        row = 0;
        col = 0;
    }
    switch (error) {
        case PARSE_ERROR_NONE:
            _wprintf_e(L"Fatal error: UNKOWN\n");
            break;
        case PARSE_ERROR_OUTOFMEMORY:
            _wprintf_e(L"Fatal error: out of memory\n");
            break;
        case PARSE_ERROR_EOF:
            _wprintf_e(L"Fatal error: unexpecetd end of file at row %llu, collum %llu\n", row, col);
            break;
        case PARSE_ERROR_INTERNAL:
            _wprintf_e(L"Fatal error: internal error at %S line %llu\n", file, line);
            break;
    }
    ExitProcess(1);
}

#define fatal_error(p, e, l) fatal_error_cb(p, e, l, __FILE__, __LINE__)

void error_cb(Parser* parser, enum ParseErrorKind error, LineInfo line,
        const char* file, uint64_t iline) {
    ParseError* e = Arena_alloc_type(&parser->arena, ParseError);
    e->pos = line;
    e->kind = error;
    e->next = NULL;
    e->file = file;
    e->internal_line = iline;
    if (parser->first_error == NULL) {
        parser->first_error = e;
        parser->last_error = e;
    } else {
        parser->last_error->next = e;
        parser->last_error = e;
    }
}

#define add_error(p, e, l) error_cb(p, e, l, __FILE__, __LINE__)

void parser_out_of_memory(void* ctx) {
    Parser* parser = ctx;
    LineInfo i;
    i.real = false;
    fatal_error(parser, PARSE_ERROR_OUTOFMEMORY, i);
}

LineInfo current_pos(Parser* parser) {
    LineInfo i = {true, parser->pos, parser->pos};
    return i;
}

name_id insert_name(Parser* parser, const uint8_t* name, uint32_t len, type_id type, enum NameKind kind) {
    hash_id h = hash(name, len);
    NameTable* names = &parser->name_table;

    name_id id = names->hash_map[h];
    name_id n_id = id;

    while (n_id != NAME_ID_INVALID) {
        if (names->data[n_id].name_len == len &&
            memcmp(names->data[n_id].name, name, len) == 0) {
            if (n_id > parser->scope_stack[parser->scope_count - 1] ||
                n_id <= parser->scope_stack[0]) {
                // Name already taken in this scope, or in builtin scope
                return NAME_ID_INVALID;
            }
        }
        n_id = parser->name_table.data[n_id].parent;
    }
    n_id = parser->name_table.size;
    uint8_t* name_ptr = Arena_alloc(&parser->arena, len, 1);
    memcpy(name_ptr, name, len);
    if (parser->name_table.size == parser->name_table.capacity) {
        size_t new_cap = parser->name_table.capacity * 2;
        NameData* new_nd = Mem_realloc(parser->name_table.data,
                                       new_cap * sizeof(NameData));
        if (new_nd == NULL) {
            parser_out_of_memory(parser);
            return NAME_ID_INVALID;
        }
        parser->name_table.data = new_nd;
        parser->name_table.capacity = new_cap;
    }
    parser->name_table.size += 1;
    parser->name_table.data[n_id].kind = kind;
    parser->name_table.data[n_id].parent = id;
    parser->name_table.data[n_id].hash_link = h;
    parser->name_table.data[n_id].name = name_ptr;
    parser->name_table.data[n_id].name_len = len;
    parser->name_table.data[n_id].type = type;

    parser->name_table.hash_map[h] = n_id;
    return n_id;
}

// Return NAME_ID_INVALID and does nothing if name already exists in current scope
name_id insert_type_name(Parser* parser, const uint8_t* name, uint32_t name_len,
                         type_id type, TypeDef* def) {
    name_id id = insert_name(parser, name, name_len, type, NAME_TYPE);
    if (id != NAME_ID_INVALID) {
        parser->name_table.data[id].type_def = def;
    }
    return id;
}

void insert_keyword(Parser* parser, const char* keyword, name_id id) {
    name_id n = insert_name(parser, (const uint8_t*)keyword, strlen(keyword),
                            TYPE_ID_INVALID, NAME_KEYWORD);
    if (n != id) {
        fatal_error(parser, PARSE_ERROR_INTERNAL, current_pos(parser));
    }
}

// Allocate new type id. Does not initialize
type_id create_type_id(Parser* parser) {
    if (parser->type_table.capacity == parser->type_table.size) {
        TypeData* d = Mem_realloc(parser->type_table.data,
                parser->type_table.capacity * 2 * sizeof(TypeData));
        if (d == NULL) {
            parser_out_of_memory(parser);
        }
        parser->type_table.data = d;
        parser->type_table.capacity *= 2;
    }
    parser->type_table.size += 1;
    return parser->type_table.size - 1;
}

type_id array_of(Parser* parser, type_id id) {
    if (parser->type_table.data[id].array_type == TYPE_ID_INVALID) {
        type_id t = create_type_id(parser);
        parser->type_table.data[t].kind = TYPE_ARRAY;
        parser->type_table.data[t].type_def = parser->type_table.data[id].type_def;
        parser->type_table.data[t].parent = id;
        parser->type_table.data[t].array_type = TYPE_ID_INVALID;
        parser->type_table.data[id].array_type = t;
    }
    return parser->type_table.data[id].array_type;
}

// Also creates unique function type
name_id insert_function_name(Parser* parser, const uint8_t* name, uint32_t name_len,
                             FunctionDef* def) {
    name_id id = insert_name(parser, name, name_len, TYPE_ID_INVALID, NAME_FUNCTION);
    if (id == NAME_ID_INVALID) {
        return id;
    }
    type_id t = create_type_id(parser);
    TypeData* d = &parser->type_table.data[t];
    d->parent = TYPE_ID_INVALID;
    d->kind = TYPE_NORMAL;
    d->array_type = TYPE_ID_INVALID;
    d->type_def = Arena_alloc_type(&parser->arena, TypeDef);
    d->type_def->kind = TYPEDEF_FUNCTION;
    d->type_def->func = def;

    parser->name_table.data[id].type = t;
    parser->name_table.data[id].func_def = def;
    return id;
}

name_id find_name(Parser* parser, const uint8_t* name, uint32_t len) {
    hash_id h = hash(name, len);
    name_id id = parser->name_table.hash_map[h];
    while (id != NAME_ID_INVALID) {
        if (parser->name_table.data[id].name_len == len &&
            memcmp(parser->name_table.data[id].name, name, len) == 0) {
            return id;
        }
        id = parser->name_table.data[id].parent;
    }
    return id;
}

name_id insert_variable_name(Parser* parser, const uint8_t* name, uint32_t name_len, type_id t, uint64_t pos) {
    name_id id = insert_name(parser, name, name_len,
                             t, NAME_VARIABLE);
    if (id == NAME_ID_INVALID) {
        name_id prev = find_name(parser, name, name_len);
        LineInfo l = {true, pos, pos};
        if (prev == NAME_ID_INVALID) {
            fatal_error(parser, PARSE_ERROR_INTERNAL, l);
        } else if (parser->name_table.data[prev].kind == NAME_KEYWORD) {
            add_error(parser, PARSE_ERROR_RESERVED_NAME, l);
        } else {
            add_error(parser, PARSE_ERROR_REDEFINITION, l);
        }
        return NAME_ID_INVALID;
    }
    return id;
}

void begin_scope(Parser* parser) {
    if (parser->scope_count == parser->scope_capacity) {
        uint64_t c = parser->scope_capacity * 2;
        name_id* n = Mem_realloc(parser->scope_stack, c * sizeof(name_id));
        if (n == NULL) {
            parser_out_of_memory(parser);
            return;
        }
        parser->scope_stack = n;
        parser->scope_capacity = c;
    }
    parser->scope_stack[parser->scope_count] = parser->name_table.size - 1;
    parser->scope_count += 1;
}

void end_scope(Parser* parser) {
    parser->scope_count -= 1;
    name_id id = parser->name_table.size - 1;
    while (id > parser->scope_stack[parser->scope_count]) {
        hash_id link = parser->name_table.data[id].hash_link;
        name_id parent = parser->name_table.data[id].parent;
        parser->name_table.hash_map[link] = parent;
        --id;
    }
}

void parser_add_builtin(Parser* parser, type_id id, enum TypeDefKind kind,
                        const char* name) {
    TypeDef* def = Arena_alloc_type(&parser->arena, TypeDef);
    def->kind = kind;
    parser->type_table.data[id].kind = TYPE_NORMAL;
    parser->type_table.data[id].array_type = TYPE_ID_INVALID;
    parser->type_table.data[id].parent = TYPE_ID_INVALID;
    parser->type_table.data[id].type_def = def;

    insert_type_name(parser, (const uint8_t*)name, strlen(name), id, def);
}

static inline bool eof(Parser* parser) {
    return parser->pos >= parser->input_size;
}

void skip_spaces(Parser* parser) {
    do {
        if (eof(parser)) {
            return;
        }
        uint8_t c = parser->indata[parser->pos];
        if (c != ' ' && c != '\t' && c != '\n' && c != '\r' &&
            c != '\f' && c != '\v') {
            return;
        }
        ++parser->pos;
    } while (1);
}

uint8_t expect_data(Parser* parser) {
    if (eof(parser)) {
        fatal_error(parser, PARSE_ERROR_EOF, current_pos(parser));
    }
    return parser->indata[parser->pos];
}


bool parse_uint(Parser* parser, uint64_t* i, uint8_t base) {
    *i = 0;
    uint64_t n = 0;
    expect_data(parser);
    uint8_t c = parser->indata[parser->pos];

    if (base == 16) { // Hex
        if ((c < '0' || c > '9') && (c < 'a' || c > 'f') &&
            (c < 'A' && c > 'F')) {
            return false;
        }
        do {
            c = parser->indata[parser->pos];
            if (n > 0xfffffffffffffff) {
                return false; // Overflow
            }
            n = n << 4;
            if (c >= '0' && c <= '9') {
                n += c - '0';
            } else if (c >= 'a' && c <= 'f') {
                n += (c - 'a') + 10;
            } else if (c >= 'A' && c <= 'F') {
                n += (c - 'A') + 10;
            } else {
                break;
            }
            parser->pos += 1;
        } while (parser->pos < parser->input_size);
    } else if (base == 2) { // Binary
        if (c != '0' && c != '1') {
            return false;
        }
        do {
            c = parser->indata[parser->pos];
            if (n > 0x7fffffffffffffff) {
                return false; // Overflow
            }
            n = n << 1;
            if (c == '0' || c == '1') {
                n += c - '0';
            } else {
                break;
            }
            parser->pos += 1;
        } while (parser->pos < parser->input_size);
    } else if (base == 8) { // Octal
        if (c < '0' || c > '7') {
            return false;
        }
        do {
            c = parser->indata[parser->pos];
            if (n > 0x1fffffffffffffff) {
                return false; // Overflow
            }
            n = n << 3;
            if (c >= '0' && c <= '7') {
                n += c - '0';
            } else {
                break;
            }
            parser->pos += 1;
        } while (parser->pos < parser->input_size);
    } else { // Base 10
        if (c < '0' || c > '9') {
            return false;
        }
        do {
            c = parser->indata[parser->pos];
            if (c < '0' || c > '9') {
                break;
            }
            uint64_t v = c - '0';
            if (n > 0x1999999999999999) {
                return false;
            } else if (n == 0x1999999999999999) {
                if (v > 5) {
                    return false;
                }
            }
            n = n * 10;
            n += v;
            parser->pos += 1;
        } while (parser->pos < parser->input_size);
    }
    *i = n;
    return true;
}

static inline bool is_identifier(uint8_t c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
             c == '_' || (c >= '0' && c <= '9');
}

static inline bool is_identifier_start(uint8_t c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c == '_');
}

const uint8_t* parse_name(Parser* parser, uint32_t* len) {
    uint64_t name_len = 0;
    const uint8_t* base = parser->indata + parser->pos;

    if (!is_identifier_start(expect_data(parser))) {
        add_error(parser, PARSE_ERROR_INVALID_CHAR, current_pos(parser));
        return NULL;
    }
    expect_data(parser);

    while (parser->pos < parser->input_size) {
        uint8_t c = parser->indata[parser->pos];
        if (is_identifier(c)) {
            name_len += 1;
            parser->pos += 1;
            continue;
        }
        break;
    }
    if (name_len > 0xffff) {
        LineInfo l = {true, base - parser->indata, parser->pos};
        add_error(parser, PARSE_ERROR_BAD_NAME, l);
        return NULL;
    }
    *len = name_len;
    return base;
}


name_id parse_known_name(Parser* parser) {
    uint32_t len = 0;
    const uint8_t* base = parse_name(parser, &len);
    if (base == NULL) {
        return NAME_ID_INVALID;
    }
    name_id name = find_name(parser, base, len);
    if (name == NAME_ID_INVALID) {
        LineInfo l = {true, base - parser->indata, parser->pos};
        add_error(parser, PARSE_ERROR_BAD_NAME, l);
        return NAME_ID_INVALID;
    }
    return name;
}

// Also accepts function names
name_id parse_variable_name(Parser* parser) {
    uint32_t len = 0;
    const uint8_t* base = parse_name(parser, &len);
    if (base == NULL) {
        return NAME_ID_INVALID;
    }
    name_id name = find_name(parser, base, len);
    if (name == NAME_ID_INVALID ||
        (parser->name_table.data[name].kind != NAME_VARIABLE &&
         parser->name_table.data[name].kind != NAME_FUNCTION)) {
        LineInfo l = {true, base - parser->indata, parser->pos};
        add_error(parser, PARSE_ERROR_BAD_NAME, l);
        return NAME_ID_INVALID;
    }
    return name;
}

uint8_t* parse_string_literal(Parser* parser, uint64_t* len) {
    // Skip leading '"'
    parser->pos += 1;
    uint64_t pos = parser->pos;
    *len = 0;
    bool has_err = false;
    while (1) {
        uint8_t c = expect_data(parser);
        parser->pos += 1;
        if (c == '"') {
            break;
        } if (c == '\\') {
            c = expect_data(parser);
            parser->pos += 1;
            if (c == '\\' || c == 'n' || c == 'r' || c == 'v' || c == 'f' ||
                c == 't' || c == '0' || c == '"' || c == '\'') {
                *len += 1;
                continue;
            } else if (c == 'x') {
                c = expect_data(parser);
                parser->pos += 1;
                if ((c < '0' || c > '9') && (c < 'a' || c > 'z') && 
                    (c < 'A' || c > 'Z')) {
                    add_error(parser, PARSE_ERROR_INVALID_ESCAPE, current_pos(parser));
                    has_err = true;
                    parser->pos -= 1;
                } else {
                    *len += 1;
                    c = expect_data(parser);
                    if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || 
                        (c >= 'A' && c <= 'F')) {
                        parser->pos += 1;
                    }
                    continue;
                }
            } else {
                add_error(parser, PARSE_ERROR_INVALID_ESCAPE, current_pos(parser));
                has_err = true;
                parser->pos -= 1;
            }
        }
        *len += 1;
    }
    if (has_err) {
        *len = 0;
        return Arena_alloc(&parser->arena, 0, 1);
    }
    uint8_t* data = Arena_alloc(&parser->arena, *len, 1);
    parser->pos = pos;

    uint64_t ix = 0;
    while (1) {
        uint8_t c = expect_data(parser);
        parser->pos += 1;
        if (c == '"') {
            return data;
        }
        if (c == '\\') {
            uint8_t c = expect_data(parser);
            parser->pos += 1;
            if (c == 'n') {
                data[ix++] = '\n';
            } else if (c == 't') {
                data[ix++] = '\t';
            } else if (c == '\\') {
                data[ix++] = '\\';
            } else if (c == '\'') {
                data[ix++] = '\'';
            } else if (c == '"') {
                data[ix++] = '"';
            } else if (c == 'v') {
                data[ix++] = '\v';
            } else if (c == 'f') {
                data[ix++] = '\f';
            } else if (c == 'r') {
                data[ix++] = '\r';
            } else if (c == '0') {
                data[ix++] = '\0';
            } else { // \x
                uint8_t c = expect_data(parser);
                parser->pos += 1;
                uint8_t val;
                if (c >= '0' && c <= '9') {
                    val = c - '0';
                } else if (c >= 'a' && c <= 'f') {
                    val = c - 'a' + 10;
                } else {
                    val = c - 'A' + 10;
                }
                c = expect_data(parser);
                if (c >= '0' && c <= '9') {
                    val = (val << 4) + c - '0';
                    parser->pos += 1;
                } else if (c >= 'a' && c <= 'f') {
                    val = (val << 4) + (c - 'a') + 10;
                    parser->pos += 1;
                } else if (c >= 'A' && c <= 'F') {
                    val = (val << 4) + (c - 'A') + 10;
                    parser->pos += 1;
                }
                data[ix++] = val;
            }
        } else {
            data[ix++] = c;
        }
    }
}

bool parse_number_literal(Parser* parser, uint64_t* i, double* d, bool* is_int) {
    uint8_t c = expect_data(parser); 
    if ((c < '0' || c > '9') && c != '.') {
        return false;
    }
    uint8_t base = 10;
    if (c == '0' && parser->pos + 1 < parser->input_size) {
        uint8_t p = parser->indata[parser->pos + 1];
        if (p == 'x' || p == 'X') {
            base = 16;
            parser->pos += 2;
        } else if (p == 'b' || p == 'B') {
            base = 2;
            parser->pos += 2;
        } else if (p == 'o' || p == 'O') {
            base = 8;
            parser->pos += 2;
        }
        c = expect_data(parser);
    }
    *is_int = true;

    if (base != 10) {
        return parse_uint(parser, i, base);
    }
    for (uint64_t ix = parser->pos; ix < parser->input_size; ++ix) {
        if (parser->indata[ix] >= '0' && parser->indata[ix] <= '9') {
            continue;
        }
        if (parser->indata[ix] == 'e' || parser->indata[ix] == 'E' ||
            parser->indata[ix] == '.') {
            *is_int = false;
        }
        break;
    }
    if (*is_int) {
        return parse_uint(parser, i, 10);
    }

    if (c != '.' && (c < '0' || c > '9')) {
        return false;
    }
    if (c == '.') {
        *d = 0.0;
    } else {
        *d = (c - '0');
        parser->pos += 1;
        if (eof(parser)) {
            return true;
        }
    }

    if (c != '0' && c != '.') {
        c = parser->indata[parser->pos];
        while (c >= '0' && c <= '9') {
            *d *= 10;
            *d += c - '0';
            parser->pos += 1;
            if (eof(parser)) {
                return true;
            }
            c = parser->indata[parser->pos];
        }
    } else if (c == '0'){
        c = parser->indata[parser->pos];
        if (c >= '0' || c <= '9') {
            return false;
        }
    }
    if (c == '.') {
        parser->pos += 1;
        c = expect_data(parser);
        if (c < '0' || c > '9') {
            return false;
        }
        double factor = 0.1;
        while (c >= '0' && c <= '9') {
            *d += (c - '0') * factor;
            factor *= 0.1;
            parser->pos += 1;
            if (eof(parser)) {
                return true;
            }
            c = parser->indata[parser->pos];
        }
    }
    if (c == 'e' || c == 'E') {
        parser->pos += 1;
        c = expect_data(parser);
        int64_t exp = 0;
        bool neg_exp = false;
        if (c == '-') {
            neg_exp = true;
            parser->pos += 1;
            c = expect_data(parser);
        } else if (c == '+') {
            parser->pos += 1;
            c = expect_data(parser);
        }
        if (c < '0' || c > '9') {
            return false;
        }
        while (c >= '0' && c <= '9') {
            exp = exp * 10;
            exp += c - '0';
            parser->pos += 1;
            if (eof(parser)) {
                break;
            }
            c = parser->indata[parser->pos];
        }
        double factor = 1.0;
        while (exp > 15) {
            exp -= 15;
            // 10^15 is the largest power of 10 that can be fully represented by a double
            factor *= 1000000000000000;
        }
        int64_t exps[15] = {1, 10, 100, 1000, 10000, 100000, 1000000, 10000000, 100000000, 
                            1000000000, 10000000000, 100000000000, 1000000000000, 10000000000000,
                            100000000000000};
        factor *= exps[exp];
        if (neg_exp) {
            *d /= factor;
        } else {
            *d *= factor;
        }
    }
    return true;
}

// counts the number of arguments to a call
uint64_t count_args(Parser* parser) {
    uint64_t ix = parser->pos;
    uint64_t count = 1;
    uint64_t open_par = 0;
    uint64_t open_bra = 0;
    uint64_t open_cur = 0;
    uint8_t c1 = expect_data(parser);
    if (c1 == ')' || c1 == '}' || c1 == ']') {
        return 0;
    }
    for (uint64_t ix = parser->pos; ix < parser->input_size; ++ix) {
        uint8_t c = parser->indata[ix];
        if (c == '(') {
            ++open_par;
        } else if (c == ')') {
            if (open_par == 0) {
                return count;
            }
            --open_par;
            continue;
        } else if (c == '[') {
            ++open_bra;
        } else if (c == ']') {
            if (open_bra == 0) {
                return count;
            }
            --open_bra;
        } else if (c == '{') {
            ++open_cur;
        } else if (c == '}') {
            if (open_cur == 0) {
                return count;
            }
            --open_cur;
        } else if (c == '\'' || c == '"') {
            ++ix;
            for (; ix < parser->input_size; ++ix) {
                if (parser->indata[ix] == '\\') {
                    ++ix;
                } else if (parser->indata[ix] == c) {
                    break;
                }
            }
        } else if (c == ',' && open_par == 0 && open_bra == 0 && open_cur == 0) {
            ++count;
        }
    }
    return count;
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

// Handle postfix operation. True if push to parse stack
// NOTE: modifies e. e might be modified even if returns false
bool postfix_operation(Expression* e, Expression** top, Parser* parser, LineInfo i) {
start:
    skip_spaces(parser);
    if (eof(parser)) {
        return false;
    }
    enum BinaryOperator op;
    uint8_t c = parser->indata[parser->pos];
    if (c == '[') {
        Expression* n = Arena_alloc_type(&parser->arena, Expression);
        n->type = EXPRESSION_ARRAY_INDEX;
        n->array_index.array = e;
        n->array_index.index = NULL;
        n->line = i;
        parser->pos += 1;
        *top = n;
        return true;
    } else if (c == '(') {
        Expression* n = Arena_alloc_type(&parser->arena, Expression);
        skip_spaces(parser);
        parser->pos += 1;
        uint64_t count = count_args(parser);
        if (count == 0) {
            *n = *e;
            e->type = EXPRESSION_CALL;
            e->call.function = n;
            e->call.arg_count = 0;
            e->call.args = NULL;
            e->line = i;
            parser->pos += 1;
            e->line.end = parser->pos;
            goto start;
        }
        n->type = EXPRESSION_CALL;
        n->call.function = e;
        n->call.arg_count = 0;
        n->line = i;
        n->call.args = Arena_alloc_count(&parser->arena, Expression*, count);
        // DEBUG: Save capacity
        n->call.arg_count = (count << 32);
        *top = n;
        return true;
    } else if (c == '+') {
        op = BINOP_ADD;
    } else if (c == '-') {
        op = BINOP_SUB;
    } else if (c == '*') {
        op = BINOP_MUL;
    } else if (c == '%') {
        op = BINOP_MOD;
    } else if (c == '/') {
        op = BINOP_DIV;
    } else if (c == '|') {
        if (parser->pos < parser->input_size - 1 &&
            parser->indata[parser->pos + 1] == '|') {
            parser->pos += 1;
            op = BINOP_BOOL_OR;
        } else {
            op = BINOP_BIT_OR;
        }
    } else if (c == '&') {
        if (parser->pos < parser->input_size - 1 &&
            parser->indata[parser->pos + 1] == '&') {
            parser->pos += 1;
            op = BINOP_BOOL_AND;
        } else {
            op = BINOP_BIT_AND;
        }
    } else if (c == '^') {
        op = BINOP_BIT_XOR;
    } else if (c == '<') {
        if (parser->pos < parser->input_size - 1 &&
            parser->indata[parser->pos + 1] == '<') {
            parser->pos += 1;
            op = BINOP_BIT_LSHIFT;
        } else if (parser->pos < parser->input_size - 1 &&
                   parser->indata[parser->pos + 1] == '=') { 
            parser->pos += 1;
            op = BINOP_CMP_LE;
        } else {
            op = BINOP_CMP_L;
        }
    } else if (c == '>') {
        if (parser->pos < parser->input_size - 1 &&
            parser->indata[parser->pos + 1] == '>') {
            parser->pos += 1;
            op = BINOP_BIT_RSHIFT;
        } else if (parser->pos < parser->input_size - 1 &&
                   parser->indata[parser->pos + 1] == '=') { 
            parser->pos += 1;
            op = BINOP_CMP_GE;
        } else {
            op = BINOP_CMP_G;
        }
    } else if (c == '=') {
        if (parser->pos < parser->input_size - 1 &&
            parser->indata[parser->pos + 1] == '=') {
            parser->pos += 1;
            op = BINOP_CMP_EQ;
        } else {
            return false;
        }
    } else if (c == '!') {
        if (parser->pos < parser->input_size - 1 &&
            parser->indata[parser->pos + 1] == '=') {
            parser->pos += 1;
            op = BINOP_CMP_NEQ;
        } else {
            return false;
        }
    } else {
        return false;
    }
    Expression* n = Arena_alloc_type(&parser->arena, Expression);
    n->type = EXPRESSION_BINOP;
    n->binop.op = op;
    n->binop.lhs = e;
    n->binop.rhs = NULL;
    n->line = i;
    parser->pos += 1;

    *top = n;
    return true;
}

bool prefix_operation(Expression* e, Parser* parser, LineInfo i) {
    if (eof(parser)) {
        return false;
    }
    enum UnaryOperator op;
    uint8_t c = parser->indata[parser->pos];
    if (c == '(') {
        op = UNOP_PAREN;
    } else if (c == '+') {
        op = UNOP_POSITIVE;
    } else if (c == '-') {
        op = UNOP_NEGATVIE;
    } else if (c == '!') {
        op = UNOP_BOOLNOT;
    } else if (c == '~') {
        op = UNOP_BITNOT;
    } else {
        return false;
    }
    e->type = EXPRESSION_UNOP;
    e->unop.op = op;
    e->unop.expr = NULL;
    e->line = i;
    parser->pos += 1;
    return e;
}

// pos will be first char not part of expression after
Expression* parse_expression(Parser* parser) {
    struct Node {
        Expression* e;
    };

    uint64_t stack_size = 0;
    uint64_t stack_cap = 8;
    struct Node* stack = Mem_alloc(8 * sizeof(struct Node));
    if (stack == NULL) {
        parser_out_of_memory(parser);
        return NULL;
    }

    while (1) {
loop:
        if (stack_size == stack_cap) {
            stack_cap *= 2;
            struct Node* n = Mem_realloc(stack,
                    stack_cap * sizeof(struct Node));
            if (n == NULL) {
                parser_out_of_memory(parser);
                Mem_free(stack);
                return NULL;
            }
            stack = n;
        }
        skip_spaces(parser);
        expect_data(parser);
        uint8_t c = parser->indata[parser->pos];
        LineInfo i = {true, parser->pos, 0};
        Expression* e = Arena_alloc_type(&parser->arena, Expression);
        if (prefix_operation(e, parser, i)) {
            stack[stack_size].e = e;
            ++stack_size;
            continue;
        } else if ((c >= '0' && c <= '9') || c == '.') {
            uint64_t u;
            double d;
            bool is_int;
            e->line = i;
            if (!parse_number_literal(parser, &u, &d, &is_int)) {
                add_error(parser, PARSE_ERROR_INVALID_LITERAL, current_pos(parser));
                e->type = EXPRESSION_INVALID;
                Mem_free(stack);
                return e;
            } else if (is_int) {
                e->type = EXPRESSION_LITERAL_UINT;
                e->literal.uint = u;
            } else {
                e->type = EXPRESSION_LITERAL_FLOAT;
                e->literal.float64 = d;
            }
            e->line.end = parser->pos;
        } else if (c == '"') {
            uint64_t len;
            uint8_t* s = parse_string_literal(parser, &len);
            e->type = EXPRESSION_LITERAL_STRING;
            e->line = i;
            e->line.end = parser->pos;
            e->literal.string.bytes = s;
            e->literal.string.len = len;
        } else {
            name_id name = parse_variable_name(parser);
            if (name == NAME_ID_INVALID) {
                e->type = EXPRESSION_INVALID;
                Mem_free(stack);
                return e;
            } else {
                e->type = EXPRESSION_VARIABLE;
                e->line = i;
                e->line.end = parser->pos;
                e->variable.ix = name;
            }
        }

        Expression* to_push;
        if (postfix_operation(e, &to_push, parser, i)) {
            stack[stack_size].e = to_push;
            ++stack_size;
            continue;
        }

        while (stack_size > 0) {
            skip_spaces(parser);
            Expression* n = stack[stack_size - 1].e;
            --stack_size;
            bool check_postfix = false;
            if (n->type == EXPRESSION_UNOP) {
                n->unop.expr = e;
                n->line.end = parser->pos;
                if (n->unop.op == UNOP_PAREN) {
                    uint8_t c = expect_data(parser);
                    if (c != ')') {
                        add_error(parser, PARSE_ERROR_INVALID_CHAR, current_pos(parser));
                        e->type = EXPRESSION_INVALID;
                        Mem_free(stack);
                        return e;
                    }
                    parser->pos += 1;
                    n->line.end += 1;
                    check_postfix =  true;
                } else if (n->unop.expr->type == EXPRESSION_BINOP) {
                    Expression* node = n;
                    Expression* bin = node->unop.expr;
                    n = node->unop.expr;
                    node->unop.expr = bin->binop.lhs;
                    bin->binop.lhs = node;
                    node->line.end = node->unop.expr->line.end;
                    bin->line.start = bin->binop.lhs->line.start;
                    Expression* parent = n;
                    while (node->unop.expr->type == EXPRESSION_BINOP) {
                        bin = node->unop.expr;
                        node->unop.expr = bin->binop.lhs;
                        bin->binop.lhs = node;
                        node->line.end = node->unop.expr->line.end;
                        bin->line.start = bin->binop.lhs->line.start;
                        parent->binop.lhs = bin;
                        parent = bin;
                    }
                }
            } else if (n->type == EXPRESSION_BINOP) {
                n->binop.rhs = e;
                n->line.end = parser->pos;
                n->line.end = parser->pos;
                Expression* node = n;
                uint32_t prio = binop_precedence(n->binop.op);

                if (e->type == EXPRESSION_BINOP &&
                    prio < binop_precedence(e->binop.op)) {
                    Expression* lhs = e->binop.lhs;
                    e->line.start = node->line.start;
                    e->binop.lhs = node;
                    node->binop.rhs = lhs;
                    node->line.end = node->binop.rhs->line.end;
                    n = e;

                    Expression* parent = n;
                    while (node->binop.rhs->type == EXPRESSION_BINOP &&
                           prio < binop_precedence(node->binop.rhs->binop.op)) {
                        Expression* lhs = node->binop.rhs->binop.lhs;
                        node->binop.rhs->binop.lhs = node;
                        parent->binop.lhs = node->binop.rhs;
                        parent = node->binop.rhs;
                        node->binop.rhs->line.start = node->line.start;
                        node->binop.rhs = lhs;
                        node->line.end = node->binop.rhs->line.end;
                    }
                } 
            } else if (n->type == EXPRESSION_ARRAY_INDEX) {
                uint8_t c = expect_data(parser);
                if (c != ']') {
                    add_error(parser, PARSE_ERROR_INVALID_CHAR, current_pos(parser));
                    e->type = EXPRESSION_INVALID;
                    Mem_free(stack);
                    return e;
                }
                parser->pos += 1;
                check_postfix =  true;
                n->line.end = parser->pos;
                n->array_index.index = e;
            } else if (n->type == EXPRESSION_CALL) {
                uint8_t c = expect_data(parser);
                uint64_t count = n->call.arg_count & 0xffffffff;
                uint64_t max_count = n->call.arg_count >> 32;
                if (count == max_count) {
                    fatal_error(parser, PARSE_ERROR_INTERNAL, current_pos(parser));
                }
                n->call.args[count] = e;
                n->call.arg_count = (count + 1) | (max_count << 32);
                parser->pos += 1;
                if (c == ',') {
                    ++stack_size;
                    goto loop;
                } else if (c != ')') {
                    add_error(parser, PARSE_ERROR_INVALID_CHAR, current_pos(parser));
                    e->type = EXPRESSION_INVALID;
                    Mem_free(stack);
                    return e;
                }
                n->call.arg_count = n->call.arg_count & 0xffffffff;
                n->line.end = parser->pos;
                check_postfix =  true;
            } else {
                fatal_error(parser, PARSE_ERROR_INTERNAL, current_pos(parser));
            }
            e = n;
            if (check_postfix) {
                if (postfix_operation(e, &to_push, parser, i)) {
                    stack[stack_size].e = to_push;
                    ++stack_size;
                    goto loop;
                }
            }
        }
        Mem_free(stack);
        return e;
    }
}

// Skip to after end of current statement
void skip_statement(Parser* parser) {
    uint64_t open_cur = 0;
    while (1) {
        uint8_t c = expect_data(parser);
        parser->pos += 1;
        if (c == ';' && open_cur == 0) {
            return;
        }
        if (c == '{') {
            open_cur += 1;
        } else if (c == '}') {
            if (open_cur <= 1) {
                return;
            }
            --open_cur;
        } else if (c == '"' || c == '\'') {
            while (1) {
                uint8_t c2 = expect_data(parser);
                parser->pos += 1;
                if (c2 == '\\') {
                    parser->pos += 1;
                } else if (c2 == c) {
                    break;
                }
            }
        }
    }
}

Statement** parse_statements(Parser* parser, uint64_t* count) {

    struct Node {
        Statement* statement;
    };

    uint64_t stack_size = 0;
    uint64_t stack_cap = 16;
    struct Node* stack = Mem_alloc(16 * sizeof(struct Node));
    if (stack == NULL) {
        fatal_error(parser, PARSE_ERROR_OUTOFMEMORY, current_pos(parser));
    }

    uint64_t top_count = 0;
    begin_scope(parser);

    while (1) {
        skip_spaces(parser);
        uint8_t c = expect_data(parser);
        if (stack_size == stack_cap) {
            struct Node* n = Mem_realloc(stack, stack_cap * 2 *
                                         sizeof(struct Node));
            if (n == NULL) {
                fatal_error(parser, PARSE_ERROR_OUTOFMEMORY,
                            current_pos(parser));
            }
            stack_cap *= 2;
            stack = n;
        }
        if (c == '}') {
            Statement** list = Arena_alloc_count(&parser->arena, Statement*,
                                                 top_count);
            for (uint64_t i = 0; i < top_count; ++i) {
                list[i] = stack[stack_size - top_count + i].statement;
            }
            stack_size -= top_count;
            parser->pos += 1;
            end_scope(parser);
            if (stack_size > 0) {
                uint64_t last_top_count = 0;
                Statement* top = stack[stack_size - 1].statement;
                if (top->type == STATEMENT_IF) {
                    top->if_.statements = list;
                    last_top_count = top->if_.statement_count;
                    top->if_.statement_count = top_count;
                } else if (top->type == STATEMENT_WHILE) {
                    top->while_.statements = list;
                    last_top_count = top->while_.statement_count;
                    top->while_.statement_count = top_count;
                } else {
                    fatal_error(parser, PARSE_ERROR_INTERNAL, current_pos(parser));
                }
                top->line.end = parser->pos;
                top_count = last_top_count;
            } else {
                *count = top_count;
                Mem_free(stack);
                return list;
            }
            continue;
        }

        if (is_identifier_start(c)) {
            uint64_t pos = parser->pos;
            name_id name = parse_known_name(parser);
            if (name == NAME_ID_INVALID) {
                skip_statement(parser);
                continue;
            }
            NameTable* names = &parser->name_table;
            if (names->data[name].kind == NAME_KEYWORD) {
                Statement* s; 
                skip_spaces(parser);
                if (expect_data(parser) != '(') {
                    add_error(parser, PARSE_ERROR_INVALID_CHAR,
                    current_pos(parser));
                    skip_statement(parser);
                    continue;
                }
                parser->pos += 1;
                Expression* e = parse_expression(parser);
                if (e->type == EXPRESSION_INVALID) {
                    skip_statement(parser);
                    continue;
                }
                skip_spaces(parser);
                if (expect_data(parser) != ')') {
                    add_error(parser, PARSE_ERROR_INVALID_CHAR,
                    current_pos(parser));
                    skip_statement(parser);
                    continue;
                }
                parser->pos += 1;
                skip_spaces(parser);
                if (expect_data(parser) != '{') {
                    add_error(parser, PARSE_ERROR_INVALID_CHAR,
                    current_pos(parser));
                    skip_statement(parser);
                    continue;
                }
                parser->pos += 1;

                if (name == NAME_ID_WHILE) {
                    s = Arena_alloc_type(&parser->arena, Statement);
                    s->type = STATEMENT_WHILE;
                    s->while_.statement_count = top_count + 1;
                    s->while_.condition = e;
                } else if (name == NAME_ID_IF) {
                    s = Arena_alloc_type(&parser->arena, Statement);
                    s->type = STATEMENT_IF;
                    s->if_.statement_count = top_count + 1;
                    s->if_.condition = e;
                } else {
                    parser->pos = pos;
                    add_error(parser, PARSE_ERROR_BAD_NAME, current_pos(parser));
                    skip_statement(parser);
                    continue;
                }
                s->line.start = pos;
                s->line.real = true;
                stack[stack_size].statement = s;
                ++stack_size;
                top_count = 0;
                begin_scope(parser);
                continue;
            } else if (names->data[name].kind == NAME_TYPE) {
                type_id t = names->data[name].type;
                skip_spaces(parser);
                uint32_t len;
                uint8_t c = expect_data(parser);
                ArraySize* array_size = NULL;
                ArraySize* last_array_size = NULL;
                while (c == '[') {
                    parser->pos += 1;
                    uint64_t size;
                    double d;
                    bool is_int;
                    skip_spaces(parser);
                    if (!parse_number_literal(parser, &size, &d, &is_int) ||
                        !is_int) {
                        add_error(parser, PARSE_ERROR_BAD_ARRAY_SIZE, current_pos(parser));
                        skip_statement(parser);
                        continue;
                    }
                    skip_spaces(parser);
                    c = expect_data(parser);
                    if (c != ']') {
                        add_error(parser, PARSE_ERROR_INVALID_CHAR, current_pos(parser));
                        skip_statement(parser);
                        continue;
                    }
                    parser->pos += 1;
                    t = array_of(parser, t);
                    skip_spaces(parser);
                    c = expect_data(parser);
                    if (array_size == NULL) {
                        array_size = Arena_alloc_type(&parser->arena, ArraySize);
                        last_array_size = array_size;
                    } else {
                        last_array_size->next = Arena_alloc_type(&parser->arena, ArraySize);
                        last_array_size = last_array_size->next;
                    }
                    last_array_size->size = size;
                    last_array_size->next = NULL;
                }

                uint64_t name_start = parser->pos;
                const uint8_t* name = parse_name(parser, &len);
                name_id n;
                if (name == NULL || 
                    (n = insert_variable_name(parser, name, len, t, pos)) == NAME_ID_INVALID) {
                    skip_statement(parser);
                    continue;
                }
                parser->name_table.data[n].array_size = array_size;
                uint64_t name_end = parser->pos;

                skip_spaces(parser);
                c = expect_data(parser);
                if (c == '=') {
                    parser->pos += 1;
                    Expression* e = parse_expression(parser);
                    if (e->type == EXPRESSION_INVALID) {
                        skip_statement(parser);
                        continue;
                    }
                    skip_spaces(parser);
                    if (expect_data(parser) != ';') {
                        skip_statement(parser);
                        continue;
                    }
                    parser->pos += 1;
                    Statement* s = Arena_alloc_type(&parser->arena, Statement);
                    s->type = STATEMENT_ASSIGN;
                    Expression* lhs = Arena_alloc_type(&parser->arena, Expression);
                    lhs->type = EXPRESSION_VARIABLE;
                    lhs->variable.ix = n;
                    lhs->line.start = name_start;
                    lhs->line.end = name_end;
                    s->assignment.lhs = lhs;
                    s->assignment.rhs = e;
                    s->line.start = pos;
                    s->line.end = parser->pos;
                    stack[stack_size].statement = s;
                    ++stack_size;
                    ++top_count;
                    continue;
                }
                if (c != ';') {
                    add_error(parser, PARSE_ERROR_INVALID_CHAR, current_pos(parser));
                    skip_statement(parser);
                    continue;
                }
                parser->pos += 1;
                continue;
            } else {
                parser->pos = pos;
            }
        }

        Expression* e = parse_expression(parser);
        if (e->type == EXPRESSION_INVALID) {
            skip_statement(parser);
            continue;
        }
        skip_spaces(parser);
        c = expect_data(parser);
        Statement* s;
        if (c == '=') {
            parser->pos += 1;
            Expression* rhs = parse_expression(parser);
            if (rhs->type == EXPRESSION_INVALID) {
                skip_statement(parser);
                continue;
            }
            skip_spaces(parser);
            if (expect_data(parser) != ';') {
                add_error(parser, PARSE_ERROR_INVALID_CHAR, current_pos(parser));
                continue;
            }
            s = Arena_alloc_type(&parser->arena, Statement);
            s->type = STATEMENT_ASSIGN;
            s->assignment.lhs = e;
            s->assignment.rhs = rhs;
        } else if (expect_data(parser) != ';') {
            add_error(parser, PARSE_ERROR_INVALID_CHAR, current_pos(parser));
            continue;
        } else {
            s = Arena_alloc_type(&parser->arena, Statement);
            s->type = STATEMENT_EXPRESSION;
            s->expression = e;
        }
        parser->pos += 1;
        s->line.start = e->line.start;
        s->line.end = parser->pos;
        stack[stack_size].statement = s;
        ++stack_size;
        ++top_count;
    }
}

bool Parser_create(Parser* parser) {
    if (!Arena_create(&parser->arena, 0x7fffffff, parser_out_of_memory,
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


    TypeData* type_data = Mem_alloc((TYPE_ID_BUILTIN_COUNT + 16) * sizeof(TypeData));
    if (type_data == NULL) {
        Arena_free(&parser->arena);
        Mem_free(function_data);
        return false;
    }

    parser->type_table.capacity = TYPE_ID_BUILTIN_COUNT + 16;
    parser->type_table.size = TYPE_ID_BUILTIN_COUNT;
    parser->type_table.data = type_data;
    NameData* name_data = Mem_alloc(16 * sizeof(NameData));
    if (name_data == NULL) {
        Arena_free(&parser->arena);
        Mem_free(function_data);
        Mem_free(type_data);
        return false;
    }
    parser->name_table.capacity = 16;
    parser->name_table.size = 0;
    parser->name_table.data = name_data;

    for (uint64_t ix = 0; ix < NAME_TABLE_HASH_ENTRIES; ++ix) {
        parser->name_table.hash_map[ix] = NAME_ID_INVALID;
    }

    parser->scope_stack = Mem_alloc(8 * sizeof(name_id));
    if (parser->scope_stack == NULL) {
        Arena_free(&parser->arena);
        Mem_free(function_data);
        Mem_free(type_data);
        Mem_free(name_data);
        return false;
    }
    parser->scope_count = 1;
    parser->scope_capacity = 8;

    parser->pos = 0;
    parser->input_size = 0;
    parser->indata = NULL;

    parser->first_error = NULL;
    parser->last_error = NULL;

    insert_keyword(parser, "fn", NAME_ID_FN);
    insert_keyword(parser, "struct", NAME_ID_STRUCT);
    insert_keyword(parser, "if", NAME_ID_IF);
    insert_keyword(parser, "while", NAME_ID_WHILE);

    parser_add_builtin(parser, TYPE_ID_UINT64, TYPEDEF_UINT64, "uint64");
    parser_add_builtin(parser, TYPE_ID_INT64, TYPEDEF_INT64, "int64");
    parser_add_builtin(parser, TYPE_ID_FLOAT64, TYPEDEF_FLOAT64, "float64");
    parser_add_builtin(parser, TYPE_ID_CSTRING, TYPEDEF_CSTRING, "<String Type>");

    parser->scope_stack[0] = parser->name_table.size - 1;

    return true;
}

void fmt_errors(Parser* parser, WString* dest) {
    ParseError* err = parser->first_error;
    while (err != NULL) {
        if (err->kind == PARSE_ERROR_NONE) {
            continue;
        }
        switch (err->kind) {
        case PARSE_ERROR_NONE:
            break;
        case PARSE_ERROR_EOF:
            WString_format_append(dest, L"Error: unexpecetd end of file");
            break;
        case PARSE_ERROR_OUTOFMEMORY:
            WString_format_append(dest, L"Error: Out of memory");
            break;
        case PARSE_ERROR_INVALID_ESCAPE:
            WString_format_append(dest, L"Error: invalid escape code");
            break;
        case PARSE_ERROR_INVALID_CHAR:
            WString_format_append(dest, L"Error: bad character");
            break;
        case PARSE_ERROR_RESERVED_NAME:
            WString_format_append(dest, L"Error: illegal name");
            break;
        case PARSE_ERROR_BAD_NAME:
            WString_format_append(dest, L"Error: bad identifier");
            break;
        case PARSE_ERROR_INVALID_LITERAL:
            WString_format_append(dest, L"Error: invalid literal");
            break;
        case PARSE_ERROR_REDEFINITION:
            WString_format_append(dest, L"Error: redefinition");
            break;
        case PARSE_ERROR_BAD_ARRAY_SIZE:
            WString_format_append(dest, L"Error: Bad array size");
            break;
        case PARSE_ERROR_INTERNAL:
            WString_format_append(dest, L"Internal compiler error");
            break;
        }
        uint64_t row, col;
        find_row_col(parser, err->pos.start, &row, &col);
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

void fmt_expression(Expression* expr, Parser* parser, WString* dest) {
    switch (expr->type) {
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
            if (WString_create(&s)) {
                if (WString_from_utf8_bytes(&s, (char*)expr->literal.string.bytes,
                                        expr->literal.string.len)) {
                    WString_append_count(dest, s.buffer, s.length);
                }
                WString_free(&s);
            }
            WString_append(dest, '"');
            return;
        case EXPRESSION_VARIABLE:
            char* n = (char*)parser->name_table.data[expr->variable.ix].name;
            uint64_t len = parser->name_table.data[expr->variable.ix].name_len;
            if (WString_create(&s)) {
                if (WString_from_utf8_bytes(&s, n, len)) {
                    WString_append_count(dest, s.buffer, s.length);
                }
                WString_free(&s);
            }
            return;
        case EXPRESSION_ARRAY_INDEX:
            WString_append(dest, '(');
            fmt_expression(expr->array_index.array, parser, dest);
            WString_append(dest, '[');
            fmt_expression(expr->array_index.index, parser, dest);
            WString_append(dest, ']');
            WString_append(dest, ')');
            return;
        default:
            WString_extend(dest, L"UNKOWN");
            return;
    }
}

void fmt_statement(Statement* statement, Parser* parser, WString* dest) {
    switch (statement->type) {
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
        WString_extend(dest, L"if ");
        fmt_expression(statement->if_.condition, parser, dest);
        WString_extend(dest, L" {\n");
        for (uint64_t ix = 0; ix < statement->if_.statement_count; ++ix) {
            fmt_statement(statement->if_.statements[ix], parser, dest);
        }
        WString_extend(dest, L"}\n");
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

void fmt_statements(Statement** s, uint64_t count, Parser* parser, WString* dest) {
    for (uint64_t ix = 0; ix < count; ++ix) {
        fmt_statement(s[ix], parser, dest);
    }
}

void dump_errors(Parser* parser) {
    WString s;
    WString_create(&s);
    fmt_errors(parser, &s);
    _wprintf_e(L"%s", s.buffer);
    parser->first_error = NULL;
    parser->last_error = NULL;
    WString_free(&s);
}

#define abs(a) ((a) < 0 ? (a) : (-a))
#ifdef _MSC_VER
#define assert(e) if (!(e)) { _wprintf_e(L"Assertion failed: '%s' at %S:%u", L#e, __FILE__, __LINE__); ExitProcess(1); }
#else
#include <assert.h>
#endif

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

    assert(expr->type == EXPRESSION_BINOP);
    assert(expr->binop.op == BINOP_BOOL_AND);
    assert(expr->line.start == 0);
    assert(expr->line.end == 14);

    assert(expr->binop.lhs->type == EXPRESSION_LITERAL_UINT);
    assert(expr->binop.lhs->literal.uint == 1);
    assert(expr->binop.lhs->line.start == 0);
    assert(expr->binop.lhs->line.end == 1);

    assert(expr->binop.rhs->type == EXPRESSION_BINOP);
    assert(expr->binop.rhs->binop.op == BINOP_SUB);
    assert(expr->binop.rhs->line.start == 5);
    assert(expr->binop.rhs->line.end == 14);

    assert(expr->binop.rhs->binop.lhs->type == EXPRESSION_LITERAL_UINT);
    assert(expr->binop.rhs->binop.lhs->literal.uint == 2);
    assert(expr->binop.rhs->binop.lhs->line.start == 5);
    assert(expr->binop.rhs->binop.lhs->line.end == 6);

    assert(expr->binop.rhs->binop.rhs->type == EXPRESSION_BINOP);
    assert(expr->binop.rhs->binop.rhs->binop.op == BINOP_DIV);
    assert(expr->binop.rhs->binop.rhs->line.start == 9);
    assert(expr->binop.rhs->binop.rhs->line.end == 14);

    assert(expr->binop.rhs->binop.rhs->binop.lhs->type == EXPRESSION_LITERAL_UINT);
    assert(expr->binop.rhs->binop.rhs->binop.lhs->literal.uint == 3);
    assert(expr->binop.rhs->binop.rhs->binop.lhs->line.start == 9);
    assert(expr->binop.rhs->binop.rhs->binop.lhs->line.end == 10);

    assert(expr->binop.rhs->binop.rhs->binop.rhs->type == EXPRESSION_LITERAL_UINT);
    assert(expr->binop.rhs->binop.rhs->binop.rhs->literal.uint == 4);
    assert(expr->binop.rhs->binop.rhs->binop.rhs->line.start == 13);
    assert(expr->binop.rhs->binop.rhs->binop.rhs->line.end == 14);

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


    assert(expr->type == EXPRESSION_BINOP);
    assert(expr->binop.op == BINOP_BOOL_AND);
    assert(expr->line.start == 0);
    assert(expr->line.end == 25);

    assert(expr->binop.lhs->type == EXPRESSION_BINOP);
    assert(expr->binop.lhs->binop.op == BINOP_SUB);
    assert(expr->binop.lhs->line.start == 0);
    assert(expr->binop.lhs->line.end == 14);

    assert(expr->binop.lhs->binop.lhs->type == EXPRESSION_LITERAL_FLOAT);
    assert(abs(expr->binop.lhs->binop.lhs->literal.float64 - 1.2e-4) < 1e-20);
    assert(expr->binop.lhs->binop.lhs->line.start == 0);
    assert(expr->binop.lhs->binop.lhs->line.end == 6);

    assert(expr->binop.lhs->binop.rhs->type == EXPRESSION_LITERAL_FLOAT);
    assert(abs(expr->binop.lhs->binop.rhs->literal.float64 - .2e20) < 1e-20);
    assert(expr->binop.lhs->binop.rhs->line.start == 9);
    assert(expr->binop.lhs->binop.rhs->line.end == 14);

    assert(expr->binop.rhs->type == EXPRESSION_BINOP);
    assert(expr->binop.rhs->binop.op == BINOP_DIV);
    assert(expr->binop.rhs->line.start == 18);
    assert(expr->binop.rhs->line.end == 25);

    assert(expr->binop.rhs->binop.lhs->type == EXPRESSION_LITERAL_FLOAT);
    assert(abs(expr->binop.rhs->binop.lhs->literal.float64 - 3e1) < 1e-20);
    assert(expr->binop.rhs->binop.lhs->line.start == 18);
    assert(expr->binop.rhs->binop.lhs->line.end == 21);

    assert(expr->binop.rhs->binop.rhs->type == EXPRESSION_LITERAL_UINT);
    assert(expr->binop.rhs->binop.rhs->literal.uint == 4);
    assert(expr->binop.rhs->binop.rhs->line.start == 24);
    assert(expr->binop.rhs->binop.rhs->line.end == 25);

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

    data = "1() / 2(\"a\", \"b\") - 3(!!!5 + +-6[1+-5] + ~!7) && 4()";
    parser.indata = (uint8_t*)data;
    parser.input_size = strlen(data);
    parser.pos = 0;
    expr = parse_expression(&parser);
    WString_clear(&output);
    fmt_expression(expr, &parser, &output);
    assert(WString_equals_str(&output, L"(((({1}()) / ({2}(\"a\", \"b\"))) - ({3}(((!(!(!{5}))) + ((+(-({6}[({1} + (-{5}))]))) + (~(!{7}))))))) && ({4}()))"));

    dump_errors(&parser);

    uint64_t count;
    data = "uint64 a = 10; uint64 b = 2 * 2; a + b; if (a > b) { float64 x = 10; }}";
    parser.indata = (uint8_t*)data;
    parser.input_size = strlen(data);
    parser.pos = 0;
    Statement** statements = parse_statements(&parser, &count);
    dump_errors(&parser);
    WString_clear(&output);
    fmt_statements(statements, count, &parser, &output);
    _wprintf_e(L"%s", output.buffer);

    return 0;
}



