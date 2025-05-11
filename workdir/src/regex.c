#include "regex.h"
#include "args.h"
#include "assert.h"
#include "dynamic_string.h"
#include <stdlib.h>

const static uint8_t utf8_len_table[256] = {
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1
};

const static uint8_t utf8_valid_table[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0xa0, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x9f, 0x80, 0x80, 0x90, 0x80, 0x80, 0x80, 0x8f, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// get how many bytes should be considered one sequence,
// the sequence might be invalid.
// If the sequence is invalid, includes byte 0 and up to
// 3 bytes that cannot start a sequence
static uint8_t get_utf8_seq(const uint8_t* bytes, uint64_t len) {
    if (len < 2) {
        return len;
    }
    if (bytes[0] <= 127) {
        return 1;
    }
    uint8_t v = utf8_valid_table[bytes[0] - 128];
    uint8_t l = utf8_len_table[bytes[0]];
    if (v == 0) { // Illegal byte 0
        if (bytes[1] >= 0x80 && bytes[1] <= 0xBF) {
            if (len > 2 && bytes[2] >= 0x80 && bytes[2] <= 0xBF) {
                if (len > 3 && bytes[3] >= 0x80 && bytes[3] <= 0xBF) {
                    return 4;
                }
                return 3;
            }
            return 2;
        }
        return 1;
    }

    if (l > len) { // Not enough bytes for full sequence
        if (bytes[1] >= 0x80 && bytes[1] <= 0xBF) {
            if (len > 2 && bytes[2] >= 0x80 && bytes[1] <= 0xBF) {
                return 3;
            }
            return 2;
        }
        return 1;
    }

    if (((v & 1) && (bytes[1] < 0x80 || bytes[1] > v)) ||
         (!(v & 1) && (bytes[1] < v || bytes[1] > 0xBF))) { // Illegal byte 1
        // Still include all bytes that cannot start a sequence
        if (bytes[1] >= 0x80 && bytes[1] <= 0xBF) {
            if (len > 2 && bytes[2] >= 0x80 && bytes[2] <= 0xBF) {
                if (len > 3 && bytes[3] >= 0x80 && bytes[3] <= 0xBF) {
                    return 4;
                }
                return 3;
            }
            return 2;
        }
        return 1;
    }
    if (l > 2) {
        if (bytes[2] < 0x80 || bytes[2] > 0xBF) { // Illegal byte 2
            return 2;
        }
        if (l > 3) {
            if (bytes[3] < 0x80 || bytes[3] > 0xBF) { // Illegal byte 3
                return 3;
            }
            return 4;
        }
        return 3;
    }
    return 2;
}

// Validate a single utf8 charpoint
static bool validate_utf8_seq(const uint8_t* bytes, uint64_t len) {
    if (len < 1) {
        return false;
    }
    if (bytes[0] <= 127) {
        return true;
    }
    uint8_t v = utf8_valid_table[bytes[0] - 128];
    uint8_t l = utf8_len_table[bytes[0]];
    if (v == 0 || l > len) {
        return false;
    }
    // Now known that l > 1, since all single byte sequences start with < 128
    if (l > 2) {
        if (bytes[2] < 0x80 || bytes[2] > 0xBF) {
            return false;
        }
        if (l > 3 && (bytes[3] < 0x80 || bytes[3] > 0xBF)) {
            return false;
        }
    }
    if (v & 1) {
        if (bytes[1] < 0x80 || bytes[1] > v) {
            return false;
        }
    } else if (bytes[1] < v || bytes[1] > 0xBF) {
        return false;
    }
    return true;
}

#define UTF8_EQ(s1, s2, l) ((s1)[0] == (s2)[0] && \
                            ((l) < 2 || ((s1)[1] == (s2)[1] && \
                             ((l) < 3 || ((s1)[2] == (s2)[2] && \
                              ((l) < 4 || (s1)[3] == (s2)[3]))))))

// WRONG on error, should not compare with 0x7F
static uint_fast8_t utf8_to_utf32(const uint8_t* utf8, uint64_t len, uint32_t* utf32) {
    if (*utf8 <= 0x7F) {
        *utf32 = *utf8;
        return 1;
    }
    *utf32 = 0;

    uint8_t l = utf8_len_table[*utf8];
    uint8_t v = utf8_valid_table[*utf8 - 128];
    if (v == 0) {
        return 1;
    }
    if (len < l) {
        return len;
    }
    if (l == 1) {
        *utf32 = *utf8;
        return l;
    }
    if (v & 1) {
        if (utf8[1] < 0x80 || utf8[1] > v) {
            if (l > 2 && utf8[2] > 0x7F) {
                if (l > 3 && utf8[3] > 0x7F) {
                    return 4;
                }
                return 3;
            }
            return 2;
        }
    } else {
        if (utf8[1] < v || utf8[1] > 0xBF) {
            if (l > 2 && utf8[2] > 0x7F) {
                if (l > 3 && utf8[3] > 0x7F) {
                    return 4;
                }
                return 3;
            }
            return 2;
        }
    }

    if (l == 2) {
        *utf32 = ((uint32_t)utf8[1] & 0b111111) |
                 (((uint32_t)utf8[0] & 0b11111) << 6);

        return 2;
    } else if (l == 3) {
        if (utf8[2] < 0x80 || utf8[2] > 0xBF) {
            return 3;
        }
        *utf32 = (((uint32_t)utf8[2]) & 0b111111) |
                 (((uint32_t)utf8[1] & 0b111111) << 6) |
                 (((uint32_t)utf8[0] & 0b1111) << 12);
        return 3;
    } else {
        if (utf8[2] < 0x80 || utf8[2] > 0xBF) {
            if (utf8[3] > 0x7F) {
                return 4;
            }
            return 3;
        }
        if (utf8[3] < 0x80 || utf8[3] > 0xBF) {
            return 4;
        }
        *utf32 = (((uint32_t)utf8[3]) & 0b111111) |
                 (((uint32_t)utf8[2] & 0b111111) << 6) |
                 (((uint32_t)utf8[1] & 0b111111) << 12) |
                 (((uint32_t)utf8[0] & 0b111) << 18);
        return 4;
    }
}

enum EdgeType {
    NFA_EDGE_ANY,
    NFA_EDGE_LITERAL,
    NFA_EDGE_UNION,
    NFA_EDGE_UNION_NOT
};

struct EdgeNFA {
    enum EdgeType type;
    uint32_t str_ix;
    uint32_t str_size;

    uint32_t from;
    uint32_t to;
};

struct NodeNFA {
    uint32_t edge_ix;
    uint32_t edge_count;

    bool accept;
};

enum RegexType {
    REGEX_LITERAL,
    REGEX_REPEAT,
    REGEX_ANY,
    REGEX_EMPTY,
    REGEX_PAREN,
    REGEX_LITERAL_UNION
};

typedef struct RegexAst {
    enum RegexType type;
    union {
        struct {
            uint32_t str_ix;
            uint32_t size;
        } literal;
        struct {
            uint32_t node_ix;
            uint32_t min;
            uint32_t max;
        } repeat;
        struct {
            uint32_t node_ix;
        } paren;
        struct {
            bool not;
            uint32_t str_ix;
            uint32_t size;
        } literal_union;
    };
    uint32_t next;
} RegexAst;

typedef struct ParseCtx {
    uint32_t size;
    uint32_t capacity;
    uint32_t node_ix;

    RegexAst *ast;

    uint32_t paren_stack_size;
    uint32_t paren_stack_cap;
    uint32_t *paren_stack;

    String pattern;
} ParseCtx;

#define DFA_REJECT_NODE ((uint32_t)-1)
//#define DFA_REJECT_NODE 255

typedef struct NodeSet {
    uint32_t *nodes;
    uint32_t node_count;
} NodeSet;

typedef struct EdgeSet {
    uint32_t *edges;
    uint32_t edge_count;
} EdgeSet;

typedef struct Utf8Edge {
    uint8_t bytes[4];
    uint32_t node_ix;
} Utf8Edge;

struct NodeDFA {
    bool literal_node;
    bool accept;
    union {
        struct {
            uint8_t ascii[128];
            uint32_t *ascii_edges;
            Utf8Edge *utf8_edges;
            uint32_t utf8_edge_count;
            uint32_t default_edge;
        };
        struct {
            uint8_t str[144];
            uint32_t str_len;
            uint32_t node_ix;
        } literal;
    };
};

typedef struct NodeDFABuilder {
    NodeSet nodes;
    EdgeSet edges;
    bool is_literal;
    uint32_t ascii_edge_count;
    uint32_t ascii_edge_cap;
    uint32_t utf8_edge_cap;
    uint32_t in_edge_count;
} NodeDFABuilder;

typedef struct DFABuilder {
    NodeDFABuilder *builders;
    NodeDFA *nodes;
    uint32_t node_count;
    uint32_t node_cap;

    uint8_t *visited;
    uint32_t *to_visit;
    uint32_t to_visit_cap;

    uint32_t *stack;
    uint32_t stack_size;
    uint32_t stack_cap;
    uint32_t minlen;
} DFABuilder;

bool parse_init(ParseCtx *ctx) {
    ctx->capacity = 4;
    ctx->ast = Mem_alloc(ctx->capacity * sizeof(RegexAst));
    if (ctx->ast == NULL) {
        return false;
    }
    ctx->size = 1;
    ctx->ast[0].type = REGEX_EMPTY;
    ctx->ast[0].next = 1;
    ctx->node_ix = 0;

    ctx->paren_stack_size = 0;
    ctx->paren_stack_cap = 4;
    ctx->paren_stack = Mem_alloc(ctx->paren_stack_cap * sizeof(uint32_t));
    if (ctx->paren_stack == NULL) {
        Mem_free(ctx->ast);
        return false;
    }
    if (!String_create(&ctx->pattern)) {
        Mem_free(ctx->ast);
        Mem_free(ctx->paren_stack);
        return false;
    }
    return true;
}

void parse_free(ParseCtx *ctx) {
    Mem_free(ctx->ast);
    Mem_free(ctx->paren_stack);
    String_free(&ctx->pattern);
}

bool parse_finalize(ParseCtx *ctx) {
    if (ctx->paren_stack_size > 0) {
        return false;
    }

    ctx->ast[ctx->node_ix].next = 0;
    return true;
}

RegexAst *newnode(ParseCtx *ctx, enum RegexType type, bool replace_empty) {
    if (ctx->size > 0 && replace_empty &&
        ctx->ast[ctx->size - 1].type == REGEX_EMPTY) {
        ctx->ast[ctx->size - 1].type = type;
        return &ctx->ast[ctx->size - 1];
    }
    if (ctx->size == ctx->capacity) {
        uint32_t cap = ctx->capacity * 2;
        RegexAst *new = Mem_realloc(ctx->ast, cap * sizeof(RegexAst));
        if (new == NULL) {
            return NULL;
        }
        ctx->ast = new;
        ctx->capacity = cap;
    }
    ctx->ast[ctx->size].type = type;
    ctx->ast[ctx->size].next = ctx->size + 1;
    ++(ctx->size);
    return &ctx->ast[ctx->size - 1];
}

bool paren_open(ParseCtx *ctx) {
    if (ctx->paren_stack_size == ctx->paren_stack_cap) {
        uint32_t cap = ctx->paren_stack_cap * 2;
        uint32_t *new = Mem_realloc(ctx->paren_stack, cap * sizeof(uint32_t));
        if (new == NULL) {
            return false;
        }
        ctx->paren_stack = new;
        ctx->paren_stack_cap = cap;
    }

    RegexAst *node = newnode(ctx, REGEX_PAREN, true);
    if (node == NULL) {
        return false;
    }
    uint32_t ix = node - ctx->ast;
    ctx->paren_stack[ctx->paren_stack_size] = ix;
    ++ctx->paren_stack_size;
    node->paren.node_ix = ctx->size;

    if (newnode(ctx, REGEX_EMPTY, false) == NULL) {
        return false;
    }
    ctx->node_ix = ctx->size - 1;

    return true;
}

bool paren_close(ParseCtx *ctx) {
    if (ctx->paren_stack_size == 0) {
        return false;
    }
    uint32_t ix = ctx->paren_stack[ctx->paren_stack_size - 1];
    RegexAst *open = &ctx->ast[ix];
    RegexAst *cur = &ctx->ast[ctx->node_ix];

    --ctx->paren_stack_size;
    cur->next = 0;
    open->next = ctx->size;
    ctx->node_ix = ix;

    return true;
}

bool literal_union_start(ParseCtx *ctx) {
    RegexAst *node = newnode(ctx, REGEX_LITERAL_UNION, true);
    if (node == NULL) {
        return false;
    }
    node->literal_union.str_ix = ctx->pattern.length;
    node->literal_union.size = 0;
    node->literal_union.not = false;
    ctx->node_ix = node - ctx->ast;
    return true;
}

bool literal_union_char(ParseCtx *ctx, char c) {
    if (!String_append(&ctx->pattern, c)) {
        return false;
    }
    ctx->ast[ctx->node_ix].literal_union.size += 1;
    return true;
}

bool append_char(ParseCtx *ctx, char c) {
    if (!String_append(&ctx->pattern, c)) {
        return false;
    }
    if (ctx->ast[ctx->node_ix].type == REGEX_LITERAL) {
        ctx->ast[ctx->node_ix].literal.size += 1;
    } else {
        RegexAst *node = newnode(ctx, REGEX_LITERAL, true);
        if (node == NULL) {
            return false;
        }
        node->literal.size = 1;
        node->literal.str_ix = ctx->pattern.length - 1;
        ctx->node_ix = node - ctx->ast;
    }
    return true;
}

bool repeat_last(ParseCtx *ctx, uint32_t min, uint32_t max,
                 bool split_literal) {
    if (split_literal && ctx->ast[ctx->node_ix].type == REGEX_LITERAL &&
        ctx->ast[ctx->node_ix].literal.size > 1) {
        uint32_t ix = ctx->pattern.length - 1;
        while (ix > ctx->ast[ctx->node_ix].literal.str_ix) {
            uint8_t c = (uint8_t) ctx->pattern.buffer[ix];
            if (c < 0x80 || c > 0xBF) {
                break;
            }
            --ix;
        }
        uint8_t* s = (uint8_t*)ctx->pattern.buffer + ix;
        if (!validate_utf8_seq(s, ctx->pattern.length - ix)) {
            // invalid unicode
            return false;
        }
        uint8_t len = utf8_len_table[s[0]];
        if (len != ctx->pattern.length - ix) {
            // should never happen
            return false;
        }
        // Split last literal to only repeat last character
        ctx->ast[ctx->node_ix].literal.size -= len;
        RegexAst *node = newnode(ctx, REGEX_LITERAL, false);
        if (node == NULL) {
            return false;
        }
        node->literal.size = len;
        node->literal.str_ix = ctx->pattern.length - len;
        ctx->node_ix = node - ctx->ast;
    }
    RegexAst *node = newnode(ctx, REGEX_REPEAT, false);
    if (node == NULL) {
        return false;
    }
    RegexAst tmp = *node;
    *node = ctx->ast[ctx->node_ix];
    node->next = 0;

    uint32_t node_ix = node - ctx->ast;
    ctx->ast[ctx->node_ix] = tmp;
    ctx->ast[ctx->node_ix].repeat.node_ix = node_ix;
    ctx->ast[ctx->node_ix].repeat.min = min;
    ctx->ast[ctx->node_ix].repeat.max = max;
    return true;
}

// REGEX_ERROR = out of memory
// REGEX_NO_MATCH = invalid pattern provided
// REGEX_MATCH = success
RegexResult ast_parse(ParseCtx *ctx, const char *pattern) {
    uint64_t len = strlen(pattern);
    if (len > INT32_MAX) {
        return REGEX_NO_MATCH;
    }

    if (!parse_init(ctx)) {
        return REGEX_ERROR;
    }

    uint64_t ix = 0;

    while (ix < len) {
        if (pattern[ix] == '\\') {
            if (ix + 1 >= len) {
                parse_free(ctx);
                return REGEX_NO_MATCH;
            }
            if (pattern[ix + 1] == '\\' || pattern[ix + 1] == '*' ||
                pattern[ix + 1] == '.' || pattern[ix + 1] == '[') {
                ++ix;
            } else if (pattern[ix + 1] == '(') {
                if (!paren_open(ctx)) {
                    parse_free(ctx);
                    return REGEX_ERROR;
                }
                ix += 2;
                continue;
            } else if (pattern[ix + 1] == ')') {
                if (ctx->paren_stack_size > 0) {
                    if (!paren_close(ctx)) {
                        parse_free(ctx);
                        return REGEX_NO_MATCH;
                    }
                    ix += 2;
                    continue;
                }
            } else if (pattern[ix + 1] == '{') {
                // \{n\} \{n,m\} \{n,\}
                ix += 2;
                uint64_t end_ix = ix;
                uint64_t comma_ix = ix - 1;
                wchar_t nbuf[25];
                memset(nbuf, 0, 25 * sizeof(wchar_t));
                while (1) {
                    if (end_ix >= len - 1 || end_ix - ix >= 24) {
                        parse_free(ctx);
                        return REGEX_NO_MATCH;
                    }
                    if (pattern[end_ix] == ',') {
                        if (comma_ix > ix || end_ix == ix) {
                            parse_free(ctx);
                            return REGEX_NO_MATCH;
                        }
                        comma_ix = end_ix;
                        nbuf[end_ix] = L'\0';
                    } else if (pattern[end_ix] == '\\') {
                        if (pattern[end_ix + 1] != '}' || end_ix == ix) {
                            parse_free(ctx);
                            return REGEX_NO_MATCH;
                        }
                        break;
                    } else if (pattern[end_ix] < '0' || pattern[end_ix] > '9') {
                        parse_free(ctx);
                        return REGEX_NO_MATCH;
                    } else {
                        nbuf[end_ix - ix] = pattern[end_ix];
                    }
                    ++end_ix;
                }
                uint64_t minr, maxr;
                if (!parse_uintw(nbuf, &minr, 10)) {
                    parse_free(ctx);
                    return REGEX_NO_MATCH;
                }
                bool no_max = false;
                if (comma_ix == end_ix - 1) {
                    maxr = 0;
                    no_max = true;
                } else if (comma_ix < ix) {
                    maxr = minr;
                } else {
                    if (!parse_uintw(nbuf + (comma_ix - ix) + 1, &maxr, 10)) {
                        parse_free(ctx);
                        return REGEX_NO_MATCH;
                    }
                }
                ix = end_ix + 2;
                if (ctx->ast[ctx->node_ix].type == REGEX_EMPTY ||
                    ctx->ast[ctx->node_ix].type == REGEX_REPEAT ||
                    minr > 4096 || maxr > 4096 || (maxr < minr && !no_max)) {
                    parse_free(ctx);
                    return REGEX_NO_MATCH;
                }
                if (maxr == 0 && !no_max) {
                    ctx->ast[ctx->node_ix].type = REGEX_EMPTY;
                } else if (!repeat_last(ctx, minr, maxr, true)) {
                    parse_free(ctx);
                    return REGEX_ERROR;
                }
                continue;
            } else {
                parse_free(ctx);
                return REGEX_NO_MATCH;
            }
        } else if (pattern[ix] == '*') {
            if (ctx->ast[ctx->node_ix].type != REGEX_EMPTY &&
                ctx->ast[ctx->node_ix].type != REGEX_REPEAT) {
                if (!repeat_last(ctx, 0, 0, true)) {
                    parse_free(ctx);
                    return REGEX_ERROR;
                }
                ++ix;
                continue;
            } else {
                parse_free(ctx);
                return REGEX_NO_MATCH;
            }
        } else if (pattern[ix] == '.') {
            RegexAst *node = newnode(ctx, REGEX_ANY, true);
            if (node == NULL) {
                parse_free(ctx);
                return REGEX_ERROR;
            }
            uint32_t node_ix = node - ctx->ast;
            ctx->node_ix = node_ix;
            ++ix;
            continue;
        } else if (pattern[ix] == '[') {
            if (ix + 1 >= len) {
                parse_free(ctx);
                return REGEX_NO_MATCH;
            }
            bool not = false;
            if (pattern[ix + 1] == '^') {
                not = true;
                ++ix;
                if (ix + 1 >= len) {
                    parse_free(ctx);
                    return REGEX_NO_MATCH;
                }
            }
            if (!literal_union_start(ctx)) {
                return REGEX_ERROR;
            }
            ctx->ast[ctx->node_ix].literal_union.not = not ;
            if (!literal_union_char(ctx, pattern[ix + 1])) {
                parse_free(ctx);
                return REGEX_ERROR;
            }
            ix += 2;
            bool last_range = false;
            while (1) {
                if (ix >= len) {
                    parse_free(ctx);
                    return REGEX_NO_MATCH;
                }
                if (pattern[ix] == ']') {
                    break;
                }
                if (pattern[ix] == '-') {
                    if (last_range || ix + 1 >= len) {
                        parse_free(ctx);
                        return REGEX_NO_MATCH;
                    }
                    if (pattern[ix + 1] == ']') {
                        if (!literal_union_char(ctx, '-')) {
                            parse_free(ctx);
                            return REGEX_ERROR;
                        }
                        ++ix;
                        break;
                    }
                    last_range = true;
                    char start = ctx->pattern.buffer[ctx->pattern.length - 1];
                    String_pop(&ctx->pattern, 1);
                    ctx->ast[ctx->node_ix].literal_union.size -= 1;

                    char end = pattern[ix + 1];
                    if (((start >= 'a' && start <= 'z') &&
                         (end >= 'a' && end <= 'z')) ||
                        ((start >= 'A' && start <= 'Z') &&
                         (end >= 'A' && end <= 'Z')) ||
                        ((start >= '0' && start <= '9') &&
                         (end >= '0' && end <= '9'))) {
                        for (char c = start; c <= end; ++c) {
                            if (!literal_union_char(ctx, c)) {
                                parse_free(ctx);
                                return REGEX_ERROR;
                            }
                        }
                    } else {
                        parse_free(ctx);
                        return REGEX_NO_MATCH;
                    }
                    ix += 2;
                } else {
                    last_range = false;
                    if (!literal_union_char(ctx, pattern[ix])) {
                        parse_free(ctx);
                        return REGEX_ERROR;
                    }
                    ++ix;
                }
            }
            ++ix;
            continue;
        }

        if (!append_char(ctx, pattern[ix])) {
            parse_free(ctx);
            return REGEX_ERROR;
        }
        ++ix;
    }

    if (!parse_finalize(ctx)) {
        parse_free(ctx);
        return REGEX_NO_MATCH;
    }

    return REGEX_MATCH;
}

static bool reserve(void **ptr, uint32_t *cap, uint32_t size,
                    size_t elem_size) {
    if (size >= *cap) {
        void *new_ptr = Mem_realloc(*ptr, (*cap) * 2 * elem_size);
        if (new_ptr == NULL) {
            return false;
        }
        *ptr = new_ptr;
        *cap = *cap * 2;
    }
    return true;
}
#define RESERVE(ptr, cap, size, type)                                          \
    reserve((void **)(ptr), cap, size, sizeof(type))

bool ast_to_nfa(ParseCtx *ctx, NFA *nfa) {
    uint32_t edge_count = 0;
    uint32_t edge_cap = 4;
    uint32_t node_count = 1;
    uint32_t node_cap = 4;

    EdgeNFA *edges = Mem_alloc(edge_cap * sizeof(EdgeNFA));
    if (edges == NULL) {
        return false;
    }
    NodeNFA *nodes = Mem_alloc(node_cap * sizeof(NodeNFA));
    if (nodes == NULL) {
        Mem_free(edges);
        return false;
    }

    struct StackNode {
        uint32_t ast_ix;
        uint32_t node_ix;

        uint32_t edge_ix;
        uint32_t done_repeats;
    };

    struct StackNode *parse_stack = Mem_alloc(4 * sizeof(struct StackNode));
    if (parse_stack == NULL) {
        goto fail;
    }
    uint32_t stack_size = 0;
    uint32_t stack_cap = 4;

    nodes[0].accept = false;
    nodes[0].edge_ix = 0;
    nodes[0].edge_count = 0;

    uint32_t ast_ix = 0;
    while (1) {
    loop:
        RegexAst *e = &ctx->ast[ast_ix];
        switch (e->type) {
        case REGEX_LITERAL_UNION:
        case REGEX_LITERAL:
        case REGEX_ANY:
            if (!RESERVE(&nodes, &node_cap, node_count + 1, NodeNFA)) {
                goto fail;
            }
            if (!RESERVE(&edges, &edge_cap, edge_count + 1, EdgeNFA)) {
                goto fail;
            }
            // Add edge to new node
            if (e->type == REGEX_LITERAL_UNION) {
                edges[edge_count].str_ix = e->literal_union.str_ix;
                edges[edge_count].str_size = e->literal_union.size;
                edges[edge_count].type =
                    e->literal_union.not ? NFA_EDGE_UNION_NOT : NFA_EDGE_UNION;
            } else if (e->type == REGEX_LITERAL) {
                edges[edge_count].str_ix = e->literal.str_ix;
                edges[edge_count].str_size = e->literal.size;
                edges[edge_count].type = NFA_EDGE_LITERAL;
            } else {
                edges[edge_count].type = NFA_EDGE_ANY;
            }
            edges[edge_count].from = node_count - 1;
            edges[edge_count].to = node_count;
            ++edge_count;
            nodes[node_count - 1].edge_count += 1;

            nodes[node_count].accept = false;
            nodes[node_count].edge_ix = edge_count;
            nodes[node_count].edge_count = 0;
            ++node_count;
            break;
        case REGEX_EMPTY:
            break;
        case REGEX_REPEAT: {
            if (!RESERVE(&parse_stack, &stack_cap, stack_size + 1,
                         struct StackNode)) {
                goto fail;
            }
            if (e->repeat.min == 0) {
                if (!RESERVE(&edges, &edge_cap, edge_count + 1, EdgeNFA)) {
                    goto fail;
                }
                nodes[node_count - 1].edge_count += 1;
                EdgeNFA edge = {NFA_EDGE_LITERAL, 0, 0, node_count - 1, 0};
                edges[edge_count] = edge;
                ++edge_count;
            }
            parse_stack[stack_size].edge_ix = edge_count - 1;
            parse_stack[stack_size].ast_ix = ast_ix;
            parse_stack[stack_size].node_ix = node_count - 1;
            parse_stack[stack_size].done_repeats = 1;
            ++stack_size;
            ast_ix = e->repeat.node_ix;
            goto loop;
        }
        case REGEX_PAREN: {
            if (!RESERVE(&parse_stack, &stack_cap, stack_size + 1,
                         struct StackNode)) {
                goto fail;
            }
            if (!RESERVE(&nodes, &node_cap, node_count + 1, NodeNFA)) {
                goto fail;
            }
            if (!RESERVE(&edges, &edge_cap, edge_count + 1, EdgeNFA)) {
                goto fail;
            }
            nodes[node_count - 1].edge_count += 1;
            EdgeNFA edge = {NFA_EDGE_LITERAL, 0, 0, node_count - 1, node_count};
            edges[edge_count] = edge;
            ++edge_count;
            nodes[node_count].accept = false;
            nodes[node_count].edge_ix = edge_count;
            nodes[node_count].edge_count = 0;
            ++node_count;
            parse_stack[stack_size].edge_ix = 0;
            parse_stack[stack_size].done_repeats = 0;
            parse_stack[stack_size].ast_ix = ast_ix;
            parse_stack[stack_size].node_ix = node_count - 1;
            ++stack_size;
            ast_ix = e->paren.node_ix;
            goto loop;
        }
        }
        bool done = false;
        while (e->next == 0) {
            if (stack_size == 0) {
                done = true;
                break;
            }
            ast_ix = parse_stack[stack_size - 1].ast_ix;
            uint32_t node_ix = parse_stack[stack_size - 1].node_ix;
            uint32_t edge_ix = parse_stack[stack_size - 1].edge_ix;
            uint32_t done_repeats = parse_stack[stack_size - 1].done_repeats;
            --stack_size;
            e = &ctx->ast[ast_ix];
            if (e->type == REGEX_REPEAT) {
                if (e->repeat.min > done_repeats) {
                    if (!RESERVE(&parse_stack, &stack_cap, stack_size + 1,
                                 struct StackNode)) {
                        goto fail;
                    }
                    parse_stack[stack_size].ast_ix = ast_ix;
                    parse_stack[stack_size].node_ix = node_count - 1;
                    parse_stack[stack_size].edge_ix = 0;
                    parse_stack[stack_size].done_repeats = done_repeats + 1;
                    ++stack_size;
                    ast_ix = e->repeat.node_ix;
                    goto loop;
                }
                if (e->repeat.min < done_repeats) {
                    edges[edge_ix].to = node_count - 1;
                }
                if (e->repeat.max == 0 && node_ix != node_count - 1) {
                    if (!RESERVE(&edges, &edge_cap, edge_count + 1, EdgeNFA)) {
                        goto fail;
                    }
                    nodes[node_count - 1].edge_count += 1;
                    EdgeNFA edge = {NFA_EDGE_LITERAL, 0, 0, node_count - 1,
                                    node_ix};
                    edges[edge_count] = edge;
                    ++edge_count;
                } else if (e->repeat.max > done_repeats &&
                           node_ix != node_count - 1) {
                    if (!RESERVE(&edges, &edge_cap, edge_count + 1, EdgeNFA)) {
                        goto fail;
                    }
                    if (!RESERVE(&parse_stack, &stack_cap, stack_size + 1,
                                 struct StackNode)) {
                        goto fail;
                    }
                    nodes[node_count - 1].edge_count += 1;
                    EdgeNFA edge = {NFA_EDGE_LITERAL, 0, 0, node_count - 1, 0};
                    edges[edge_count] = edge;
                    ++edge_count;
                    parse_stack[stack_size].ast_ix = ast_ix;
                    parse_stack[stack_size].node_ix = node_count - 1;
                    parse_stack[stack_size].edge_ix = edge_count - 1;
                    parse_stack[stack_size].done_repeats = done_repeats + 1;
                    ++stack_size;
                    ast_ix = e->repeat.node_ix;
                    goto loop;
                }
            } else if (e->type == REGEX_PAREN) {
                if (!RESERVE(&edges, &edge_cap, edge_count + 1, EdgeNFA)) {
                    goto fail;
                }
                if (!RESERVE(&nodes, &node_cap, node_count + 1, NodeNFA)) {
                    goto fail;
                }
                nodes[node_count - 1].edge_count += 1;
                EdgeNFA edge = {NFA_EDGE_LITERAL, 0, 0, node_count - 1,
                                node_count};
                edges[edge_count] = edge;
                ++edge_count;
                nodes[node_count].accept = false;
                nodes[node_count].edge_ix = edge_count;
                nodes[node_count].edge_count = 0;
                ++node_count;
            }
        }
        if (done) {
            break;
        }
        ast_ix = e->next;
    }
    nodes[node_count - 1].accept = true;

    nfa->nodes = nodes;
    nfa->edges = edges;
    nfa->node_count = node_count;
    nfa->edge_count = edge_count;
    nfa->edge_cap = edge_cap;
    nfa->node_cap = node_cap;
    Mem_free(parse_stack);
    return true;

fail:
    Mem_free(parse_stack);
    Mem_free(nodes);
    Mem_free(edges);
    return false;
}

int uint32_cmp(const void *a, const void *b) {
    uint32_t i1 = *(uint32_t *)a;
    uint32_t i2 = *(uint32_t *)b;
    return (i1 > i2) - (i1 < i2);
}

bool dfa_collect_state(uint32_t start, NFA *nfa, NodeSet *node_set,
                       EdgeSet *edge_set, uint8_t *visited, uint32_t **to_visit,
                       uint32_t *to_visit_cap) {
    memset(visited, 0, nfa->node_count * sizeof(uint8_t));
    uint32_t to_visit_size = 1;
    node_set->nodes = Mem_alloc(4 * sizeof(uint32_t));
    node_set->node_count = 0;
    uint32_t node_cap = 4;
    if (node_set->nodes == NULL) {
        return false;
    }
    edge_set->edges = Mem_alloc(4 * sizeof(uint32_t));
    edge_set->edge_count = 0;
    uint32_t edge_cap = 4;
    if (edge_set->edges == NULL) {
        Mem_free(node_set->nodes);
        return false;
    }
    (*to_visit)[0] = start;
    while (to_visit_size > 0) {
        uint32_t node_ix = (*to_visit)[to_visit_size - 1];
        to_visit_size -= 1;
        visited[node_ix] = 1;
        if (!RESERVE(&node_set->nodes, &node_cap, node_set->node_count + 1,
                     uint32_t)) {
            goto fail;
        }
        node_set->nodes[node_set->node_count] = node_ix;
        ++node_set->node_count;
        uint32_t count = edge_set->edge_count + nfa->nodes[node_ix].edge_count;
        if (!RESERVE(&edge_set->edges, &edge_cap, count, uint32_t)) {
            goto fail;
        }

        for (uint32_t i = 0; i < nfa->nodes[node_ix].edge_count; ++i) {
            uint32_t edge_ix = nfa->nodes[node_ix].edge_ix + i;
            if (nfa->edges[edge_ix].type == NFA_EDGE_LITERAL &&
                nfa->edges[edge_ix].str_size == 0) {
                if (!visited[nfa->edges[edge_ix].to]) {
                    if (!RESERVE(to_visit, to_visit_cap, to_visit_size + 1,
                                 uint32_t)) {
                        goto fail;
                    }
                    (*to_visit)[to_visit_size] = nfa->edges[edge_ix].to;
                    ++to_visit_size;
                }
            } else {
                edge_set->edges[edge_set->edge_count] = edge_ix;
                ++edge_set->edge_count;
            }
        }
    }

    qsort(node_set->nodes, node_set->node_count, sizeof(uint32_t), uint32_cmp);
    qsort(edge_set->edges, edge_set->edge_count, sizeof(uint32_t), uint32_cmp);

    return true;
fail:
    Mem_free(node_set->nodes);
    Mem_free(edge_set->edges);
    return false;
}

static bool merge_nodes(uint32_t **dest, uint32_t *dest_size, uint32_t *other,
                        uint32_t other_size) {
    if (other_size == 0) {
        return true;
    }
    uint32_t *res = Mem_alloc((*dest_size + other_size) * sizeof(uint32_t));
    if (res == NULL) {
        return false;
    }
    uint32_t node_count = 0;
    uint32_t d_ix = 0;
    uint32_t o_ix = 0;
    while (1) {
        if (d_ix >= *dest_size) {
            for (; o_ix < other_size; ++o_ix) {
                if (node_count == 0 || res[node_count - 1] != other[o_ix]) {
                    res[node_count] = other[o_ix];
                    ++node_count;
                }
            }
            break;
        }
        if (o_ix >= other_size) {
            for (; d_ix < *dest_size; ++d_ix) {
                if (node_count == 0 || res[node_count - 1] != (*dest)[d_ix]) {
                    res[node_count] = (*dest)[d_ix];
                    ++node_count;
                }
            }
            break;
        }
        uint32_t n;
        if ((*dest)[d_ix] < other[o_ix]) {
            n = (*dest)[d_ix];
            ++d_ix;
        } else {
            n = other[o_ix];
            ++o_ix;
        }
        if (node_count == 0 || res[node_count - 1] != n) {
            res[node_count] = n;
            ++node_count;
        }
    }
    Mem_free(*dest);
    *dest = res;
    *dest_size = node_count;
    return true;
}

bool dfa_get_state(NFA* nfa, DFABuilder *builder, NodeSet *nodes, EdgeSet *edges,
                   uint32_t *node_ix) {
    for (uint32_t i = 0; i < builder->node_count; ++i) {
        NodeSet *n = &builder->builders[i].nodes;
        if (n->node_count != nodes->node_count) {
            continue;
        }
        if (memcmp(n->nodes, nodes->nodes, n->node_count) == 0) {
            *node_ix = i;
            Mem_free(nodes->nodes);
            Mem_free(edges->edges);
            return true;
        }
    }
    if (builder->stack_size == builder->stack_cap) {
        uint32_t *new_stack = Mem_realloc(
            builder->stack, (builder->stack_cap * 2) * sizeof(uint32_t));
        if (new_stack == NULL) {
            return false;
        }
        builder->stack = new_stack;
        builder->stack_cap = builder->stack_cap * 2;
    }
    if (builder->node_cap == builder->node_count) {
        NodeDFA *new_nodes = Mem_realloc(
            builder->nodes, (builder->node_cap * 2) * sizeof(NodeDFA));
        if (new_nodes == NULL) {
            return false;
        }
        builder->nodes = new_nodes;
        NodeDFABuilder *new_builders =
            Mem_realloc(builder->builders,
                        (builder->node_cap * 2) * sizeof(NodeDFABuilder));
        if (new_builders == NULL) {
            return false;
        }
        builder->builders = new_builders;
        builder->node_cap = builder->node_cap * 2;
    }
    NodeDFABuilder *b = &builder->builders[builder->node_count];
    NodeDFA *node = &builder->nodes[builder->node_count];
    b->nodes = *nodes;
    b->edges = *edges;
    b->is_literal = false;
    b->ascii_edge_count = 0;
    b->ascii_edge_cap = 4;
    b->utf8_edge_cap = 0;
    b->in_edge_count = 0;
    node->literal_node = false;
    node->accept = false;
    node->ascii_edges = Mem_alloc(4 * sizeof(uint32_t));
    if (node->ascii_edges == NULL) {
        return false;
    }
    node->utf8_edges = NULL;
    node->utf8_edge_count = 0;
    node->default_edge = DFA_REJECT_NODE;

    *node_ix = builder->node_count;
    builder->node_count += 1;
    builder->stack[builder->stack_size] = builder->node_count - 1;
    builder->stack_size += 1;

    for (uint32_t ix = 0; ix < nodes->node_count; ++ix) {
        if (nfa->nodes[nodes->nodes[ix]].accept) {
            node->accept = true;
            break;
        }
    }

    return true;
}

// NFA edges has already been validated for valid utf8
// bytes: 1-4 bytes of valid utf8
bool dfa_char_state(NFA *nfa, char *chars, DFABuilder *builder,
                    NodeDFABuilder *b, uint8_t* bytes, uint32_t *ix,
                    uint32_t *matching_edges) {
    NodeSet node_set;
    EdgeSet edge_set;
    node_set.nodes = NULL;
    node_set.node_count = 0;
    edge_set.edges = NULL;
    edge_set.edge_count = 0;

    NodeSet nodes;
    EdgeSet edges;
    *matching_edges = 0;
    uint8_t l = utf8_len_table[bytes[0]];

    for (uint32_t i = 0; i < b->edges.edge_count; ++i) {
        EdgeNFA *e = &nfa->edges[b->edges.edges[i]];
        uint8_t* e_chars = (uint8_t*)chars + e->str_ix;
        if (e->type == NFA_EDGE_LITERAL) {
            if (utf8_len_table[e_chars[0]] != l) {
                continue;
            }
            if (e_chars[0] != bytes[0] || (l > 1 && e_chars[1] != bytes[1]) ||
                (l > 2 && e_chars[2] != bytes[2]) ||
                (l > 3 && e_chars[3] != bytes[3])) {
                continue;
            }
        } else if (e->type == NFA_EDGE_UNION || e->type == NFA_EDGE_UNION_NOT) {
            bool found = false;
            for (uint32_t ix = 0; ix < e->str_size;
                 ix += utf8_len_table[e_chars[ix]]) {
                if (utf8_len_table[e_chars[ix]] != l) {
                    continue;
                }
                if (e_chars[ix] != bytes[0] ||
                    (l > 1 && e_chars[ix + 1] != bytes[1]) ||
                    (l > 2 && e_chars[ix + 2] != bytes[2]) ||
                    (l > 3 && e_chars[ix + 3] != bytes[3])) {
                    continue;
                }
                found = true;
                break;
            }
            if (!found) {
                if (e->type == NFA_EDGE_UNION) {
                    continue;
                }
            } else if (e->type == NFA_EDGE_UNION_NOT) {
                continue;
            }
        }
        ++(*matching_edges);
        if (!dfa_collect_state(e->to, nfa, &nodes, &edges, builder->visited,
                               &builder->to_visit, &builder->to_visit_cap)) {
            return false;
        }
        bool merge = merge_nodes(&node_set.nodes, &node_set.node_count,
                                 nodes.nodes, nodes.node_count) &&
                     merge_nodes(&edge_set.edges, &edge_set.edge_count,
                                 edges.edges, edges.edge_count);
        Mem_free(nodes.nodes);
        Mem_free(edges.edges);
        if (!merge) {
            Mem_free(node_set.nodes);
            Mem_free(edge_set.edges);
            return false;
        }
    }
    if (node_set.node_count == 0) {
        // not reachable
        *ix = DFA_REJECT_NODE;
        Mem_free(node_set.nodes);
        Mem_free(edge_set.edges);
        return true;
    }

    if (!dfa_get_state(nfa, builder, &node_set, &edge_set, ix)) {
        Mem_free(node_set.nodes);
        Mem_free(edge_set.edges);
        return false;
    }
    return true;
}

bool dfa_add_utf8_edge(NodeDFA *node, NodeDFABuilder *builder,
                            uint8_t* bytes, uint32_t dest) {
    uint8_t len = utf8_len_table[bytes[0]];
    for (uint32_t ix = 0; ix < node->utf8_edge_count; ++ix) {
        if (utf8_len_table[node->utf8_edges[ix].bytes[0]] == len) {
            if (memcmp(bytes, node->utf8_edges[ix].bytes, len) == 0) {
                return true;
            }
        }
    }
    if (node->utf8_edge_count == builder->utf8_edge_cap) {
        if (builder->utf8_edge_cap == 0) {
            node->utf8_edges = Mem_alloc(2 * sizeof(Utf8Edge));
            if (node->utf8_edges == NULL) {
                return false;
            }
            builder->utf8_edge_cap = 2;
        } else {
            Utf8Edge *edges =
                Mem_realloc(node->utf8_edges,
                            2 * builder->utf8_edge_cap * sizeof(Utf8Edge));
            if (edges == NULL) {
                return false;
            }
            node->utf8_edges = edges;
            builder->utf8_edge_cap = builder->utf8_edge_cap * 2;
        }
    }
    node->utf8_edges[node->utf8_edge_count].bytes[0] = bytes[0];
    node->utf8_edges[node->utf8_edge_count].bytes[1] = len > 1 ? bytes[1] : 0;
    node->utf8_edges[node->utf8_edge_count].bytes[2] = len > 2 ? bytes[2] : 0;
    node->utf8_edges[node->utf8_edge_count].bytes[3] = len > 3 ? bytes[3] : 0;
    node->utf8_edges[node->utf8_edge_count].node_ix = dest;
    ++node->utf8_edge_count;
    return true;
}

bool dfa_add_edge(DFABuilder *b, NodeDFA *node, NodeDFABuilder *builder,
                  uint8_t* utf8, uint32_t dest) {
    if (builder->ascii_edge_count == 256 && utf8[0] <= 127) {
        return false;
    }
    if (dest != DFA_REJECT_NODE) {
        b->builders[dest].in_edge_count += 1;
    }
    if (utf8[0] > 127) {
        return dfa_add_utf8_edge(node, builder, utf8, dest);
    }
    for (uint32_t ix = 0; ix < builder->ascii_edge_count; ++ix) {
        if (node->ascii_edges[ix] == dest) {
            node->ascii[utf8[0]] = ix;
            return true;
        }
    }
    if (builder->ascii_edge_count == builder->ascii_edge_cap) {
        uint32_t *new_edges = Mem_realloc(
            node->ascii_edges, 2 * builder->ascii_edge_cap * sizeof(uint32_t));
        if (new_edges == NULL) {
            return false;
        }
        node->ascii_edges = new_edges;
        builder->ascii_edge_cap *= 2;
    }
    node->ascii_edges[builder->ascii_edge_count] = dest;
    node->ascii[utf8[0]] = builder->ascii_edge_count;
    ++builder->ascii_edge_count;
    return true;
}

bool dfa_default_state(NFA *nfa, char *chars, DFABuilder *builder,
                       NodeDFABuilder *b, uint32_t *ix) {
    NodeSet node_set;
    EdgeSet edge_set;
    node_set.nodes = NULL;
    node_set.node_count = 0;
    edge_set.edges = NULL;
    edge_set.edge_count = 0;

    NodeSet nodes;
    EdgeSet edges;

    for (uint32_t i = 0; i < b->edges.edge_count; ++i) {
        EdgeNFA *e = &nfa->edges[b->edges.edges[i]];
        if (e->type == NFA_EDGE_UNION_NOT || e->type == NFA_EDGE_ANY) {
            if (!dfa_collect_state(e->to, nfa, &nodes, &edges, builder->visited,
                                   &builder->to_visit,
                                   &builder->to_visit_cap)) {
                return false;
            }
            bool merge = merge_nodes(&node_set.nodes, &node_set.node_count,
                                     nodes.nodes, nodes.node_count) &&
                         merge_nodes(&edge_set.edges, &edge_set.edge_count,
                                     edges.edges, edges.edge_count);
            Mem_free(nodes.nodes);
            Mem_free(edges.edges);
            if (!merge) {
                Mem_free(node_set.nodes);
                Mem_free(edge_set.edges);
                return false;
            }
        }
    }
    if (node_set.node_count == 0) {
        // Default state not reachable
        *ix = DFA_REJECT_NODE;
        Mem_free(node_set.nodes);
        Mem_free(edge_set.edges);
        return true;
    }

    if (!dfa_get_state(nfa, builder, &node_set, &edge_set, ix)) {
        Mem_free(node_set.nodes);
        Mem_free(edge_set.edges);
        return false;
    }
    return true;
}

bool nfa_split_literals(NFA *nfa, char *chars) {
    uint32_t *node_cap = &nfa->node_cap;
    uint32_t *edge_cap = &nfa->edge_cap;
    for (uint64_t node_ix = 0; node_ix < nfa->node_count; ++node_ix) {
        for (uint64_t i = 0; i < nfa->nodes[node_ix].edge_count; ++i) {
            uint64_t edge_ix = nfa->nodes[node_ix].edge_ix + i;
            if (nfa->edges[edge_ix].type != NFA_EDGE_LITERAL ||
                nfa->edges[edge_ix].str_size == 0) {
                continue;
            }
            uint8_t* str = (uint8_t*)&chars[nfa->edges[edge_ix].str_ix];
            if (!validate_utf8_seq(str, nfa->edges[edge_ix].str_size)) {
                return false;
            }
            uint8_t clen = utf8_len_table[str[0]];
            if (clen == nfa->edges[edge_ix].str_size) {
                continue;
            }
            if (!RESERVE(&nfa->nodes, node_cap, nfa->node_count + 1, NodeNFA)) {
                return false;
            }
            if (!RESERVE(&nfa->edges, edge_cap, nfa->edge_count + 1, EdgeNFA)) {
                return false;
            }

            EdgeNFA edge = {NFA_EDGE_LITERAL, nfa->edges[edge_ix].str_ix + clen,
                            nfa->edges[edge_ix].str_size - clen, nfa->node_count,
                            nfa->edges[edge_ix].to};
            nfa->edges[nfa->edge_count] = edge;
            nfa->edge_count += 1;
            nfa->nodes[nfa->node_count].accept = false;
            nfa->nodes[nfa->node_count].edge_ix = nfa->edge_count - 1;
            nfa->nodes[nfa->node_count].edge_count = 1;
            nfa->edges[edge_ix].to = nfa->node_count;
            nfa->edges[edge_ix].str_size = clen;
            uint64_t new_node = nfa->node_count;
            nfa->node_count += 1;

            for (uint64_t j = 0; j < nfa->nodes[node_ix].edge_count; ++j) {
                uint64_t e2_ix = nfa->nodes[node_ix].edge_ix + j;
                if (edge_ix == e2_ix) {
                    continue;
                }
                EdgeNFA *e2 = &nfa->edges[e2_ix];
                if (e2->type != NFA_EDGE_LITERAL || e2->str_size == 0) {
                    continue;
                }
                uint8_t* e2_str = (uint8_t*)chars + e2->str_ix;
                if (!validate_utf8_seq((uint8_t*)chars + e2->str_ix,
                                       e2->str_size)) {
                    return false;
                }
                if (clen != utf8_len_table[e2_str[0]]) {
                    continue;
                }
                if (str[0] != e2_str[0] ||
                    (clen > 1 && str[1] != e2_str[1]) ||
                    (clen > 2 && str[2] != e2_str[2]) ||
                    (clen > 3 && str[3] != e2_str[3])) {
                    continue;
                }

                if (!RESERVE(&nfa->edges, edge_cap, nfa->edge_count + 1,
                             EdgeNFA)) {
                    return false;
                }
                EdgeNFA edge = {NFA_EDGE_LITERAL, nfa->edges[e2_ix].str_ix + clen,
                                nfa->edges[e2_ix].str_size - clen, nfa->node_count,
                                nfa->edges[e2_ix].to};
                nfa->edges[nfa->edge_count] = edge;
                nfa->edge_count += 1;
                nfa->nodes[new_node].edge_count += 1;
                // del e2...
                uint32_t count = nfa->nodes[node_ix].edge_count;
                memmove(nfa->edges + e2_ix, nfa->edges + e2_ix + 1,
                        (count - e2_ix - 1) * sizeof(EdgeNFA));
                if (edge_ix > e2_ix) {
                    --i;
                    --edge_ix;
                }
                --j;
                --e2_ix;
            }
        }
    }
    return true;
}

static bool dfa_builder_create(DFABuilder *b, uint32_t node_count) {
    b->builders = Mem_alloc(4 * sizeof(NodeDFABuilder));
    if (b->builders == NULL) {
        return false;
    }
    b->nodes = Mem_alloc(4 * sizeof(NodeDFA));
    if (b->nodes == NULL) {
        Mem_free(b->builders);
        return false;
    }
    b->node_count = 0;
    b->node_cap = 4;
    b->to_visit = Mem_alloc(4 * sizeof(uint32_t));
    if (b->to_visit == NULL) {
        Mem_free(b->builders);
        Mem_free(b->nodes);
        return false;
    }
    b->to_visit_cap = 4;
    b->visited = Mem_alloc(node_count * sizeof(uint8_t));
    if (b->visited == NULL) {
        Mem_free(b->builders);
        Mem_free(b->nodes);
        Mem_free(b->to_visit);
        return false;
    }
    b->stack = Mem_alloc(4 * sizeof(uint32_t));
    if (b->stack == NULL) {
        Mem_free(b->builders);
        Mem_free(b->nodes);
        Mem_free(b->to_visit);
        Mem_free(b->visited);
        return false;
    }
    b->stack_size = 0;
    b->stack_cap = 4;
    return true;
}

void dfa_builder_free(DFABuilder *b) {
    Mem_free(b->stack);
    Mem_free(b->to_visit);
    Mem_free(b->visited);
    for (uint32_t i = 0; i < b->node_count; ++i) {
        if (b->nodes[i].literal_node) {
            continue;
        }
        Mem_free(b->builders[i].edges.edges);
        Mem_free(b->builders[i].nodes.nodes);
        if (b->builders[i].utf8_edge_cap > 0) {
            Mem_free(b->nodes[i].utf8_edges);
        }
        Mem_free(b->nodes[i].ascii_edges);
    }
    Mem_free(b->nodes);
    Mem_free(b->builders);
}

bool dfa_single_out_edge(DFABuilder *builder, uint32_t node_ix, uint32_t *dest,
                         uint8_t *edge) {
    if (builder->nodes[node_ix].default_edge != DFA_REJECT_NODE) {
        return false;
    }
    if (builder->builders[node_ix].ascii_edge_count +
            builder->nodes[node_ix].utf8_edge_count >
        2) {
        return false;
    }
    if (builder->nodes[node_ix].utf8_edge_count > 0) {
        *dest = builder->nodes[node_ix].utf8_edges[0].node_ix;
        uint8_t* bytes = builder->nodes[node_ix].utf8_edges[0].bytes;
        for (uint32_t ix = 0; ix < utf8_len_table[bytes[0]]; ++ix) {
            edge[ix] = bytes[ix];
        }
        return *dest != DFA_REJECT_NODE;
    }
    for (uint32_t ix = 0; ix < 128; ++ix) {
        if (builder->nodes[node_ix].ascii[ix] > 0) {
            *edge = ix;
            ++ix;
            for (; ix < 128; ++ix) {
                if (builder->nodes[node_ix].ascii[ix] > 0) {
                    return false;
                }
            }
            *dest = builder->nodes[node_ix].ascii_edges[1];
            return *dest != DFA_REJECT_NODE;
        }
    }
    return false;
}

void dfa_node_mark_literal(NodeDFA *node, NodeDFABuilder *builder) {
    if (builder->is_literal) {
        return;
    }
    builder->is_literal = true;
    Mem_free(node->ascii_edges);
    Mem_free(node->utf8_edges);
}

uint32_t dfa_minlen(DFABuilder* b) {
    NodeDFA* dfa = b->nodes;
    uint32_t* visited = Mem_alloc(b->node_count * sizeof(uint32_t));
    if (visited == NULL) {
        return UINT32_MAX;
    }
    memset(visited, 0, b->node_count * sizeof(uint32_t));
    struct Node {
        uint32_t ix;
        uint32_t cost;
    };
    struct Node* to_visit = Mem_alloc(4 * sizeof(struct Node));
    if (to_visit == NULL) {
        Mem_free(visited);
        return UINT32_MAX;
    }
    uint32_t to_visit_size = 1;
    uint32_t to_visit_cap = 4;
    uint32_t cost = UINT32_MAX;

    to_visit[0].cost = 0;
    to_visit[0].ix = 0;

    while (to_visit_size > 0) {
        --to_visit_size;
        struct Node n = to_visit[to_visit_size];
        if (dfa[n.ix].accept) {
            if (n.cost < cost) {
                cost = n.cost;
            }
        }

        for (uint32_t ix = 0; ix < b->builders[n.ix].ascii_edge_count; ++ix) {
            if (dfa[n.ix].ascii_edges[ix] == DFA_REJECT_NODE) {
                continue;
            }
            uint32_t cost = visited[dfa[n.ix].ascii_edges[ix]];
            if (cost == 0 || cost > n.cost + 1) {
                if (!RESERVE(&to_visit, &to_visit_cap, to_visit_size + 1, struct Node)) {
                    goto end;
                }
                visited[dfa[n.ix].ascii_edges[ix]] = n.cost + 1;
                to_visit[to_visit_size].ix = dfa[n.ix].ascii_edges[ix];
                to_visit[to_visit_size].cost = n.cost + 1;
                ++to_visit_size;
            }
        }
        for (uint32_t ix = 0; ix < dfa[n.ix].utf8_edge_count; ++ix) {
            if (dfa[n.ix].utf8_edges[ix].node_ix == DFA_REJECT_NODE) {
                continue;
            }
            uint32_t cost = visited[dfa[n.ix].utf8_edges[ix].node_ix];
            if (cost == 0 || cost > n.cost + 1) {
                if (!RESERVE(&to_visit, &to_visit_cap, to_visit_size + 1, struct Node)) {
                    goto end;
                }
                visited[dfa[n.ix].utf8_edges[ix].node_ix] = n.cost + 1;
                to_visit[to_visit_size].ix = dfa[n.ix].utf8_edges[ix].node_ix;
                to_visit[to_visit_size].cost = n.cost + 1;
                ++to_visit_size;
            }
        }
    }
end:
    Mem_free(to_visit);
    Mem_free(visited);
    return cost;
}

bool dfa_finalize(DFABuilder *builder) {
    builder->minlen = dfa_minlen(builder);

    for (uint32_t ix = 0; ix < builder->node_count; ++ix) {
        Mem_free(builder->builders[ix].nodes.nodes);
        Mem_free(builder->builders[ix].edges.edges);
        if (builder->builders[ix].is_literal) {
            continue;
        }
        uint32_t dest_node;
        uint8_t first_edge[4] = {0, 0, 0, 0};
        if (!dfa_single_out_edge(builder, ix, &dest_node, first_edge)) {
            continue;
        }
        if (builder->builders[dest_node].in_edge_count != 1) {
            continue;
        }
        NodeDFA *n = &builder->nodes[ix];
        if (n->accept != builder->nodes[dest_node].accept) {
            continue;
        }

        uint8_t l = utf8_len_table[first_edge[0]];
        if (builder->nodes[dest_node].literal_node) {
            if (builder->nodes[dest_node].literal.str_len + l > 144) {
                continue;
            }
            dfa_node_mark_literal(n, &builder->builders[ix]);
            n->literal_node = true;
            builder->nodes[dest_node].literal_node = false;
            n->literal.str_len = builder->nodes[dest_node].literal.str_len + l;
            for (uint8_t i = 0; i < l; ++i) {
                n->literal.str[i] = first_edge[i];
            }
            memcpy(n->literal.str + l, builder->nodes[dest_node].literal.str,
                   n->literal.str_len - l);
            n->literal.node_ix = builder->nodes[dest_node].literal.node_ix;
            continue;
        }
        uint32_t next_dest;
        uint8_t c[4] = {0, 0, 0, 0};
        if (!dfa_single_out_edge(builder, dest_node, &next_dest, c)) {
            continue;
        }
        dfa_node_mark_literal(n, &builder->builders[ix]);
        dfa_node_mark_literal(&builder->nodes[dest_node],
                              &builder->builders[dest_node]);
        n->literal_node = true;
        n->literal.node_ix = next_dest;
        for (uint8_t i = 0; i < l; ++i) {
            n->literal.str[i] = first_edge[i];
        }
        for (uint8_t i = 0; i < utf8_len_table[c[0]]; ++i) {
            n->literal.str[l + i] = c[i];
        }
        n->literal.str_len = l + utf8_len_table[c[0]];
        builder->builders[ix].is_literal = true;
        builder->builders[dest_node].is_literal = true;

        while (1) {
            if (builder->builders[next_dest].in_edge_count != 1) {
                break;
            }
            uint32_t dest = next_dest;
            if (n->accept != builder->nodes[dest].accept) {
                break;
            }
            if (!dfa_single_out_edge(builder, next_dest, &next_dest, c)) {
                break;
            }
            uint8_t l = utf8_len_table[c[0]];
            if (n->literal.str_len + l > 144) {
                break;
            }

            dfa_node_mark_literal(&builder->nodes[dest],
                                  &builder->builders[dest]);
            n->literal.node_ix = next_dest;
            for (uint8_t i = 0; i < l; ++i) {
                n->literal.str[n->literal.str_len + i] = c[i];
            }
            n->literal.str_len += l;
        }
    }

    Mem_free(builder->visited);
    Mem_free(builder->to_visit);
    Mem_free(builder->stack);

    uint32_t node_count = 0;
    uint32_t *node_map = Mem_alloc(builder->node_count * sizeof(uint32_t));
    if (node_map != NULL) {
        for (uint32_t ix = 0; ix < builder->node_count; ++ix) {
            node_map[ix] = node_count;
            if (builder->builders[ix].is_literal) {
                if (!builder->nodes[ix].literal_node) {
                    continue;
                }
            }
            ++node_count;
        }
    }
    NodeDFA *nodes = Mem_alloc(node_count * sizeof(NodeDFA));
    if (nodes == NULL || node_map == NULL) {
        Mem_free(node_map);
        Mem_free(nodes);
        for (uint32_t ix = 0; ix < builder->node_count; ++ix) {
            if (builder->builders[ix].is_literal) {
                continue;
            }
            Mem_free(builder->nodes[ix].ascii_edges);
            Mem_free(builder->nodes[ix].utf8_edges);
        }
        Mem_free(builder->builders);
        Mem_free(builder->nodes);
        return false;
    }

    for (uint32_t ix = 0; ix < builder->node_count; ++ix) {
        uint32_t new_ix = node_map[ix];
        NodeDFA *n = &builder->nodes[ix];
        if (n->literal_node) {
            memcpy(nodes + new_ix, n, sizeof(NodeDFA));
            if (n->literal.node_ix != DFA_REJECT_NODE) {
                nodes[new_ix].literal.node_ix = node_map[n->literal.node_ix];
            }
            continue;
        }
        if (builder->builders[ix].is_literal) {
            continue;
        }
        memcpy(nodes + new_ix, n, sizeof(NodeDFA));
        if (n->default_edge != DFA_REJECT_NODE) {
            nodes[new_ix].default_edge = node_map[n->default_edge];
        }
        for (uint32_t i = 0; i < n->utf8_edge_count; ++i) {
            if (n->utf8_edges[i].node_ix == DFA_REJECT_NODE) {
                continue;
            }
            nodes[new_ix].utf8_edges[i].node_ix =
                node_map[n->utf8_edges[i].node_ix];
        }
        // OPTIM
        /*for (uint32_t i = 0; i < 128; ++i) {
            if (n->ascii_edges[n->ascii[i]] == DFA_REJECT_NODE) {
                nodes[new_ix].ascii[i] = DFA_REJECT_NODE;
            } else {
                nodes[new_ix].ascii[i] = node_map[n->ascii_edges[n->ascii[i]]];
            }
        }*/
        // END OPTIM
        
        for (uint32_t i = 0; i < builder->builders[ix].ascii_edge_count; ++i) {
            if (n->ascii_edges[i] == DFA_REJECT_NODE) {
                continue;
            }
            nodes[new_ix].ascii_edges[i] = node_map[n->ascii_edges[i]];
        }
        
    }

    Mem_free(builder->builders);
    Mem_free(builder->nodes);
    builder->nodes = nodes;
    builder->node_count = node_count;

    return true;
}

bool nfa_copy(NFA* in, NFA* out) {
    out->nodes = Mem_alloc(in->node_count * sizeof(NodeNFA));
    if (out->nodes == NULL) {
        return false;
    }
    out->edges = Mem_alloc(in->edge_count * sizeof(EdgeNFA));
    if (out->edges == NULL) {
        Mem_free(out->nodes);
        return false;
    }
    memcpy(out->nodes, in->nodes, in->node_count * sizeof(NodeNFA));
    memcpy(out->edges, in->edges, in->edge_count * sizeof(EdgeNFA));
    out->edge_count = in->edge_count;
    out->edge_cap = in->edge_count;
    out->node_count = in->node_count;
    out->node_cap = in->node_count;
    return true;
}

RegexResult nfa_to_dfa(NFA *nfa, char *chars, NodeDFA **dfa,
                       uint32_t* dfa_nodes, uint32_t* minlen) {
    if (!nfa_split_literals(nfa, chars)) {
        return REGEX_ERROR;
    }

    DFABuilder b;
    if (!dfa_builder_create(&b, nfa->node_count)) {
        return REGEX_ERROR;
    }

    NodeSet nodes;
    EdgeSet edges;
    if (!dfa_collect_state(0, nfa, &nodes, &edges, b.visited, &b.to_visit,
                           &b.to_visit_cap)) {
        dfa_builder_free(&b);
        return REGEX_ERROR;
    }
    uint32_t ix;
    if (!dfa_get_state(nfa, &b, &nodes, &edges, &ix)) {
        dfa_builder_free(&b);
        return REGEX_ERROR;
    }

    while (b.stack_size > 0) {
        --b.stack_size;
        uint32_t node_ix = b.stack[b.stack_size];
        uint32_t default_ix;
        if (!dfa_default_state(nfa, chars, &b, &b.builders[node_ix],
                               &default_ix)) {
            dfa_builder_free(&b);
            return REGEX_ERROR;
        }
        uint8_t null_bytes[] = {0, 0, 0, 0};
        if (!dfa_add_edge(&b, &b.nodes[node_ix], &b.builders[node_ix],
                          null_bytes, default_ix)) {
            dfa_builder_free(&b);
            return REGEX_ERROR;
        }

        memset(&b.nodes[node_ix].ascii, 0, 128);
        b.nodes[node_ix].default_edge = default_ix;
        for (uint32_t i = 0; i < b.builders[node_ix].edges.edge_count; ++i) {
            uint32_t edge_ix = b.builders[node_ix].edges.edges[i];
            EdgeNFA *e = &nfa->edges[edge_ix];
            uint32_t dest_ix;
            uint32_t matching_edges;
            if (e->type == NFA_EDGE_LITERAL) {
                uint8_t* bytes = (uint8_t*)chars + e->str_ix;
                if (!validate_utf8_seq(bytes, e->str_size)) {
                    dfa_builder_free(&b);
                    return REGEX_ERROR;
                }
                if (!dfa_char_state(nfa, chars, &b, &b.builders[node_ix],
                                    bytes, &dest_ix,
                                    &matching_edges) ||
                    !dfa_add_edge(&b, &b.nodes[node_ix], &b.builders[node_ix],
                                  bytes, dest_ix)) {
                    dfa_builder_free(&b);
                    return REGEX_ERROR;
                }
            } else if (e->type == NFA_EDGE_UNION_NOT ||
                       e->type == NFA_EDGE_UNION) {
                for (uint32_t ix = 0; ix < e->str_size;) {
                    uint8_t* bytes = (uint8_t*)chars + e->str_ix + ix;
                    if (!validate_utf8_seq(bytes, e->str_size - ix)) {
                        dfa_builder_free(&b);
                        return REGEX_ERROR;
                    }
                    ix += utf8_len_table[bytes[0]];
                    if (!dfa_char_state(nfa, chars, &b, &b.builders[node_ix],
                                        bytes, &dest_ix, &matching_edges) ||
                        !dfa_add_edge(&b, &b.nodes[node_ix],
                                      &b.builders[node_ix], bytes, dest_ix)) {
                        dfa_builder_free(&b);
                        return REGEX_ERROR;
                    }
                }
            }
        }
    }

    // Add implicit start edge to count
    b.builders[0].in_edge_count += 1;
    if (!dfa_finalize(&b)) {
        return REGEX_ERROR;
    }

    *dfa = b.nodes;
    *minlen = b.minlen;
    *dfa_nodes = b.node_count;

    return REGEX_MATCH;
}

// Basic posix
Regex *Regex_compile(const char *pattern) {
    ParseCtx ctx;

    RegexResult res = ast_parse(&ctx, pattern);
    if (res != REGEX_MATCH) {
        return NULL;
    }

    NFA nfa;
    if (!ast_to_nfa(&ctx, &nfa)) {
        parse_free(&ctx);
        return NULL;
    }

    Mem_free(ctx.ast);
    Mem_free(ctx.paren_stack);

    NFA nfa_cpy;
    NodeDFA* dfa = NULL;
    uint32_t minlen;
    uint32_t dfa_nodes;
    if (nfa_copy(&nfa, &nfa_cpy)) {
        if (nfa_to_dfa(&nfa_cpy, ctx.pattern.buffer, &dfa, &dfa_nodes, &minlen) != REGEX_MATCH) {
            dfa = NULL;
            minlen = UINT32_MAX;
            dfa_nodes = 0;
        }
        Mem_free(nfa_cpy.nodes);
        Mem_free(nfa_cpy.edges);
    }


    Regex *regex = Mem_alloc(sizeof(Regex));
    if (regex == NULL) {
        String_free(&ctx.pattern);
        Mem_free(nfa.nodes);
        Mem_free(nfa.edges);
        return NULL;
    }
    regex->chars = ctx.pattern.buffer;
    regex->nfa = nfa;
    regex->dfa = dfa;
    regex->minlen = minlen;
    regex->dfa_nodes = dfa_nodes;

    return regex;
}

void Regex_free(Regex *regex) {
    Mem_free(regex->nfa.nodes);
    Mem_free(regex->nfa.edges);
    String s;
    s.buffer = regex->chars;
    String_free(&s);
    if (regex->dfa != NULL) {
        for (uint32_t i = 0; i < regex->dfa_nodes; ++i) {
            if (regex->dfa[i].literal_node) {
                continue;
            }
            Mem_free(regex->dfa[i].utf8_edges);
            Mem_free(regex->dfa[i].ascii_edges);
        }
        Mem_free(regex->dfa);
    }
    Mem_free(regex);
}

bool matches_edge(Regex *regex, const char *str, uint64_t len, uint64_t *str_ix,
                  uint64_t edge_ix) {
    if (regex->nfa.edges[edge_ix].type == NFA_EDGE_ANY) {
        if (*str_ix < len) {
            ++(*str_ix);
            return true;
        }
        return false;
    }
    char *edge = regex->chars + regex->nfa.edges[edge_ix].str_ix;
    uint64_t edge_len = regex->nfa.edges[edge_ix].str_size;
    if (regex->nfa.edges[edge_ix].type == NFA_EDGE_LITERAL) {
        if (edge_len > len - *str_ix) {
            return false;
        }
        if (memcmp(str + *str_ix, edge, edge_len) == 0) {
            *str_ix += edge_len;
            return true;
        }
        return false;
    } else if (regex->nfa.edges[edge_ix].type == NFA_EDGE_UNION) {
        if (*str_ix >= len) {
            return false;
        }
        if (memchr(edge, str[*str_ix], edge_len) != NULL) {
            *str_ix += 1;
            return true;
        }
        return false;
    } else {
        if (*str_ix >= len) {
            return false;
        }
        if (memchr(edge, str[*str_ix], edge_len) == NULL) {
            *str_ix += 1;
            return true;
        }
        return false;
    }
}

RegexResult Regex_fullmatch_dfa(Regex* regex, const char* str, uint64_t len) {
    NodeDFA* dfa = regex->dfa;
    uint64_t node_ix = 0;
    for (uint64_t ix = 0; ix < len;) {
        if (dfa[node_ix].literal_node) {
            if (len - ix < dfa[node_ix].literal.str_len) {
                return REGEX_NO_MATCH;
            }
            if (memcmp(str + ix, dfa[node_ix].literal.str, dfa[node_ix].literal.str_len) != 0) {
                return REGEX_NO_MATCH;
            }
            ix += dfa[node_ix].literal.str_len;
            node_ix = dfa[node_ix].literal.node_ix;
        } else {
            uint8_t c = (uint8_t)str[ix];
            if (c < 128) {
                ++ix;
                node_ix = dfa[node_ix].ascii_edges[dfa[node_ix].ascii[c]];
            } else {
                uint8_t* bytes = (uint8_t*)str + ix;
                uint8_t seq_len = get_utf8_seq(bytes, len - ix);
                ix += seq_len;
                uint64_t n_ix = node_ix;
                node_ix = dfa[n_ix].default_edge;
                for (uint32_t i = 0; i < dfa[n_ix].utf8_edge_count; ++i) {
                    uint8_t* edge = dfa[n_ix].utf8_edges[i].bytes;
                    uint8_t l = utf8_len_table[edge[0]];
                    if (l == seq_len && UTF8_EQ(bytes, edge, l)) {
                        node_ix = dfa[n_ix].utf8_edges[i].node_ix;
                        break;
                    }
                }
            }
        }
        if (node_ix == DFA_REJECT_NODE) {
            break;
        }
    }
    if (node_ix != DFA_REJECT_NODE && dfa[node_ix].accept) {
        return REGEX_MATCH;
    }
    return REGEX_NO_MATCH;
}

void Regex_allmatch_init(Regex* regex, const char* str, uint64_t len, RegexAllCtx* ctx) {
    ctx->regex = regex;
    ctx->str = str;
    ctx->len = len;
    ctx->start = 0;
}

RegexResult Regex_allmatch_dfa(RegexAllCtx* ctx, const char** match, uint64_t* match_len) {
    NodeDFA* dfa = ctx->regex->dfa;
    uint64_t len = ctx->len;
    uint8_t* str = (uint8_t*)ctx->str;
    for (uint64_t s = ctx->start; s <= len; ++s) {
        uint32_t node_ix = 0;
        if (len - s < ctx->regex->minlen) {
            break;
        }
        uint64_t ix = s;
        uint64_t accept_node = DFA_REJECT_NODE;
        uint64_t accept_ix;
        while (1) {
            if (dfa[node_ix].accept) {
                accept_node = node_ix;
                accept_ix = ix;
            }
            if (dfa[node_ix].literal_node) {
                if (len - ix < dfa[node_ix].literal.str_len) {
                    break;
                }
                // for some reason, loop is faster than memcmp...
                for (uint64_t i = 0; i < dfa[node_ix].literal.str_len; ++i) {
                    if (dfa[node_ix].literal.str[i] != str[ix + i]) {
                        goto next;
                    }
                }
                if (dfa[node_ix].literal.node_ix == DFA_REJECT_NODE) {
                    break;
                }
                ix += dfa[node_ix].literal.str_len;
                node_ix = dfa[node_ix].literal.node_ix;
            } else {
                if (ix >= len) {
                    break;
                }
                uint8_t c = str[ix];
                if (c < 128) {
                    node_ix = dfa[node_ix].ascii_edges[dfa[node_ix].ascii[c]];
                    if (node_ix == DFA_REJECT_NODE)  {
                        break;
                    }
                    ++ix;
                } else {
                    uint8_t* bytes = str + ix;
                    uint8_t seq_len = get_utf8_seq(bytes, len - ix);
                    uint64_t n_ix = node_ix;
                    node_ix = dfa[n_ix].default_edge;
                    for (uint32_t i = 0; i < dfa[n_ix].utf8_edge_count; ++i) {
                        uint8_t* edge = dfa[n_ix].utf8_edges[i].bytes;
                        uint8_t l = utf8_len_table[edge[0]];
                        if (l == seq_len && UTF8_EQ(bytes, edge, l)) {
                            node_ix = dfa[n_ix].utf8_edges[i].node_ix;
                            break;
                        }
                    }
                    if (node_ix == DFA_REJECT_NODE) {
                        break;
                    }
                    ix += seq_len;
                }
            }
        }
        next:
        if (accept_node != DFA_REJECT_NODE) {
            if (ctx->start == accept_ix) {
                ctx->start = accept_ix + 1;
            } else {
                ctx->start = accept_ix;
            }
            *match = (char*)str + s;
            *match_len = accept_ix - s;
            return REGEX_MATCH;
        }
    }
    return REGEX_NO_MATCH;
}

RegexResult Regex_anymatch_dfa(Regex* regex, const char* str, uint64_t len) {
    NodeDFA* dfa = regex->dfa;
    uint8_t* bytes = (uint8_t*) str;
    for (uint64_t s = 0; s < len; ++s) {
        uint32_t node_ix = 0;
        if (len - s < regex->minlen) {
            return REGEX_NO_MATCH;
        }
        for (uint64_t ix = s; ix < len;) {
            if (dfa[node_ix].accept) {
                return REGEX_MATCH;
            }
            if (dfa[node_ix].literal_node) {
                if (len - ix < dfa[node_ix].literal.str_len) {
                    break;
                }
                // for some reason, loop is faster than memcmp...
                for (uint64_t i = 0; i < dfa[node_ix].literal.str_len; ++i) {
                    if (dfa[node_ix].literal.str[i] != bytes[ix + i]) {
                        goto next;
                    }
                }
                ix += dfa[node_ix].literal.str_len;
                node_ix = dfa[node_ix].literal.node_ix;
            } else {
                uint8_t c = bytes[ix];
                if (c < 128) {
                    ++ix;
                    node_ix = dfa[node_ix].ascii_edges[dfa[node_ix].ascii[c]];
                } else {
                    uint8_t* bytes = (uint8_t*)str + ix;
                    uint8_t seq_len = get_utf8_seq(bytes, len - ix);
                    ix += seq_len;
                    uint64_t n_ix = node_ix;
                    node_ix = dfa[n_ix].default_edge;
                    for (uint32_t i = 0; i < dfa[n_ix].utf8_edge_count; ++i) {
                        uint8_t* edge = dfa[n_ix].utf8_edges[i].bytes;
                        uint8_t l = utf8_len_table[edge[0]];
                        if (l == seq_len && UTF8_EQ(bytes, edge, l)) {
                            node_ix = dfa[n_ix].utf8_edges[i].node_ix;
                            break;
                        }
                    }
                }
            }
            if (node_ix == DFA_REJECT_NODE) {
                break;
            }
        }
        next:
        if (node_ix != DFA_REJECT_NODE && dfa[node_ix].accept) {
            return REGEX_MATCH;
        }
    }
    return REGEX_NO_MATCH;
}

RegexResult Regex_fullmatch(Regex *regex, const char *str, uint64_t len) {
    if (regex->dfa != NULL) {
        return Regex_fullmatch_dfa(regex, str, len);
    }
    struct Node {
        uint64_t str_ix;
        uint32_t node_ix;
    };

    struct Node *to_visit = Mem_alloc(4 * sizeof(struct Node));
    uint8_t *visited = Mem_alloc((len + 1) * regex->nfa.node_count / 8 + 1);
    memset(visited, 0, (len + 1) * regex->nfa.node_count / 8 + 1);

    uint32_t to_visit_size = 1;
    uint32_t to_visit_cap = 4;
    if (to_visit == NULL || visited == NULL) {
        Mem_free(to_visit);
        Mem_free(visited);
        return REGEX_ERROR;
    }
    to_visit[0].node_ix = 0;
    to_visit[0].str_ix = 0;

    NFA *nfa = &regex->nfa;

    while (to_visit_size > 0) {
        struct Node node = to_visit[to_visit_size - 1];
        --to_visit_size;

        size_t ix = (node.str_ix + node.node_ix * (len + 1)) / 8;
        size_t bit = (node.str_ix + node.node_ix * (len + 1)) % 8;
        if ((visited[ix] >> bit) & 1) {
            continue;
        }

        if (nfa->nodes[node.node_ix].accept && node.str_ix == len) {
            Mem_free(to_visit);
            Mem_free(visited);
            return REGEX_MATCH;
        }

        visited[ix] |= (1 << bit);

        for (uint32_t ix = 0; ix < nfa->nodes[node.node_ix].edge_count; ++ix) {
            uint32_t edge_ix = ix + nfa->nodes[node.node_ix].edge_ix;
            uint64_t str_ix = node.str_ix;
            bool matches = matches_edge(regex, str, len, &str_ix, edge_ix);

            if (matches) {
                if (!RESERVE(&to_visit, &to_visit_cap, to_visit_size + 1,
                             struct Node)) {
                    goto fail;
                }
                to_visit[to_visit_size].str_ix = str_ix;
                to_visit[to_visit_size].node_ix = nfa->edges[edge_ix].to;
                ++to_visit_size;
            }
        }
    }

    Mem_free(to_visit);
    Mem_free(visited);
    return REGEX_NO_MATCH;

fail:
    Mem_free(to_visit);
    Mem_free(visited);
    return REGEX_ERROR;
}

RegexResult Regex_anymatch(Regex *regex, const char *str, uint64_t len) {
    if (regex->dfa != NULL) {
        return Regex_anymatch_dfa(regex, str, len);
    }
    struct Node {
        uint64_t str_ix;
        uint32_t node_ix;
    };

    struct Node* to_visit = Mem_alloc((2 * len) * sizeof(struct Node));
    uint32_t to_visit_cap = 2 * len;
    if (to_visit == NULL) {
        return REGEX_ERROR;
    }
    uint8_t* visited = Mem_alloc((len + 1) * regex->nfa.node_count / 8 + 1);
    if (visited == NULL) {
        Mem_free(to_visit);
        return REGEX_ERROR;
    }
    memset(visited, 0, (len + 1) * regex->nfa.node_count / 8 + 1);

    uint32_t to_visit_size = len;
    for (uint64_t ix = 0; ix < len; ++ix) {
        to_visit[len - ix - 1].node_ix = 0;
        to_visit[len - ix - 1].str_ix = ix;
    }

    NFA *nfa = &regex->nfa;

    while (to_visit_size > 0) {
        struct Node node = to_visit[to_visit_size - 1];
        --to_visit_size;

        size_t ix = (node.str_ix + node.node_ix * (len + 1)) / 8;
        size_t bit = (node.str_ix + node.node_ix * (len + 1)) % 8;
        if ((visited[ix] >> bit) & 1) {
            continue;
        }

        if (nfa->nodes[node.node_ix].accept) {
            Mem_free(to_visit);
            Mem_free(visited);
            return REGEX_MATCH;
        }

        visited[ix] |= (1 << bit);

        for (uint32_t ix = 0; ix < nfa->nodes[node.node_ix].edge_count; ++ix) {
            uint32_t edge_ix = ix + nfa->nodes[node.node_ix].edge_ix;
            uint64_t str_ix = node.str_ix;
            if (matches_edge(regex, str, len, &str_ix, edge_ix)) {
                if (!RESERVE(&to_visit, &to_visit_cap, to_visit_size + 1,
                        struct Node)) {
                    goto fail;
                }
                to_visit[to_visit_size].str_ix = str_ix;
                to_visit[to_visit_size].node_ix = nfa->edges[edge_ix].to;
                ++to_visit_size;
            }
        }
    }
fail:
    Mem_free(to_visit);
    Mem_free(visited);
    return REGEX_NO_MATCH;
}

RegexResult Regex_allmatch(RegexAllCtx* ctx, const char** match, uint64_t* len) {
    if (ctx->regex->dfa == NULL) {
        return REGEX_ERROR;
    }
    return Regex_allmatch_dfa(ctx, match, len);
}
