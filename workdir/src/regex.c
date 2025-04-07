#include "regex.h"
#include "dynamic_string.h"

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
            } else {
                parse_free(ctx);
                return REGEX_NO_MATCH;
            }
        } else if (pattern[ix] == '*') {
            if (ctx->ast[ctx->node_ix].type != REGEX_EMPTY &&
                ctx->ast[ctx->node_ix].type != REGEX_REPEAT) {
                if (ctx->ast[ctx->node_ix].type == REGEX_LITERAL &&
                    ctx->ast[ctx->node_ix].literal.size > 1) {
                    // Split last literal to only repeat last character
                    --ctx->ast[ctx->node_ix].literal.size;
                    RegexAst *node = newnode(ctx, REGEX_LITERAL, false);
                    if (node == NULL) {
                        parse_free(ctx);
                        return REGEX_ERROR;
                    }
                    node->literal.size = 1;
                    node->literal.str_ix = ctx->pattern.length - 1;
                    ctx->node_ix = node - ctx->ast;
                }
                RegexAst *node = newnode(ctx, REGEX_REPEAT, false);
                if (node == NULL) {
                    parse_free(ctx);
                    return REGEX_ERROR;
                }
                RegexAst tmp = *node;
                *node = ctx->ast[ctx->node_ix];
                node->next = 0;

                uint32_t node_ix = node - ctx->ast;
                ctx->ast[ctx->node_ix] = tmp;
                ctx->ast[ctx->node_ix].repeat.node_ix = node_ix;
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
    uint32_t edges_size = 0;
    uint32_t edges_cap = 4;
    uint32_t node_count = 1;
    uint32_t node_cap = 4;

    EdgeNFA *edges = Mem_alloc(edges_cap * sizeof(EdgeNFA));
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
            if (!RESERVE(&edges, &edges_cap, edges_size + 1, EdgeNFA)) {
                goto fail;
            }
            // Add edge to new node
            if (e->type == REGEX_LITERAL_UNION) {
                edges[edges_size].str_ix = e->literal_union.str_ix;
                edges[edges_size].str_size = e->literal_union.size;
                edges[edges_size].type =
                    e->literal_union.not ? NFA_EDGE_UNION_NOT : NFA_EDGE_UNION;
            } else if (e->type == REGEX_LITERAL) {
                edges[edges_size].str_ix = e->literal.str_ix;
                edges[edges_size].str_size = e->literal.size;
                edges[edges_size].type = NFA_EDGE_LITERAL;
            } else {
                edges[edges_size].type = NFA_EDGE_ANY;
            }
            edges[edges_size].from = node_count - 1;
            edges[edges_size].to = node_count;
            ++edges_size;
            nodes[node_count - 1].edge_count += 1;

            nodes[node_count].accept = false;
            nodes[node_count].edge_ix = edges_size;
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
            if (!RESERVE(&edges, &edges_cap, edges_size + 1, EdgeNFA)) {
                goto fail;
            }
            nodes[node_count - 1].edge_count += 1;
            EdgeNFA edge = {NFA_EDGE_LITERAL, 0, 0, node_count - 1, 0};
            edges[edges_size] = edge;
            ++edges_size;
            parse_stack[stack_size].edge_ix = edges_size - 1;
            parse_stack[stack_size].ast_ix = ast_ix;
            parse_stack[stack_size].node_ix = node_count - 1;
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
            if (!RESERVE(&edges, &edges_cap, edges_size + 1, EdgeNFA)) {
                goto fail;
            }
            nodes[node_count - 1].edge_count += 1;
            EdgeNFA edge = {NFA_EDGE_LITERAL, 0, 0, node_count - 1, node_count};
            edges[edges_size] = edge;
            ++edges_size;
            nodes[node_count].accept = false;
            nodes[node_count].edge_ix = edges_size;
            nodes[node_count].edge_count = 0;
            ++node_count;
            parse_stack[stack_size].edge_ix = 0;
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
            --stack_size;
            e = &ctx->ast[ast_ix];
            if (e->type == REGEX_REPEAT) {
                edges[edge_ix].to = node_count - 1;
                if (node_ix != node_count - 1) {
                    if (!RESERVE(&edges, &edges_cap, edges_size + 1, EdgeNFA)) {
                        goto fail;
                    }
                    nodes[node_count - 1].edge_count += 1;
                    EdgeNFA edge = {NFA_EDGE_LITERAL, 0, 0, node_count - 1,
                                    node_ix};
                    edges[edges_size] = edge;
                    ++edges_size;
                }
            } else if (e->type == REGEX_PAREN) {
                if (!RESERVE(&edges, &edges_cap, edges_size + 1, EdgeNFA)) {
                    goto fail;
                }
                if (!RESERVE(&nodes, &node_cap, node_count + 1, NodeNFA)) {
                    goto fail;
                }
                nodes[node_count - 1].edge_count += 1;
                EdgeNFA edge = {NFA_EDGE_LITERAL, 0, 0, node_count - 1,
                                node_count};
                edges[edges_size] = edge;
                ++edges_size;
                nodes[node_count].accept = false;
                nodes[node_count].edge_ix = edges_size;
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
    Mem_free(parse_stack);
    return true;

fail:
    Mem_free(parse_stack);
    Mem_free(nodes);
    Mem_free(edges);
    return false;
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

    Regex *regex = Mem_alloc(sizeof(Regex));
    if (regex == NULL) {
        String_free(&ctx.pattern);
        Mem_free(nfa.nodes);
        Mem_free(nfa.edges);
        return NULL;
    }
    regex->chars = ctx.pattern.buffer;
    regex->nfa = nfa;

    return regex;
}

void Regex_free(Regex *regex) {
    Mem_free(regex->nfa.nodes);
    Mem_free(regex->nfa.edges);
    String s;
    s.buffer = regex->chars;
    String_free(&s);
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

// TODO: union, sort edged based on length, utf8
RegexResult Regex_fullmatch(Regex *regex, const char *str, uint64_t len) {
    struct Node {
        uint64_t str_ix;
        uint32_t node_ix;
    };

    struct Node *to_visit = Mem_alloc(4 * sizeof(struct Node));
    struct Node *visited = Mem_alloc(4 * sizeof(struct Node));

    uint32_t to_visit_size = 1;
    uint32_t to_visit_cap = 4;
    uint32_t visited_size = 0;
    uint32_t visited_cap = 4;
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

        bool is_visited = false;
        for (uint64_t ix = 0; ix < visited_size; ++ix) {
            if (visited[ix].node_ix == node.node_ix &&
                visited[ix].str_ix == node.str_ix) {
                is_visited = true;
            }
        }
        if (is_visited) {
            continue;
        }

        if (nfa->nodes[node.node_ix].accept && node.str_ix == len) {
            Mem_free(to_visit);
            Mem_free(visited);
            return REGEX_MATCH;
        }

        if (!RESERVE(&visited, &visited_cap, visited_size + 1, struct Node)) {
            goto fail;
        }
        visited[visited_size] = node;
        ++visited_size;

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

    Mem_free(to_visit);
    Mem_free(visited);
    return REGEX_NO_MATCH;

fail:
    Mem_free(to_visit);
    Mem_free(visited);
    return REGEX_ERROR;
}
