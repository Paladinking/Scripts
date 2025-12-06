#include "tokenizer.h"
#include "tables.h"

const static enum LogCatagory LOG_CATAGORY = LOG_CATAGORY_TOKENIZER;


// Set parser input to indata
void parser_set_input(Parser* parser, String* indata, const char* filename, bool is_import) {
    parser->filename = filename;
    parser->import_module = is_import;
    parser->indata = (const uint8_t*)indata->buffer;
    parser->pos = 0;
    parser->input_size = indata->length;
}

// Skip all space characters at current pos
// Also skip comments
void parser_skip_spaces(Parser* parser) {
    bool line_comment = false;
    do {
        if (parser->pos >= parser->input_size) {
            return;
        }
        uint8_t c = parser->indata[parser->pos];
        if (line_comment) {
            if (c == '\n' || c== '\r') {
                line_comment = false;
            } else {
                ++parser->pos;
                continue;
            }
        }

        if (c == '/') {
            if (parser->pos + 1 < parser->input_size &&
                parser->indata[parser->pos + 1] == '/') {
                line_comment = true;
                parser->pos += 2;
                continue;
            }
        }
        if (c != ' ' && c != '\t' && c != '\n' && c != '\r' &&
            c != '\f' && c != '\v') {
            return;
        }
        ++parser->pos;
    } while (1);
}

void parser_skip_statement(Parser *parser) {
    assert(parser->pos < parser->input_size && parser->indata[parser->pos] == '{');
    uint64_t pos = parser->pos;
    uint64_t open_cur = 0;
    while (1) {
        if (parser->pos >= parser->input_size) {
            LineInfo l = {parser->filename, pos, parser->pos};
            add_error(parser, PARSE_ERROR_EOF, l);
            return;
        }
        uint8_t c = parser->indata[parser->pos];
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
                if (parser->pos >= parser->input_size) {
                    LineInfo l = {parser->filename, pos, parser->pos};
                    add_error(parser, PARSE_ERROR_EOF, l);
                    return;
                }
                uint8_t c2 = parser->indata[parser->pos];
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

// Read a keyword / identifier
const uint8_t* parser_read_name(Parser* parser, uint32_t* len) {
    uint64_t name_len = 0;
    const uint8_t* base = parser->indata + parser->pos;

    assert(parser->pos < parser->input_size && is_identifier_start(parser->indata[parser->pos]));

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
        LineInfo l = {parser->filename, base - parser->indata, parser->pos};
        add_error(parser, PARSE_ERROR_BAD_NAME, l);
        *len = 0;
        return NULL;
    }
    *len = name_len;
    return base;
}

// Read a string literal
uint8_t* parser_read_string(Parser* parser, uint64_t* len) {
    // Skip leading '"'
    parser->pos += 1;
    uint64_t pos = parser->pos;
    *len = 0;
    bool has_err = false;
    while (1) {
        if (parser->pos >= parser->input_size) {
            LineInfo l = {parser->filename, pos - 1, parser->pos};
            add_error(parser, PARSE_ERROR_EOF, l);
            return NULL;
        }
        uint8_t c = parser->indata[parser->pos];
        parser->pos += 1;
        if (c == '"') {
            break;
        } if (c == '\\') {
            if (parser->pos >= parser->input_size) {
                LineInfo l = {parser->filename, pos - 1, parser->pos};
                add_error(parser, PARSE_ERROR_EOF, l);
                return NULL;
            } 
            c = parser->indata[parser->pos];
            parser->pos += 1;
            if (c == '\\' || c == 'n' || c == 'r' || c == 'v' || c == 'f' ||
                c == 't' || c == '0' || c == '"' || c == '\'') {
                *len += 1;
                continue;
            } else if (c == 'x') {
                if (parser->pos >= parser->input_size) {
                    LineInfo l = {parser->filename, pos - 1, parser->pos};
                    add_error(parser, PARSE_ERROR_EOF, l);
                    return NULL;
                }
                c = parser->indata[parser->pos];
                parser->pos += 1;
                if ((c < '0' || c > '9') && (c < 'a' || c > 'z') && 
                    (c < 'A' || c > 'Z')) {
                    LineInfo pos = {parser->filename, parser->pos - 3, parser->pos};
                    add_error(parser, PARSE_ERROR_INVALID_ESCAPE, pos);
                    has_err = true;
                    parser->pos -= 1;
                } else {
                    *len += 1;
                    if (parser->pos >= parser->input_size) {
                        LineInfo l = {parser->filename, pos - 1, parser->pos};
                        add_error(parser, PARSE_ERROR_EOF, l);
                        return NULL;
                    }
                    c = parser->indata[parser->pos];
                    if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || 
                        (c >= 'A' && c <= 'F')) {
                        parser->pos += 1;
                    }
                    continue;
                }
            } else {
                LineInfo pos = {parser->filename, parser->pos - 2, parser->pos};
                add_error(parser, PARSE_ERROR_INVALID_ESCAPE, pos);
                has_err = true;
                parser->pos -= 1;
            }
        }
        *len += 1;
    }
    if (has_err) {
        *len = 0;
        return NULL;
    }
    uint8_t* data = Arena_alloc(&parser->arena, *len, 1);
    parser->pos = pos;

    uint64_t ix = 0;
    while (1) {
        uint8_t c = parser->indata[parser->pos];
        parser->pos += 1;
        if (c == '"') {
            return data;
        }
        if (c == '\\') {
            uint8_t c = parser->indata[parser->pos];
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
                uint8_t c = parser->indata[parser->pos];
                parser->pos += 1;
                uint8_t val;
                if (c >= '0' && c <= '9') {
                    val = c - '0';
                } else if (c >= 'a' && c <= 'f') {
                    val = c - 'a' + 10;
                } else {
                    val = c - 'A' + 10;
                }
                c = parser->indata[parser->pos];
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

static bool parser_read_uint(Parser* parser, uint64_t* i, uint8_t base) {
    *i = 0;
    uint64_t n = 0;
    if (parser->pos >= parser->input_size) {
        LineInfo l = {parser->filename, parser->pos, parser->pos};
        add_error(parser, PARSE_ERROR_EOF, l);
        return false;
    }
    uint8_t c = parser->indata[parser->pos];
    uint64_t start = parser->pos;
    bool overflow = false;

    if (base == 16) { // Hex
        if ((c < '0' || c > '9') && (c < 'a' || c > 'f') &&
            (c < 'A' && c > 'F')) {
            LineInfo l = {parser->filename, parser->pos, parser->pos + 1};
            add_error(parser, PARSE_ERROR_INVALID_CHAR, l);
            return false;
        }
        do {
            c = parser->indata[parser->pos];
            if (n > 0xfffffffffffffff) {
                overflow = true;
                n = 0;
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
            LineInfo l = {parser->filename, parser->pos, parser->pos + 1};
            add_error(parser, PARSE_ERROR_INVALID_CHAR, l);
            return false;
        }
        do {
            c = parser->indata[parser->pos];
            if (n > 0x7fffffffffffffff) {
                overflow = true;
                n = 0;
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
            LineInfo l = {parser->filename, parser->pos, parser->pos + 1};
            add_error(parser, PARSE_ERROR_INVALID_CHAR, l);
            return false;
        }
        do {
            c = parser->indata[parser->pos];
            if (n > 0x1fffffffffffffff) {
                overflow = true;
                n = 0;
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
            LineInfo l = {parser->filename, parser->pos, parser->pos + 1};
            add_error(parser, PARSE_ERROR_INVALID_CHAR, l);
            return false;
        }
        do {
            c = parser->indata[parser->pos];
            if (c < '0' || c > '9') {
                break;
            }
            uint64_t v = c - '0';
            if (n > 0x1999999999999999) {
                overflow = true;
                n = 0;
            } else if (n == 0x1999999999999999) {
                if (v > 5) {
                    overflow = true;
                    n = 0;
                }
            }
            n = n * 10;
            n += v;
            parser->pos += 1;
        } while (parser->pos < parser->input_size);
    }
    if (overflow) {
        LineInfo l = {parser->filename, start, parser->pos};
        add_error(parser, PARSE_ERROR_INVALID_LITERAL, l);
        return false;
    }

    *i = n;
    return true;
}

// Read a number literar, integer or real
bool parser_read_number(Parser* parser, uint64_t* i, double* d, bool* is_int) {
    if (parser->pos >= parser->input_size) {
        LineInfo l = {parser->filename, parser->pos, parser->pos};
        add_error(parser, PARSE_ERROR_EOF, l);
        return false;
    }
    uint8_t c = parser->indata[parser->pos];
    if ((c < '0' || c > '9') && c != '.') {
        LineInfo l = {parser->filename, parser->pos, parser->pos + 1};
        add_error(parser, PARSE_ERROR_INVALID_CHAR, l);
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
        if (parser->pos >= parser->input_size) {
            LineInfo l = {parser->filename, parser->pos, parser->pos};
            add_error(parser, PARSE_ERROR_EOF, l);
            return false;
        }
        c = parser->indata[parser->pos];
    }
    *is_int = true;

    if (base != 10) {
        return parser_read_uint(parser, i, base);
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
        return parser_read_uint(parser, i, 10);
    }
    if (c == '.') {
        *d = 0.0;
    } else {
        *d = (c - '0');
        parser->pos += 1;
        if (parser->pos >= parser->input_size) {
            return true;
        }
    }

    if (c != '0' && c != '.') {
        c = parser->indata[parser->pos];
        while (c >= '0' && c <= '9') {
            *d *= 10;
            *d += c - '0';
            parser->pos += 1;
            if (parser->pos >= parser->input_size) {
                return true;
            }
            c = parser->indata[parser->pos];
        }
    } else if (c == '0'){
        c = parser->indata[parser->pos];
        if (c >= '0' && c <= '9') {
            LineInfo l = {parser->filename, parser->pos, parser->pos + 1};
            add_error(parser, PARSE_ERROR_INVALID_CHAR, l);
            return false;
        }
    }
    if (c == '.') {
        parser->pos += 1;
        if (parser->pos >= parser->input_size) {
            LineInfo l = {parser->filename, parser->pos, parser->pos};
            add_error(parser, PARSE_ERROR_EOF, l);
            return false;
        }
        c = parser->indata[parser->pos];
        if (c < '0' || c > '9') {
            LineInfo l = {parser->filename, parser->pos, parser->pos + 1};
            add_error(parser, PARSE_ERROR_INVALID_CHAR, l);
            return false;
        }
        double factor = 0.1;
        while (c >= '0' && c <= '9') {
            *d += (c - '0') * factor;
            factor *= 0.1;
            parser->pos += 1;
            if (parser->pos >= parser->input_size) {
                return true;
            }
            c = parser->indata[parser->pos];
        }
    }
    if (c == 'e' || c == 'E') {
        parser->pos += 1;
        if (parser->pos >= parser->input_size) {
            LineInfo l = {parser->filename, parser->pos, parser->pos};
            add_error(parser, PARSE_ERROR_EOF, l);
            return false;
        }
        c = parser->indata[parser->pos];
        int64_t exp = 0;
        bool neg_exp = false;
        if (c == '-' || c == '+') {
            neg_exp = c == '-';
            parser->pos += 1;
            if (parser->pos >= parser->input_size) {
                LineInfo l = {parser->filename, parser->pos, parser->pos};
                add_error(parser, PARSE_ERROR_EOF, l);
                return false;
            }
            c = parser->indata[parser->pos];
        }
        if (c < '0' || c > '9') {
            return false;
        }
        while (c >= '0' && c <= '9') {
            exp = exp * 10;
            exp += c - '0';
            parser->pos += 1;
            if (parser->pos >= parser->input_size) {
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

void parser_row_col(const Parser* parser, uint64_t pos, uint64_t* row, uint64_t* col) {
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
