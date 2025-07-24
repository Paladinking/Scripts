#include "parser.h"
#include "log.h"
#include "format.h"
#include "glob.h"
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


void find_row_col(const Parser* parser, uint64_t pos, uint64_t* row, uint64_t* col) {
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
    Log_Shutdown();
    ExitProcess(1);
}

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
                LOG_DEBUG("insert_name: Name '%.*s' already taken", len, name);
                return NAME_ID_INVALID;
            }
        }
        n_id = parser->name_table.data[n_id].parent;
    }
    n_id = parser->name_table.size;
    LOG_DEBUG("insert_name: Inserted name '%.*s', id %llu", len, name, n_id);
    uint8_t* name_ptr = Arena_alloc(&parser->arena, len, 1);
    memcpy(name_ptr, name, len);
    if (parser->name_table.size == parser->name_table.capacity) {
        size_t new_cap = parser->name_table.capacity * 2;
        NameData* new_nd = Mem_realloc(parser->name_table.data,
                                       new_cap * sizeof(NameData));
        if (new_nd == NULL) {
            out_of_memory(parser);
            return NAME_ID_INVALID;
        }
        parser->name_table.data = new_nd;
        parser->name_table.capacity = new_cap;
    }
    parser->name_table.size += 1;
    parser->name_table.data[n_id].kind = kind;
    parser->name_table.data[n_id].has_var = false;
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
            out_of_memory(parser);
        }
        parser->type_table.data = d;
        parser->type_table.capacity *= 2;
    }
    parser->type_table.size += 1;
    return parser->type_table.size - 1;
}

AllocInfo allocation_of_type(Parser* parser, TypeDef* def) {
    if (def->kind == TYPEDEF_UINT64 || def->kind == TYPEDEF_INT64 || def->kind == TYPEDEF_FLOAT64) {
        return (AllocInfo){8, 8};
    } 
    if (def->kind == TYPEDEF_BOOL) {
        return (AllocInfo){1, 1};
    }
    if (def->kind == TYPEDEF_NONE || def->kind == TYPEDEF_FUNCTION) {
        return (AllocInfo){0, 1};
    }
    if (def->kind == TYPEDEF_CSTRING) {
        return (AllocInfo){PTR_SIZE, PTR_SIZE};
    }
    // TODO: struct
    LineInfo l = {0, 0, false};
    LOG_ERROR("Unkown typedef %d", def->kind);
    fatal_error(parser, PARSE_ERROR_INTERNAL, l);
    return (AllocInfo){0, 0};
}

AllocInfo allocation_of(Parser* parser, type_id id) {
    assert(id != TYPE_ID_INVALID);
    TypeDef* def = parser->type_table.data[id].type_def;
    if (parser->type_table.data[id].kind == TYPE_NORMAL) {
        return allocation_of_type(parser, def);
    } else {
        AllocInfo root = allocation_of_type(parser, def);
        uint64_t scale = parser->type_table.data[id].array_size;
        // For now...
        assert(parser->type_table.data[parser->type_table.data[id].parent].kind == TYPE_NORMAL);

        root.size *= scale;
        return root;
    }
}

type_id type_of(Parser* parser, name_id name) {
    if (name == NAME_ID_INVALID) {
        return TYPE_ID_INVALID;
    }
    return parser->name_table.data[name].type;
} 

type_id array_of(Parser* parser, type_id id, uint64_t size) {
    type_id t = create_type_id(parser);
    parser->type_table.data[t].kind = TYPE_ARRAY;
    parser->type_table.data[t].type_def = parser->type_table.data[id].type_def;
    parser->type_table.data[t].parent = id;
    parser->type_table.data[t].array_size = size;

    return t;
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
    d->array_size = 0;
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

name_id insert_variable_name(Parser* parser, const uint8_t* name, uint32_t name_len, type_id t,
                             uint64_t pos, func_id func_id) {
    name_id id = insert_name(parser, name, name_len,
                             t, NAME_VARIABLE);
    parser->name_table.data[id].function = func_id;
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
            out_of_memory(parser);
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
    parser->type_table.data[id].array_size = 0;
    parser->type_table.data[id].parent = TYPE_ID_INVALID;
    parser->type_table.data[id].type_def = def;

    name_id n = insert_type_name(parser, (const uint8_t*)name, strlen(name), id, def);
    def->name = n;
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

bool expect_char(Parser* parser, uint8_t c) {
    skip_spaces(parser);
    if (eof(parser)) {
        LOG_DEBUG("expect_char: expected '%c', got eof", c);
        add_error(parser, PARSE_ERROR_INVALID_CHAR, current_pos(parser));
        return false;
    }
    if (parser->indata[parser->pos] != c) {
        LOG_DEBUG("expect_char: expected '%c', got '%c'", c, parser->indata[parser->pos]);
        add_error(parser, PARSE_ERROR_INVALID_CHAR, current_pos(parser));
        return false;
    }
    parser->pos += 1;
    return true;
}

bool peek_keyword(Parser* parser, name_id keyword) {
    skip_spaces(parser);
    const uint8_t* name = parser->name_table.data[keyword].name;
    uint32_t name_len = parser->name_table.data[keyword].name_len;
    if (parser->input_size - parser->pos < name_len) {
        return false;
    }
    if (memcmp(name, parser->indata + parser->pos, name_len) != 0) {
        return false;
    }
    if (parser->input_size - parser->pos > name_len &&
        is_identifier(parser->indata[parser->pos + name_len])) {
        return false;
    }
    parser->pos += name_len;
    return true;
}


static bool parser_parse_uint(Parser* parser, uint64_t* i, uint8_t base) {
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

const uint8_t* parse_name(Parser* parser, uint32_t* len) {
    uint64_t name_len = 0;
    const uint8_t* base = parser->indata + parser->pos;

    if (!is_identifier_start(expect_data(parser))) {
        LOG_DEBUG("parse_name: '%c' is not identifier start", expect_data(parser));
        add_error(parser, PARSE_ERROR_INVALID_CHAR, current_pos(parser));
        return NULL;
    }

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
        LOG_INFO("parse_known_name: parsed unkown '%.*s'", len, base);
        add_error(parser, PARSE_ERROR_BAD_NAME, l);
        return NAME_ID_INVALID;
    }
    LOG_DEBUG("parse_known_name: parsed '%.*s'", len, base);
    return name;
}

// Also accepts function names
bool is_variable(Parser* parser, name_id name) {
    if (name == NAME_ID_INVALID) {
        return false;
    }
    enum NameKind kind = parser->name_table.data[name].kind;
    return kind == NAME_VARIABLE || kind == NAME_FUNCTION;
}
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
        return parser_parse_uint(parser, i, base);
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
        return parser_parse_uint(parser, i, 10);
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
        if (c >= '0' && c <= '9') {
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

type_id parse_known_type(Parser* parser) {
    uint32_t len = 0;
    const uint8_t* base = parse_name(parser, &len);
    if (base == NULL) {
        return TYPE_ID_INVALID;
    }
    name_id name = find_name(parser, base, len);
    if (name == NAME_ID_INVALID ||
         parser->name_table.data[name].kind != NAME_TYPE) {
        LineInfo l = {true, base - parser->indata, parser->pos};
        add_error(parser, PARSE_ERROR_BAD_TYPE, l);
        return TYPE_ID_INVALID;
    }

    // TODO: Array types...

    return parser->name_table.data[name].type;
}

// counts the number of arguments to a call
uint64_t count_args(Parser* parser, uint64_t* pos) {
    uint64_t count = 1;
    uint64_t open_par = 0;
    uint64_t open_bra = 0;
    uint64_t open_cur = 0;
    skip_spaces(parser);
    uint8_t c1 = expect_data(parser);
    if (c1 == ')' || c1 == '}' || c1 == ']') {
        if (pos != NULL) {
            *pos = parser->pos;
        }
        return 0;
    }
    for (uint64_t ix = parser->pos; ix < parser->input_size; ++ix) {
        uint8_t c = parser->indata[ix];
        if (c == '(') {
            ++open_par;
        } else if (c == ')') {
            if (open_par == 0) {
                if (pos != NULL) {
                    *pos = ix;
                }
                return count;
            }
            --open_par;
            continue;
        } else if (c == '[') {
            ++open_bra;
        } else if (c == ']') {
            if (open_bra == 0) {
                if (pos != NULL) {
                    *pos = ix;
                }
                return count;
            }
            --open_bra;
        } else if (c == '{') {
            ++open_cur;
        } else if (c == '}') {
            if (open_cur == 0) {
                if (pos != NULL) {
                    *pos = ix;
                }
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
    if (pos != NULL) {
        *pos = parser->input_size;
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
        n->kind = EXPRESSION_ARRAY_INDEX;
        n->type = TYPE_ID_INVALID;
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
        uint64_t count = count_args(parser, NULL);
        if (count == 0) {
            *n = *e;
            e->kind = EXPRESSION_CALL;
            e->call.function = n;
            e->call.arg_count = 0;
            e->call.args = NULL;
            e->line = i;
            parser->pos += 1;
            e->line.end = parser->pos;
            goto start;
        }
        n->kind = EXPRESSION_CALL;
        n->type = TYPE_ID_INVALID;
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
    n->kind = EXPRESSION_BINOP;
    n->type = TYPE_ID_INVALID;
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
        op = UNOP_NEGATIVE;
    } else if (c == '!') {
        op = UNOP_BOOLNOT;
    } else if (c == '~') {
        op = UNOP_BITNOT;
    } else {
        return false;
    }
    e->kind = EXPRESSION_UNOP;
    e->unop.op = op;
    e->unop.expr = NULL;
    e->line = i;
    parser->pos += 1;
    return e;
}

// pos will be first char not part of expression after
Expression* parse_expression(Parser* parser) {
    LOG_DEBUG("Parsing expression at %llu", parser->pos);
    struct Node {
        Expression* e;
    };

    uint64_t stack_size = 0;
    uint64_t stack_cap = 8;
    struct Node* stack = Mem_alloc(8 * sizeof(struct Node));
    if (stack == NULL) {
        out_of_memory(parser);
        return NULL;
    }

    while (1) {
loop:
        LOG_DEBUG("Expression parse loop at %llu", parser->pos);
        if (stack_size == stack_cap) {
            stack_cap *= 2;
            struct Node* n = Mem_realloc(stack,
                    stack_cap * sizeof(struct Node));
            if (n == NULL) {
                out_of_memory(parser);
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
        e->type = TYPE_ID_INVALID;
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
                e->kind = EXPRESSION_INVALID;
                Mem_free(stack);
                return e;
            } else if (is_int) {
                e->kind = EXPRESSION_LITERAL_UINT;
                e->literal.uint = u;
            } else {
                e->kind = EXPRESSION_LITERAL_FLOAT;
                e->literal.float64 = d;
            }
            e->line.end = parser->pos;
        } else if (c == '"') {
            uint64_t len;
            uint8_t* s = parse_string_literal(parser, &len);
            e->kind = EXPRESSION_LITERAL_STRING;
            e->line = i;
            e->line.end = parser->pos;
            e->literal.string.bytes = s;
            e->literal.string.len = len;
        } else {
            name_id name = parse_known_name(parser);
            if (name == NAME_ID_INVALID) {
                e->kind = EXPRESSION_INVALID;
                Mem_free(stack);
                return e;
            }
            e->line = i;
            e->line.end = parser->pos;
            if (name == NAME_ID_TRUE) {
                e->kind = EXPRESSION_LITERAL_BOOL;
                e->literal.uint = 1;
            } else if (name == NAME_ID_FALSE) {
                e->kind = EXPRESSION_LITERAL_BOOL;
                e->literal.uint = 0;
            } else if (is_variable(parser, name)) {
                e->kind = EXPRESSION_VARIABLE;
                e->variable.ix = name;
            } else {
                LineInfo l = {true, i.start, parser->pos};
                add_error(parser, PARSE_ERROR_BAD_NAME, l);
                e->kind = EXPRESSION_INVALID;
                Mem_free(stack);
                return e;
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
            if (n->kind == EXPRESSION_UNOP) {
                n->unop.expr = e;
                n->line.end = parser->pos;
                if (n->unop.op == UNOP_PAREN) {
                    uint8_t c = expect_data(parser);
                    if (c != ')') {
                        add_error(parser, PARSE_ERROR_INVALID_CHAR, current_pos(parser));
                        e->kind = EXPRESSION_INVALID;
                        Mem_free(stack);
                        return e;
                    }
                    parser->pos += 1;
                    n->line.end += 1;
                    check_postfix =  true;
                } else if (n->unop.expr->kind == EXPRESSION_BINOP) {
                    Expression* node = n;
                    Expression* bin = node->unop.expr;
                    n = node->unop.expr;
                    node->unop.expr = bin->binop.lhs;
                    bin->binop.lhs = node;
                    node->line.end = node->unop.expr->line.end;
                    bin->line.start = bin->binop.lhs->line.start;
                    Expression* parent = n;
                    while (node->unop.expr->kind == EXPRESSION_BINOP) {
                        bin = node->unop.expr;
                        node->unop.expr = bin->binop.lhs;
                        bin->binop.lhs = node;
                        node->line.end = node->unop.expr->line.end;
                        bin->line.start = bin->binop.lhs->line.start;
                        parent->binop.lhs = bin;
                        parent = bin;
                    }
                }
            } else if (n->kind == EXPRESSION_BINOP) {
                n->binop.rhs = e;
                n->line.end = parser->pos;
                n->line.end = parser->pos;
                Expression* node = n;
                uint32_t prio = binop_precedence(n->binop.op);

                if (e->kind == EXPRESSION_BINOP &&
                    prio < binop_precedence(e->binop.op)) {
                    Expression* lhs = e->binop.lhs;
                    e->line.start = node->line.start;
                    e->binop.lhs = node;
                    node->binop.rhs = lhs;
                    node->line.end = node->binop.rhs->line.end;
                    n = e;

                    Expression* parent = n;
                    while (node->binop.rhs->kind == EXPRESSION_BINOP &&
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
            } else if (n->kind == EXPRESSION_ARRAY_INDEX) {
                uint8_t c = expect_data(parser);
                if (c != ']') {
                    add_error(parser, PARSE_ERROR_INVALID_CHAR, current_pos(parser));
                    e->kind = EXPRESSION_INVALID;
                    Mem_free(stack);
                    return e;
                }
                parser->pos += 1;
                check_postfix =  true;
                n->line.end = parser->pos;
                n->array_index.index = e;
            } else if (n->kind == EXPRESSION_CALL) {
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
                    e->kind = EXPRESSION_INVALID;
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

Statement** parse_statements(Parser* parser, uint64_t* count, func_id function) {

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
            skip_spaces(parser);
            if (stack_size > 0) {
                uint64_t last_top_count = 0;
                Statement* top = stack[stack_size - 1].statement;
                if (top->type == STATEMENT_IF) {
                    while (top->if_.else_branch != NULL) {
                        top = top->if_.else_branch;
                    }
                    top->if_.statements = list;
                    last_top_count = top->if_.statement_count;
                    top->if_.statement_count = top_count;
                    top->line.end = parser->pos;
                    top_count = last_top_count;
                    skip_spaces(parser);
                    uint64_t else_pos = parser->pos;
                    if (peek_keyword(parser, NAME_ID_ELSE)) {
                        Expression* cond = NULL;
                        if (peek_keyword(parser, NAME_ID_IF)) {
                            if (!expect_char(parser, '(')) {
                                skip_statement(parser);
                                continue;
                            }
                            cond = parse_expression(parser);
                            if (cond->kind == EXPRESSION_INVALID ||
                                !expect_char(parser, ')')) {
                                skip_statement(parser);
                                continue;
                            }
                        }
                        if (!expect_char(parser, '{')) {
                            skip_statement(parser);
                            continue;
                        }
                        Statement* s = Arena_alloc_type(&parser->arena, Statement);
                        s->type = STATEMENT_IF;
                        s->if_.condition = cond;
                        s->if_.statement_count = top_count;
                        s->if_.else_branch = NULL;
                        s->line.start = else_pos;
                        top->if_.else_branch = s;
                        top_count = 0;
                        begin_scope(parser);
                        continue;
                    }
                } else if (top->type == STATEMENT_WHILE) {
                    top->while_.statements = list;
                    last_top_count = top->while_.statement_count;
                    top->while_.statement_count = top_count;
                    top->line.end = parser->pos;
                    top_count = last_top_count;
                } else {
                    fatal_error(parser, PARSE_ERROR_INTERNAL, current_pos(parser));
                }
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
                if (name == NAME_ID_RETURN) {
                    Expression* e = parse_expression(parser);
                    if (e->kind == EXPRESSION_INVALID) {
                        skip_statement(parser);
                        continue;
                    }
                    if (!expect_char(parser, ';')) {
                        skip_statement(parser);
                        continue;
                    }
                    s = Arena_alloc_type(&parser->arena, Statement);
                    s->type = STATEMENT_RETURN;
                    s->return_.return_value = e;
                    s->line.start = pos;
                    s->line.end = parser->pos;
                    stack[stack_size].statement = s;
                    ++stack_size;
                    ++top_count;
                    continue;
                }

                if (!expect_char(parser, '(')) {
                    skip_statement(parser);
                    continue;
                }
                Expression* e = parse_expression(parser);
                if (e->kind == EXPRESSION_INVALID) {
                    skip_statement(parser);
                    continue;
                }
                if (!expect_char(parser, ')') || !expect_char(parser, '{')) {
                    skip_statement(parser);
                    continue;
                }

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
                    s->if_.else_branch = NULL;
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
                    if (!expect_char(parser, ']')) {
                        skip_statement(parser);
                        continue;
                    }
                    t = array_of(parser, t, size);
                    skip_spaces(parser);
                    c = expect_data(parser);
                }

                uint64_t name_start = parser->pos;
                const uint8_t* name = parse_name(parser, &len);
                name_id n;
                if (name == NULL || 
                    (n = insert_variable_name(parser, name, len, t, pos, function)) == NAME_ID_INVALID) {
                    skip_statement(parser);
                    continue;
                }
                uint64_t name_end = parser->pos;

                skip_spaces(parser);
                c = expect_data(parser);
                if (c == '=') {
                    parser->pos += 1;
                    Expression* e = parse_expression(parser);
                    if (e->kind == EXPRESSION_INVALID) {
                        skip_statement(parser);
                        continue;
                    }
                    if (!expect_char(parser, ';')) {
                        skip_statement(parser);
                        continue;
                    }
                    Statement* s = Arena_alloc_type(&parser->arena, Statement);
                    s->type = STATEMENT_ASSIGN;
                    Expression* lhs = Arena_alloc_type(&parser->arena, Expression);
                    lhs->kind = EXPRESSION_VARIABLE;
                    lhs->type = TYPE_ID_INVALID;
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
        if (e->kind == EXPRESSION_INVALID) {
            skip_statement(parser);
            continue;
        }
        skip_spaces(parser);
        c = expect_data(parser);
        Statement* s;
        if (c == '=') {
            parser->pos += 1;
            Expression* rhs = parse_expression(parser);
            if (rhs->kind == EXPRESSION_INVALID) {
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

// 'fn' is already consumed
bool scan_function(Parser* parser) {
    LOG_DEBUG("Scanning function at %llu", parser->pos);
    uint64_t pos = parser->pos - 2;
    skip_spaces(parser);
    uint32_t len;
    const uint8_t* name = parse_name(parser, &len);
    if (name == NULL) {
        return false;
    }
    LOG_DEBUG("Found fn name '%.*s' at %llu", len, name, parser->pos);
    if (!expect_char(parser, '(')) {
        LOG_DEBUG("Missing '(' at %llu", parser->pos);
        return false;
    }

    uint64_t skip = 0;
    uint64_t arg_count = count_args(parser, &skip);
    parser->pos = skip;
    LOG_DEBUG("Skip %llu", skip);
    if (!expect_char(parser, ')')) {
        return false;
    }
    skip_spaces(parser);
    if (expect_data(parser) == '-') {
        parser->pos += 1;
        if (expect_data(parser) != '>') {
            return false;
        }
        parser->pos += 1;
    }

    skip_statement(parser);

    FunctionDef* func = Arena_alloc_type(&parser->arena, FunctionDef);
    func->arg_count = arg_count;
    func->return_type = TYPE_ID_INVALID;
    func->line.start = pos;
    func->line.end = parser->pos;

    name_id id = insert_function_name(parser, name, len, func);
    if (id == NAME_ID_INVALID) {
        LineInfo l = {true, pos, parser->pos};
        add_error(parser, PARSE_ERROR_BAD_NAME, l);
        return false;
    }

    func->name = id;
    return true;
}


FunctionDef* parse_function(Parser* parser) {
    LOG_DEBUG("parse_function: parsing function");
    skip_spaces(parser);
    name_id name = parse_known_name(parser);
    if (name == NAME_ID_INVALID) {
        return NULL;
    }
    if (parser->name_table.data[name].kind != NAME_FUNCTION) {
        add_error(parser, PARSE_ERROR_BAD_NAME, current_pos(parser));
        return NULL;
    }

    FunctionDef* f = parser->name_table.data[name].func_def;
    func_id id = parser->function_table.size;

    CallArg* args = Arena_alloc_count(&parser->arena, CallArg, f->arg_count);
    if (!expect_char(parser, '(')) {
        return NULL;
    }
    f->args = args;
    begin_scope(parser);
    LOG_DEBUG("parse_function: parsing %lu arguments", f->arg_count);
    if (f->arg_count == 0 && !expect_char(parser, ')')) {
        LOG_DEBUG("parse_function: expected ')' for no argument function");
        return NULL;
    }
    for (uint64_t ix = 0; ix < f->arg_count; ++ix) {
        uint8_t end = ix == f->arg_count - 1 ? ')' : ',';
        skip_spaces(parser);
        uint64_t p = parser->pos;
        type_id var_type = parse_known_type(parser);

        uint32_t len;
        skip_spaces(parser);
        const uint8_t* var_name = parse_name(parser, &len);
        if (var_type == TYPE_ID_INVALID || var_name == NULL) {
            while (expect_data(parser) != end && expect_data(parser) != '{') {
                parser->pos += 1;
            }
            if (expect_data(parser) != '{') {
                parser->pos += 1;
            }
            args[ix].name = NAME_ID_INVALID;
            args[ix].line.real = false;
            continue;
        }
        name_id var_id = insert_variable_name(parser, var_name, len, var_type, p, id);
        args[ix].name = var_id;
        args[ix].line.start = p;
        args[ix].line.end = parser->pos;
        args[ix].type = var_type;
        expect_char(parser, end);
    }
    skip_spaces(parser);
    uint8_t c = expect_data(parser);
    if (c == '-') {
        LOG_DEBUG("parse_function: found return type");
        parser->pos += 1;
        if (expect_data(parser) != '>') {
            add_error(parser, PARSE_ERROR_INVALID_CHAR, current_pos(parser));
            end_scope(parser);
            return NULL;
        }
        parser->pos += 1;
        skip_spaces(parser);
        type_id ret_type = parse_known_type(parser);
        if (ret_type == TYPE_ID_INVALID) {
            end_scope(parser);
            return NULL;
        }
        f->return_type = ret_type;
    } else {
        f->return_type = TYPE_ID_NONE;
    }

    if (!expect_char(parser, '{')) {
        LOG_DEBUG("parse_function: Missing '{'");
        end_scope(parser);
        return NULL;
    }

    uint64_t statement_count;
    LOG_DEBUG("parse_function: parsing statements");
    Statement** statements = parse_statements(parser, &statement_count, id);
    f->statements = statements;
    f->statement_count = statement_count;
    end_scope(parser);

    return f;
}


bool parse_program(Parser* parser) {
    uint64_t pos = parser->pos;
    while (1) {
        skip_spaces(parser);
        if (eof(parser)) {
            break;
        }

        uint64_t pos = parser->pos;
        name_id id = parse_known_name(parser);
        if (id == NAME_ID_INVALID) {
            skip_statement(parser);
            continue;
        } else if (id != NAME_ID_FN) {
            LineInfo l = {true, pos, parser->pos};
            add_error(parser, PARSE_ERROR_BAD_NAME, l);
            skip_statement(parser);
            continue;
        }
        if (!scan_function(parser)) {
            LOG_DEBUG("Failed scan_function at %llu", parser->pos);
            skip_statement(parser);
            continue;
        }
    }
    LOG_INFO("parse_program: Done scanning");
    parser->pos = pos;
    while (1) {
        skip_spaces(parser);
        if (eof(parser)) {
            break;
        }
        uint64_t pos = parser->pos;
        name_id id = parse_known_name(parser);
        if (id == NAME_ID_INVALID) {
            skip_statement(parser);
            continue;
        } else if (id != NAME_ID_FN) {
            LineInfo l = {true, pos, parser->pos};
            add_error(parser, PARSE_ERROR_BAD_NAME, l);
            skip_statement(parser);
            continue;
        }
        FunctionDef* func = parse_function(parser);
        if (func == NULL) {
            skip_statement(parser);
            continue;
        }
        if (parser->function_table.size == parser->function_table.capacity) {
            parser->function_table.capacity *= 2;
            FunctionDef** funcs = Mem_realloc(parser->function_table.data,
                    parser->function_table.capacity * sizeof(FunctionDef*));
            if (funcs == NULL) {
                fatal_error(parser, PARSE_ERROR_OUTOFMEMORY, current_pos(parser));
                return false;
            }
            parser->function_table.data = funcs;
        }
        parser->function_table.data[parser->function_table.size] = func;
        parser->function_table.size += 1;
    }

    return true;
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
    insert_keyword(parser, "else", NAME_ID_ELSE);
    insert_keyword(parser, "return", NAME_ID_RETURN);
    insert_keyword(parser, "true", NAME_ID_TRUE);
    insert_keyword(parser, "false", NAME_ID_FALSE);

    parser_add_builtin(parser, TYPE_ID_UINT64, TYPEDEF_UINT64, "uint64");
    parser_add_builtin(parser, TYPE_ID_INT64, TYPEDEF_INT64, "int64");
    parser_add_builtin(parser, TYPE_ID_FLOAT64, TYPEDEF_FLOAT64, "float64");
    parser_add_builtin(parser, TYPE_ID_CSTRING, TYPEDEF_CSTRING, "<String Type>");
    parser_add_builtin(parser, TYPE_ID_NONE, TYPEDEF_NONE, "<None Type>");
    parser_add_builtin(parser, TYPE_ID_BOOL, TYPEDEF_BOOL, "bool");

    parser->scope_stack[0] = parser->name_table.size - 1;

    return true;
}


