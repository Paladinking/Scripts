#include "mem.h"
#include "args.h"
#include "glob.h"
#include "printf.h"
#include "hashmap.h"
#include "ochar.h"

typedef uint32_t rule_id;

#define MAX_RULES 512
#define RULE_ID_ERROR 0
#define RULE_ID_NONE ((rule_id) -1)

typedef struct Rule {
    enum {
        RULE_MAPPING,
        RULE_ATOM,
        RULE_LITERAL,
        RULE_SLOT
    } type;
    const char* name;
    rule_id next;
    uint32_t count;
    rule_id* data;
    const char* redhook;
    const char* ctype;
} Rule;

typedef struct TokenSet {
    rule_id* tokens;
    uint32_t size;
    uint32_t cap;
} TokenSet;

void reserve(void** dst, uint32_t* cap, uint32_t size, size_t elem_size) {
    if (size <= *cap) {
        return;
    }
    if (*cap == 0) {
        *cap = 4;
        while (*cap < size) {
            *cap *= 2;
        }
        *dst = Mem_xalloc(*cap * elem_size);
    } else {
        uint32_t new_cap = *cap;
        while (new_cap < size) {
            new_cap *= 2;
        }
        void* new_ptr = Mem_xrealloc(*dst, new_cap * elem_size);
        *dst = new_ptr;
        *cap = new_cap;
    }
}

#define RESERVE(ptr, cap, size) reserve((void**)&(ptr), &(cap), size, sizeof(*(ptr)))

void TokenSet_create(TokenSet* set) {
    set->tokens = Mem_xalloc(8 * sizeof(rule_id));
    set->cap = 8;
    set->size = 0;
}

void TokenSet_free(TokenSet* set) {
    Mem_free(set->tokens);
    set->tokens = NULL;
    set->size = 0;
    set->cap = 0xafafafaf;
}

void TokenSet_clear(TokenSet* set) {
    set->size = 0;
}

void TokenSet_copy(TokenSet* new, TokenSet* old) {
    new->tokens = Mem_xalloc(old->cap * sizeof(rule_id));
    memcpy(new->tokens, old->tokens, old->size * sizeof(rule_id));
    new->size = old->size;
    new->cap = old->cap;
}

bool TokenSet_equals(TokenSet* a, TokenSet* b) {
    if (a->size != b->size) {
        return false;
    }
    return memcmp(a->tokens, b->tokens, a->size * sizeof(rule_id)) == 0;
}

// Returns false on no change
bool TokenSet_insert(TokenSet* set, rule_id id) {
    if (set->cap == set->size) {
        rule_id* new_ptr = Mem_xrealloc(set->tokens, set->cap * 
                                        2 * sizeof(rule_id));
        set->tokens = new_ptr;
        set->cap *= 2;
    }
    for (uint32_t ix = 0; ix < set->size; ++ix) {
        if (id < set->tokens[ix]) {
            memmove(set->tokens + ix + 1, set->tokens + ix,
                    (set->size - ix) * sizeof(rule_id));
            set->tokens[ix] = id;
            set->size += 1;
            return true;
        } else if (id == set->tokens[ix]) {
            return false;
        }
    }
    set->tokens[set->size] = id;
    set->size += 1;
    return true;
}

bool TokenSet_contains(TokenSet* set, rule_id id) {
    for (uint32_t ix = 0; ix < set->size; ++ix) {
        if (id == set->tokens[ix]) {
            return true;
        } else if (id < set->tokens[ix]) {
            break;
        }
    }
    return false;
}

typedef struct NodeRow {
    rule_id rule;
    uint32_t ix;
    TokenSet lookahead;
} NodeRow;

rule_id NodeRow_get_next(NodeRow* row, Rule* rules) {
    uint32_t count = rules[row->rule].count;
    if (row->ix >= count) {
        return RULE_ID_NONE;
    }
    return rules[row->rule].data[row->ix];
}

void NodeRow_get_advanced(NodeRow* row, NodeRow* dest) {
    dest->rule = row->rule;
    dest->ix = row->ix + 1;
    TokenSet_copy(&dest->lookahead, &row->lookahead);
}

// Return 0, 1, or 2
uint32_t NodeRow_get_next_rules(NodeRow* row, Rule* rules,
                                rule_id* first, rule_id* second) {
    uint32_t count = rules[row->rule].count;
    if (row->ix >= count) {
        return 0;
    }
    *first = rules[row->rule].data[row->ix];
    if (row->ix + 1 >= count) {
        return 1;
    }
    *second = rules[row->rule].data[row->ix + 1];
    return 2;
}

struct Node;

typedef struct NodeRows {
    NodeRow* rows;
    uint32_t count;
    uint32_t cap;
} NodeRows;

void NodeRows_create(NodeRows* rows) {
    rows->rows = Mem_xalloc(8 * sizeof(NodeRow));
    rows->count = 0;
    rows->cap = 8;
}

// Check if the LR(1) items are equal
bool NodeRows_fully_equal(NodeRows* a, NodeRows* b) {
    if (a->count != b->count) {
        return false;
    }
    for (uint32_t i = 0; i < a->count; ++i) {
        if (a->rows[i].rule != b->rows[i].rule) {
            return false;
        }
        if (a->rows[i].ix != b->rows[i].ix) {
            return false;
        }
        if (!TokenSet_equals(&a->rows[i].lookahead,
                             &b->rows[i].lookahead)) {
            return false;
        }
    }
    return true;
}

// Check if the LR(0) items are equal
bool NodeRows_equal(NodeRows* a, NodeRows* b) {
    if (a->count != b->count) {
        return false;
    }
    for (uint32_t i = 0; i < a->count; ++i) {
        if (a->rows[i].rule != b->rows[i].rule) {
            return false;
        }
        if (a->rows[i].ix != b->rows[i].ix) {
            return false;
        }
    }
    return true;
}

// returns false if row already existed
bool NodeRows_append(NodeRows* rows, NodeRow* row) {
    if (rows->count == rows->cap) {
        NodeRow* p = Mem_xrealloc(rows->rows, rows->cap * 2 * sizeof(NodeRow));
        rows->rows = p;
        rows->cap *= 2;
    }
    uint32_t i = 0;
    for (; i < rows->count; ++i) {
        if (rows->rows[i].rule < row->rule) {
            continue;
        }
        if (rows->rows[i].rule > row->rule) {
            break;
        }
        if (rows->rows[i].ix < row->ix) {
            continue;
        }
        if (rows->rows[i].ix > row->ix) {
            break;
        }
        bool change = false;
        for (uint32_t j = 0; j < row->lookahead.size; ++j) {
            rule_id token = row->lookahead.tokens[j];
            if (TokenSet_insert(&rows->rows[i].lookahead, token)) {
                change = true;
            }
        }
        TokenSet_free(&row->lookahead);
        return change;
    }
    if (i == rows->count) {
        rows->rows[rows->count] = *row;
    } else {
        memmove(rows->rows + i + 1, rows->rows + i, 
                (rows->count - i) * sizeof(NodeRow));
        rows->rows[i] = *row;
    }
    rows->count += 1;
    return true;
}

void NodeRows_free(NodeRows* rows) {
    for (uint32_t ix = 0; ix < rows->count; ++ix) {
        TokenSet_free(&rows->rows[ix].lookahead);
    }
    Mem_free(rows->rows);
    rows->rows = NULL;
    rows->count = 0;
    rows->cap = 0;
}

struct Node;

typedef struct NodeEdge {
    rule_id token;
    struct Node* dest;
} NodeEdge;

typedef struct Node {
    NodeRows rows;
    NodeEdge* edges;
    uint32_t edge_count;
    uint32_t edge_cap;
    uint32_t ix;
    bool accept;
    bool has_reduce;
    bool has_shift;
    bool reduct_reachable;
} Node;

Node* Node_create(NodeRows rows) {
    Node* node = Mem_xalloc(sizeof(Node));
    node->rows = rows;
    node->accept = false;
    node->has_shift = false;
    node->has_reduce = false;
    node->reduct_reachable = false;
    node->edges = Mem_xalloc(8 * sizeof(NodeEdge));
    node->edge_cap = 8;
    node->edge_count = 0;
    node->ix = 0;
    return node;
}

void Node_free(Node* node) {
    NodeRows_free(&node->rows);
    Mem_free(node->edges);
    Mem_free(node);
}

typedef struct Graph {
    Node** nodes;
    uint32_t count;
    uint32_t cap;
} Graph;

Graph* Graph_create() {
    Graph* g = Mem_xalloc(sizeof(Graph));
    g->nodes = Mem_xalloc(8 * sizeof(Node*));
    g->count = 0;
    g->cap = 8;
    return g;
}

void Graph_addNode(Graph* g, Node* n) {
    reserve((void**)&g->nodes, &g->cap, g->count + 1, sizeof(Node*));
    g->nodes[g->count] = n;
    n->ix = g->count;
    g->count += 1;
}

void Node_add_children(Node* node, Graph* g, Rule* rules);

void Graph_addEdge(Graph* g, Node* source, Node* dest, rule_id token,
                   Rule* rules) {
    RESERVE(source->edges, source->edge_cap,
            source->edge_count + 1);
    for (uint32_t ix = 0; ix < g->count; ++ix) {
        if (NodeRows_fully_equal(&g->nodes[ix]->rows, &dest->rows)) {
            Mem_free(dest);
            source->edges[source->edge_count].dest = g->nodes[ix];
            source->edges[source->edge_count].token = token;
            source->edge_count += 1;
            return;

        }
    }
    Graph_addNode(g, dest);
    source->edges[source->edge_count].dest = dest;
    source->edges[source->edge_count].token = token;
    source->edge_count += 1;
    Node_add_children(dest, g, rules);
}

void Graph_free(Graph* g) {
    for (uint32_t ix = 0; ix < g->count; ++ix) {
        Node_free(g->nodes[ix]);
    }
    Mem_free(g->nodes);
    g->nodes = NULL;
    g->count = 0;
    g->cap = 0;
    Mem_free(g);
}


enum CheckState {
    UNCHECKED = 0,
    PENDING, CHECKED_EMPTY, CHECKED_NONEMPTY
};

enum CheckState get_first_tokens(Rule* rules, rule_id id, TokenSet* set,
                                 int8_t* checked, uint32_t verbose) {
    if (verbose) {
        for (int i = 0; i < verbose; ++i) {
            _printf_e(" ");
        }
        _printf_e("get_first: %u (%s), ", id, rules[id].name);
        if (checked[id] == UNCHECKED) {
            _printf_e("UNCHECKED\n");
        } else if (checked[id] == PENDING) {
            _printf_e("PENDING\n");
        } else if (checked[id] == CHECKED_EMPTY) {
            _printf_e("CHECKED_EMPTY\n");
        } else if (checked[id] == CHECKED_NONEMPTY) {
            _printf_e("CHECKED_NONEMPTY\n");
        } else {
            _printf_e("UNKOWN\n");
        }
    }
    if (checked[id] != UNCHECKED) {
        return checked[id];
    }
    if (rules[id].type != RULE_MAPPING) {
        TokenSet_insert(set, id);
        checked[id] = CHECKED_NONEMPTY;
        if (verbose) {
            for (int i = 0; i < verbose; ++i) {
                _printf_e(" ");
            }
            _printf_e("Return CHECKED_NONEMPTY\n");
        }
        return CHECKED_NONEMPTY;
    }

    checked[id] = PENDING;

    int8_t own_checked[MAX_RULES];

    uint32_t pending_count = MAX_RULES + 1;
    uint32_t last_pending_count;
    while (pending_count > 0) {
        rule_id r = id;
        uint32_t ix = 0;
        bool has_nonempty = false;
        last_pending_count = pending_count;
        pending_count = 0;
        memcpy(own_checked, checked, sizeof(own_checked));
        while (r != RULE_ID_NONE) {
            if (ix >= rules[r].count) {
                checked[id] = CHECKED_EMPTY;
                own_checked[id] = CHECKED_EMPTY;
                r = rules[r].next;
                ix = 0;
                continue;
            }
            rule_id rule = rules[r].data[ix];
            if (own_checked[rule] == UNCHECKED) {
                if (verbose) {
                    get_first_tokens(rules, rule, set, own_checked, verbose + 1);
                } else {
                    get_first_tokens(rules, rule, set, own_checked, 0);
                }
            }
            if (own_checked[rule] == PENDING) {
                r = rules[r].next;
                ++pending_count;
            } else if (own_checked[rule] == CHECKED_NONEMPTY) {
                r = rules[r].next;
                has_nonempty = true;
            } else if (own_checked[rule] == CHECKED_EMPTY) {
                ++ix;
            }
        }
        if (pending_count == last_pending_count) {
            if (checked[id] != CHECKED_EMPTY && has_nonempty) {
                checked[id] = CHECKED_NONEMPTY;
                if (verbose) {
                    for (int i = 0; i < verbose; ++i) {
                        _printf_e(" ");
                    }
                    _printf_e("Return CHECKED_PENDING\n");
                }
            } else {
                checked[id] = PENDING;
                if (verbose) {
                    for (int i = 0; i < verbose; ++i) {
                        _printf_e(" ");
                    }
                    _printf_e("Return CHECKED_PENDING\n");
                }
            }
            return checked[id];
        }
    }

    if (verbose) {
        for (int i = 0; i < verbose; ++i) {
            _printf_e(" ");
        }
    }
    if (checked[id] != CHECKED_EMPTY) {
        checked[id] = CHECKED_NONEMPTY;
        if (verbose) {
            _printf_e("Return CHECKED_NONEMPTY\n");
        }
    } else if (verbose) {
        _printf_e("Return CHECKED_EMPTY\n");
    }
    return checked[id];
}

void NodeRow_get_lookahead(NodeRow* row, TokenSet* look, Rule* rules) {
    if (row->ix + 1 >= rules[row->rule].count) {
        TokenSet_copy(look, &row->lookahead);
        return;
    }

    int8_t checked[MAX_RULES];

    TokenSet_create(look);
    bool has_empty = false;
    uint32_t ix = row->ix + 1;
    do {
        has_empty = false;
        if (ix >= rules[row->rule].count) {
            for (uint32_t i = 0; i < row->lookahead.size; ++i) {
                TokenSet_insert(look, row->lookahead.tokens[i]);
            }
            break;
        }
        memset(checked, 0, sizeof(checked));
        rule_id r = rules[row->rule].data[ix];
        enum CheckState s = get_first_tokens(rules, r, look, checked, 0);
        if (s == CHECKED_EMPTY) {
            has_empty = true;
        } else if (s != CHECKED_NONEMPTY) {
            _printf_e("Recusrive pattern '%s'\n", rules[row->rule].name);
            memset(checked, 0, sizeof(checked));
            rule_id r = rules[row->rule].data[ix];
            enum CheckState s = get_first_tokens(rules, r, look, checked, 1);
            _printf_e("State: %d\n", s);
            __debugbreak();
        }
        ++ix;
    } while (has_empty);
}


void Node_expand(Node* node, Rule* rules) {
    bool changed = true;

    while (changed) {
        changed = false;
        for (uint32_t ix = 0; ix < node->rows.count; ++ix) {
            NodeRow* row = &node->rows.rows[ix];
            rule_id r = NodeRow_get_next(row, rules);
            if (r == RULE_ID_NONE || rules[r].type != RULE_MAPPING) {
                continue;
            }
            TokenSet look;
            NodeRow_get_lookahead(row, &look, rules);
            for (;r != RULE_ID_NONE; r = rules[r].next) {
                NodeRow new_row = {r, 0};
                TokenSet_copy(&new_row.lookahead, &look);
                if (NodeRows_append(&node->rows, &new_row)) {
                    changed = true;
                }
                row = &node->rows.rows[ix];
            }
            TokenSet_free(&look);
        }
    }
}

void Node_add_children(Node* node, Graph* g, Rule* rules) {
    struct NewNode {
        rule_id token;
        NodeRows rows;
    };

    struct NewNode* consumed = Mem_xalloc(4 * sizeof(struct NewNode));
    uint32_t cleared = 0;
    uint32_t size = 0;
    uint32_t cap = 4;

    for (uint32_t ix = 0; ix < node->rows.count; ++ix) {
        NodeRow* row = &node->rows.rows[ix];
        rule_id to_consume = NodeRow_get_next(row, rules);
        if (to_consume == RULE_ID_NONE) {
            continue;
        }
        NodeRow advanced;
        NodeRow_get_advanced(row, &advanced);
        RESERVE(consumed, cap, size + 1);
        uint32_t i = 0;
        for (; i < size; ++i) {
            if (consumed[i].token == to_consume) {
                NodeRows_append(&consumed[i].rows, &advanced);
                break;
            }
        }
        if (i == size) {
            NodeRows_create(&consumed[size].rows);
            consumed[size].token = to_consume;
            ++size;
            NodeRows_append(&consumed[size - 1].rows, &advanced);
        }
    }

    while (cleared < size) {
        Node* n = Node_create(consumed[cleared].rows);
        ++cleared;
        Node_expand(n, rules);
        Graph_addEdge(g, node, n, consumed[cleared - 1].token, rules);

    }
    Mem_free(consumed);
}

bool is_space(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

// TODO: stderr
void fmt_rule(Rule* rules, rule_id rule) {
    if (rules[rule].type == RULE_LITERAL) {
        _printf("'%s'", rules[rule].name);
        return;
    }
    if (rules[rule].type == RULE_ATOM) {
        _printf("%s : (%s)", rules[rule].name, rules[rule].ctype);
        return;
    }
    if (rules[rule].type == RULE_SLOT) {
        _printf("(%s)", rules[rule].name);
        return;
    }
    _printf("%s (%s) -> ", rules[rule].name, rules[rule].ctype);
    if (rules[rule].count == 0) {
        oprintf("'';");
        if (rules[rule].redhook != NULL) {
            _printf(" [%s]", rules[rule].redhook);
        }
        return;
    }
    for (uint32_t ix = 0; ix < rules[rule].count; ++ix) {
        if (ix > 0) {
            oprintf(" + ");
        }
        if (rules[rules[rule].data[ix]].type == RULE_LITERAL) {
            _printf("'%s'", rules[rules[rule].data[ix]].name);
        } else if (rules[rules[rule].data[ix]].type == RULE_SLOT) {
            _printf("(%s)", rules[rules[rule].data[ix]].name);
        } else {
            _printf("%s", rules[rules[rule].data[ix]].name);
        }
    }
    oprintf(";");
    if (rules[rule].redhook != NULL) {
        _printf(" [%s]", rules[rule].redhook);
    }
}

void fmt_row(Rule* rules, NodeRow* row) {
    _printf("%s ->", rules[row->rule].name);
    for (uint32_t ix = 0; ix < rules[row->rule].count; ++ix) {
        rule_id r = rules[row->rule].data[ix];
        if (ix == row->ix) {
            oprintf(" .");
        }
        if (rules[r].type == RULE_LITERAL) {
            _printf(" '%s'", rules[r].name);
        } else {
            _printf(" %s", rules[r].name);
        }
    }
    if (row->ix == rules[row->rule].count) {
        oprintf(" .");
    }
    oprintf(" \t{");
    for (uint32_t ix = 0; ix < row->lookahead.size; ++ix) {
        if (ix > 0) {
            oprintf(", ");
        }
        if (rules[row->lookahead.tokens[ix]].type == RULE_LITERAL) {
            _printf("'%s'", rules[row->lookahead.tokens[ix]].name);
        } else {
            _printf("%s", rules[row->lookahead.tokens[ix]].name);
        }
    }
    oprintf("}\n");
}

void fmt_rows(Rule* rules, NodeRows* rows) {
    for (uint32_t ix = 0; ix < rows->count; ++ix) {
        fmt_row(rules, &rows->rows[ix]);
    }
}

char upper(char a) {
    if (a >= 'a' && a <= 'z') {
        return a - 'a' + 'A';
    }
    return a;
}

void output_include_guard(const ochar_t* name, HANDLE h) {
    if (name[0] >= oL('0') && name[0] <= oL('9')) {
        oprintf_h(h, "_");
    }
    for (uint32_t ix = 0; name[ix] != oL('\0'); ++ix) {
        ochar_t c = otoupper(name[ix]);
        if (c == oL('.') || c == oL('-') || c == oL(' ')) {
            c = oL('_');
        }
        if (c == oL('_') || (c >= oL('a') && c <= oL('z')) ||
            (c >= oL('A') && c <= oL('Z')) || (c >= oL('0') && c <= oL('9'))) {
            oprintf_h(h, "%c", c);
        }
    }
}

void output_header(Graph* graph, Rule* rules, uint32_t rule_count, HANDLE h,
                   String* includes, const ochar_t* name) {
    if (name != NULL) {
        oprintf_h(h, "#ifndef ");
        output_include_guard(name, h);
        oprintf_h(h, "\n#define ");
        output_include_guard(name, h);
        oprintf_h(h, "\n");
    }
    _printf_h(h, "#include <stdint.h>\n");
    _printf_h(h, "#include <stdbool.h>\n");

    for (uint64_t ix = 0; ix < includes->length;) {
        uint64_t len = strlen(includes->buffer + ix);
        _printf_h(h, "#include %s\n", includes->buffer + ix);
        ix += len + 1;
    }
    _printf_h(h, "\n");

    _printf_h(h, "enum TokenType {\n");
    _printf_h(h, "    TOKEN_LITERAL, TOKEN_END");
    for (uint32_t ix = 0; ix < rule_count - 1; ++ix) {
        if (rules[ix].type != RULE_ATOM) {
            continue;
        }
        _printf_h(h, ", TOKEN_");
        for (const char *c = rules[ix].name; c[0] != '\0'; ++c) {
            char up = upper(*c);
            _printf_h(h, "%c", up);
        }
    }
    _printf_h(h, "\n};\n");

    _printf_h(h, "typedef struct Token {\n");
    _printf_h(h, "    enum TokenType id;\n");
    _printf_h(h, "    union {\n");
    _printf_h(h, "        const char* literal;\n");
    for (uint32_t ix = 0; ix < rule_count - 1; ++ix) {
        if (rules[ix].type != RULE_ATOM) {
            continue;
        }
        if (strcmp(rules[ix].ctype, "void") == 0) {
            continue;
        }
        _printf_h(h, "        %s %s;\n", rules[ix].ctype, rules[ix].name);
    }
    _printf_h(h, "    };\n");
    _printf_h(h, "} Token;\n\n");

    _printf_h(h, "typedef struct SyntaxError {\n");
    _printf_h(h, "    Token token;\n");
    _printf_h(h, "    uint64_t start;\n");
    _printf_h(h, "    uint64_t end;\n");
    _printf_h(h, "    const Token* expected;\n");
    _printf_h(h, "    uint64_t expected_count;\n");
    _printf_h(h, "} SyntaxError;\n\n");

    _printf_h(h, "Token peek_token(void* ctx, uint64_t* start, uint64_t* end);\n\n");
    _printf_h(h, "void consume_token(void* ctx, uint64_t* start, uint64_t* end);\n\n");
    _printf_h(h, "void parser_error(void* ctx, SyntaxError e);\n\n");
    _printf_h(h, "bool parse(void* ctx);\n");


    for (uint32_t ix = 0; ix < rule_count - 1; ++ix) {
        if (rules[ix].type != RULE_MAPPING) {
            continue;
        }
        if (rules[ix].redhook == NULL || rules[ix].redhook[0] == '$') {
            continue;
        }
        _printf_h(h, "\n%s %s(void* ctx, uint64_t start, uint64_t end", 
                rules[ix].ctype, rules[ix].redhook);
        for (uint32_t j = 0; j < rules[ix].count; ++j) {
            if (rules[rules[ix].data[j]].type == RULE_LITERAL) {
                continue;
            }
            if (strcmp(rules[rules[ix].data[j]].ctype, "void") == 0) {
                continue;
            }
            _printf_h(h, ", %s", rules[rules[ix].data[j]].ctype);
        }
        _printf_h(h, ");\n");
    }
    if (name != NULL) {
        _printf_h(h, "#endif\n");
    }
}

const static char* INDENTS[8] =  {
    "", "    ", "        ", "            ",
    "                ", "                    ",
    "                        ", "                            "
};

void output_shift(NodeEdge* edge, HANDLE h, uint8_t indents) {
    const char* i = INDENTS[indents];
    uint32_t new_state = edge->dest->ix;
    _printf_h(h, "%sn.state = %lu;\n", i, new_state);
    _printf_h(h, "%sconsume_token(ctx, &n.start, &n.end);\n", i);
    _printf_h(h, "%spush_state(&stack, n);\n", i);
}

void output_reduce(rule_id r, Rule* rules, uint32_t* map_ix,
                   uint32_t mapping_count, bool accept, HANDLE h, 
                   uint8_t indents) {
    const char* i = INDENTS[indents];
    _printf_h(h, i);
    if (rules[r].count > 0) {
        _printf_h(h, "n.start = stack.b[stack.size - %lu].start;\n",
                  rules[r].count);
        _printf_h(h, i);
        _printf_h(h, "n.end = stack.b[stack.size - 1].end;\n");
    } else {
        _printf_h(h, "n.start = start;\n");
        _printf_h(h, i);
        _printf_h(h, "n.end = start;\n");
    }
    if (rules[r].redhook != NULL) {
        rule_id id = r;
        while (rules[id].next != RULE_ID_NONE) {
            id = rules[id].next;
        }
        if (strcmp(rules[id].ctype, "void") != 0) {
            _printf_h(h, "%sn.m%lu = ", i, id);
        } else {
            _printf_h(h, "%s", i);
        }
        if (rules[r].redhook[0] == '$') {
            const char* hook = rules[r].redhook + 1;
            while (is_space(*hook)) {
                ++hook;
            }
            while (*hook != '\0') {
                if (*hook == '$') {
                    ++hook;
                    uint32_t ix;
                    char c = *hook;
                    if (c == '\0') {
                        break;
                    }
                    if (c < '0' || c > '9') {
                        continue;
                    }
                    ++hook;
                    ix = c - '0';
                    // Accept up to 2 digits.
                    if ((*hook) >= '0' && (*hook) <= '9') {
                        ix = ix * 10 + (*hook) - '0';
                        ++hook;
                    }
                    uint32_t count = 0;
                    for (uint32_t i2 = 0; i2 < rules[r].count; ++i2) {
                        if (rules[rules[r].data[i2]].type != RULE_LITERAL) {
                            rule_id id = rules[r].data[i2];
                            if (rules[id].type == RULE_MAPPING) {
                                while (rules[id].next != RULE_ID_NONE) {
                                    id = rules[id].next;
                                }
                            }
                            if (strcmp(rules[id].ctype, "void") == 0) {
                                continue;
                            }

                            if (count < ix) {
                                ++count;
                                continue;
                            }
                            uint32_t s_ix = rules[r].count - i2;
                            _printf_h(h, "stack.b[stack.size - %lu].m%lu",
                                      s_ix, id);
                            break;
                        }
                    }
                } else {
                    _printf_h(h, "%c", *hook);
                    ++hook;
                }
            }
            _printf_h(h, ";\n");
        } else {
            _printf_h(h, "%s(ctx, n.start, n.end", rules[r].redhook);
            for (uint32_t i2 = 0; i2 < rules[r].count; ++i2) {
                if (rules[rules[r].data[i2]].type == RULE_LITERAL) {
                    continue;
                }
                rule_id id = rules[r].data[i2];
                if (rules[id].type == RULE_MAPPING) {
                    while (rules[id].next != RULE_ID_NONE) {
                        id = rules[id].next;
                    }
                }
                if (strcmp(rules[id].ctype, "void") == 0) {
                    continue;
                }
                uint32_t s_ix = rules[r].count - i2;
                _printf_h(h, ", stack.b[stack.size - %lu].m%lu",
                        s_ix, id);
            }
            _printf_h(h, ");\n");
        }
    }
    if (rules[r].count > 0) {
        _printf_h(h, "%sstack.size -= %lu;\n", i,
                rules[r].count);
        _printf_h(h, i);
        _printf_h(h, "n.state = stack.b[stack.size - 1].state;\n");
    }
    _printf_h(h, i);
    if (accept) {
        _printf_h(h, "goto accept;\n");
        return;
    }
    _printf_h(h, "n.state = reduce_table[n.state * %lu + %lu];\n",
              mapping_count, map_ix[r]);
    _printf_h(h, i);
    if (rules[r].count == 0) {
        _printf_h(h, "push_state(&stack, n);\n");
    } else {
        _printf_h(h, "stack.b[stack.size] = n;\n");
        _printf_h(h, "%s++stack.size;\n", i);
    }
}

void output_reduce_table(Graph* graph, Rule* rules, uint32_t rule_count,
                         uint32_t* map_ix, uint32_t mapping_count,
                         HANDLE h) {
    uint32_t state_count = graph->count;
    const char* size = "uint32_t";
    uint32_t notfound = UINT32_MAX;
    if (state_count < 255) {
        size = "uint8_t";
        notfound = UINT8_MAX;
    } else if (state_count < 65535) {
        size = "uint16_t";
        notfound = UINT16_MAX;
    }
    _printf_h(h, "const static %s reduce_table[%lu] = {\n", 
            size, state_count * mapping_count);
    for (uint32_t ix = 0; ix < graph->count; ++ix) {
        Node* n = graph->nodes[ix];
        _printf_h(h, "    ");
        for (uint32_t i = 0; i < rule_count; ++i) {
            if (rules[i].type != RULE_MAPPING) {
                continue;
            }
            rule_id id = i;
            while (rules[id].next != RULE_ID_NONE) {
                id = rules[id].next;
            }
            bool found = false;
            for (uint32_t j = 0; j < n->edge_count; ++j) {
                rule_id edge_id = n->edges[j].token;
                if (rules[edge_id].type != RULE_MAPPING) {
                    continue;
                }
                while (rules[edge_id].next != RULE_ID_NONE) {
                    edge_id = rules[edge_id].next;
                }
                if (edge_id == id) {
                    found = true;
                    uint32_t n_ix = n->edges[j].dest->ix;
                    _printf_h(h, "%lu", n_ix);
                    break;
                }
            }
            if (!found) {
                _printf_h(h, "%lu", notfound);
            }
            if (map_ix[i] + 1 < mapping_count) {
                _printf_h(h, ", ");
            }
        } 
        if (ix + 1 < graph->count) {
            _printf_h(h, ",\n");
        }
    }
    _printf_h(h, "\n};\n\n");
}

NodeEdge* get_error_shift(Node* n) {
    for (uint32_t ix = 0; ix < n->edge_count; ++ix) {
        if (n->edges[ix].token == RULE_ID_ERROR) {
            return &n->edges[ix];
        }
    }
    return NULL;
}

void output_error_shifts(Graph* graph, Rule* rules, uint32_t rule_count, HANDLE h) {
    _printf_h(h, "        while(1) {\n");
    _printf_h(h, "            uint32_t i = stack.size;\n");
    _printf_h(h, "            while (i > 0) {\n");
    _printf_h(h, "                --i;\n");
    _printf_h(h, "                switch(stack.b[i].state) {\n");
    for (uint32_t ix = 0; ix < graph->count; ++ix) {
        Node* n = graph->nodes[ix];
        NodeEdge* e = get_error_shift(n);
        if (e == NULL) {
            continue;
        }
        Node* d = e->dest;
        bool has_action = false;
        for (uint32_t edge_ix = 0; edge_ix < d->edge_count; ++edge_ix) {
            if (d->edges[edge_ix].token == RULE_ID_ERROR) {
                continue;
            }
            if (rules[d->edges[edge_ix].token].type == RULE_MAPPING) {
                continue;
            }
            if (!has_action) {
                has_action = true;
                _printf_h(h, "                case %u:\n", n->ix);
                _printf_h(h, "                    if (");
            } else {
                _printf_h(h, " || ");
            }
            rule_id r = d->edges[edge_ix].token;
            if (rules[r].type == RULE_LITERAL) {
                const char* s = rules[r].name;
                uint64_t l = strlen(s);
                _printf_h(h, "(t.id == TOKEN_LITERAL && len == %llu", l);
                for (uint64_t i = 0; i < l; ++i) {
                    _printf_h(h, " && t.literal[%llu] == '%c'", i, s[i]);
                }
                _printf_h(h, ")");
            } else {
                _printf_h(h, "t.id == TOKEN_");
                for (const char* c = rules[r].name; *c != '\0'; ++c) {
                    _printf_h(h, "%c", upper(*c));
                }
            }
        }
        for (uint32_t row_ix = 0; row_ix < d->rows.count; ++row_ix) {
            NodeRow* row = &d->rows.rows[row_ix];
            if (row->ix < rules[row->rule].count) {
                continue;
            }
            for (uint32_t k = 0; k < row->lookahead.size; ++k) {
                rule_id id = row->lookahead.tokens[k];
                if (id == RULE_ID_ERROR || rules[id].type == RULE_MAPPING) {
                    continue;
                }
                if (!has_action) {
                    has_action = true;
                    _printf_h(h, "                case %u:\n", n->ix);
                    _printf_h(h, "                    if (");
                } else {
                    _printf_h(h, " || ");
                }
                if (rules[id].type == RULE_LITERAL) {
                    const char* s = rules[id].name;
                    uint64_t l = strlen(s);
                    _printf_h(h, "(t.id == TOKEN_LITERAL && len == %llu", l);
                    for (uint64_t i = 0; i < l; ++i) {
                        _printf_h(h, " && t.literal[%llu] == '%c'", i, s[i]);
                    }
                    _printf_h(h, ")");
                } else if (id == rule_count - 1) {
                    _printf_h(h, "t.id == TOKEN_END");
                } else {
                    _printf_h(h, "t.id == TOKEN_");
                    for (const char* c = rules[id].name; *c != '\0'; ++c) {
                        _printf_h(h, "%c", upper(*c));
                    }
                }
            }
        }
        if (has_action) {
            _printf_h(h, ") {\n");
            _printf_h(h, "                        stack.size = i + 1;\n");
            _printf_h(h, "                        n.state = %lu;\n", e->dest->ix);
            _printf_h(h, "                        push_state(&stack, n);\n");
            _printf_h(h, "                        goto loop;\n");
            _printf_h(h, "                    }\n");
        }
    }


    _printf_h(h, "                default:\n");
    _printf_h(h, "                    break;\n");
    _printf_h(h, "                }\n");
    _printf_h(h, "            }\n");
    _printf_h(h, "            if (t.id != TOKEN_END) {\n");
    _printf_h(h, "                consume_token(ctx, &n.start, &n.end);\n");
    _printf_h(h, "                t = peek_token(ctx, &start, &end);\n");
    _printf_h(h, "                len = end - start;\n");
    _printf_h(h, "                continue;\n");
    _printf_h(h, "            }\n");
    _printf_h(h, "            Mem_free(stack.b);\n");
    _printf_h(h, "            return false;\n");
    _printf_h(h, "        }\n");
}

void output_syntax_error(Graph* graph, Rule* rules, uint32_t rule_count, HANDLE h) {
    uint8_t included[MAX_RULES];
    uint32_t* counts = Mem_xalloc(graph->count * sizeof(uint32_t));

    for (uint32_t ix = 0; ix < graph->count; ++ix) {
        memset(included, 0, sizeof(included));
        _printf_h(h, "const static Token state%u_expected[] = {", ix);
        bool has_entry = false;
        counts[ix] = 0;

        Node* n = graph->nodes[ix];
        for (uint32_t edge_ix = 0; edge_ix < n->edge_count; ++edge_ix) {
            NodeEdge* e = &n->edges[edge_ix];
            if (included[e->token]) {
                continue;
            }
            if (e->token == RULE_ID_ERROR || rules[e->token].type == RULE_MAPPING) {
                continue;
            }
            included[e->token] = 1;
            if (has_entry) {
                _printf_h(h, ", ");
            } else {
                has_entry = true;
            }
            ++counts[ix];
            if (rules[e->token].type == RULE_LITERAL) {
                _printf_h(h, "{ TOKEN_LITERAL, \"%s\" }", rules[e->token].name);
            } else {
                _printf_h(h, "{ TOKEN_");
                if (e->token == rule_count - 1) {
                    _printf_h(h, "END");
                } else {
                    for (const char* c = rules[e->token].name; *c != '\0'; ++c) {
                        _printf_h(h, "%c", upper(*c));
                    }
                }
                _printf_h(h, " }");
            }
        }
        for (uint32_t row_ix = 0; row_ix < n->rows.count; ++row_ix) {
            NodeRow* row = &n->rows.rows[row_ix];
            if (row->ix < rules[row->rule].count) {
                continue;
            }
            for (uint32_t token_ix = 0; token_ix < row->lookahead.size; ++token_ix) {
                rule_id r = row->lookahead.tokens[token_ix];
                if (included[r] || r == RULE_ID_ERROR || rules[r].type == RULE_MAPPING) {
                    continue;
                }
                included[r] = 1;
                if (has_entry) {
                    _printf_h(h, ", ");
                } else {
                    has_entry = true;
                }
                ++counts[ix];
                if (rules[r].type == RULE_LITERAL) {
                    _printf_h(h, "{ TOKEN_LITERAL, \"%s\" }", rules[r].name);
                } else {
                    _printf_h(h, "{ TOKEN_");
                    if (r == rule_count - 1) {
                        _printf_h(h, "END");
                    } else {
                        for (const char* c = rules[r].name; *c != '\0'; ++c) {
                            _printf_h(h, "%c", upper(*c));
                        }
                    }
                    _printf_h(h, " }");
                }
            }
        }

        _printf_h(h, "};\n");
    }
    _printf_h(h, "static const Token* expected[] = {");
    for (uint32_t ix = 0; ix < graph->count; ++ix) {
        _printf_h(h, "state%u_expected", ix);
        if (ix < graph->count - 1) {
            _printf_h(h, ", ");
        }
    }
    _printf_h(h, "};\n");
    _printf_h(h, "static const uint32_t expected_count[] = {");
    for (uint32_t ix = 0; ix < graph->count; ++ix) {
        _printf_h(h, "%u", counts[ix]);
        if (ix < graph->count - 1) {
            _printf_h(h, ", ");
        }
    }
    _printf_h(h, "};\n\n");
    Mem_free(counts);

    _printf_h(h, "static SyntaxError get_syntax_error(int32_t state, Token t"
                 ", uint64_t start, uint64_t end) {\n");
    _printf_h(h, "    SyntaxError e = {t, start, end, NULL, 0};\n");
    _printf_h(h, "    if (state < 0 || state >= %u) {\n", graph->count);
    _printf_h(h, "        return e;\n");
    _printf_h(h, "    }\n");
    _printf_h(h, "    e.expected = expected[state];\n");
    _printf_h(h, "    e.expected_count = expected_count[state];\n");
    _printf_h(h, "    return e;\n");
    _printf_h(h, "}\n\n");
}

void output_code(Graph* graph, Rule* rules, uint32_t rule_count, HANDLE h,
                 String* includes, const ochar_t* header) {
    if (header != NULL) {
        oprintf_h(h, "#include \"%s\"\n", header);
    }
    _printf_h(h, "#include \"mem.h\"\n\n");

    if (header == NULL) {
        output_header(graph, rules, rule_count, h, includes, NULL);
    }

    uint32_t mapping_count = 0;
    uint32_t MAP_IX[MAX_RULES];

    for (uint32_t ix = 0; ix < rule_count - 1; ++ix) {
        if (rules[ix].type == RULE_MAPPING) {
            MAP_IX[ix] = mapping_count;
            ++mapping_count;
        }
    }

    _printf_h(h, "struct StackEntry {\n");
    _printf_h(h, "    int32_t state;\n");
    _printf_h(h, "    uint64_t start;\n");
    _printf_h(h, "    uint64_t end;\n");
    _printf_h(h, "    union {\n");
    for (uint32_t ix = 0; ix < rule_count - 1; ++ix) {
        if (rules[ix].type == RULE_LITERAL ||
            (rules[ix].type == RULE_MAPPING && 
             rules[ix].next != RULE_ID_NONE)) {
            continue;
        }
        if (strcmp(rules[ix].ctype, "void") == 0) {
            continue;
        }
        _printf_h(h, "        %s m%lu;\n", rules[ix].ctype, ix);
    }
    _printf_h(h, "    };\n");
    _printf_h(h, "};\n\n");
    _printf_h(h, "struct Stack {\n");
    _printf_h(h, "    struct StackEntry* b;\n");
    _printf_h(h, "    uint32_t size;\n");
    _printf_h(h, "    uint32_t cap;\n");
    _printf_h(h, "};\n\n");
    
    output_reduce_table(graph, rules, rule_count, MAP_IX, mapping_count, h);

    output_syntax_error(graph, rules, rule_count, h);

    _printf_h(h, "static void push_state(struct Stack* stack, struct StackEntry n) {\n");
    _printf_h(h, "    if (stack->size == stack->cap) {\n");
    _printf_h(h, "        struct StackEntry* p = (struct StackEntry*)Mem_realloc(stack->b,\n");
    _printf_h(h, "                           2 * stack->cap * sizeof(struct StackEntry));\n");
    _printf_h(h, "        if (p == NULL) {\n");
    _printf_h(h, "            // TODO\n");
    _printf_h(h, "        }\n");
    _printf_h(h, "        stack->b = p;\n");
    _printf_h(h, "        stack->cap *= 2;\n");
    _printf_h(h, "    }\n");
    _printf_h(h, "    stack->b[stack->size] = n;\n");
    _printf_h(h, "    stack->size += 1;\n");
    _printf_h(h, "}\n\n");

    _printf_h(h, "bool parse(void* ctx) {\n");
    _printf_h(h, "    struct Stack stack;\n");
    _printf_h(h, "    stack.b = (struct StackEntry*)Mem_alloc(16 * sizeof(struct StackEntry));\n");
    _printf_h(h, "    stack.size = 1;\n");
    _printf_h(h, "    stack.b[0].state = 0;\n");
    _printf_h(h, "    stack.b[0].start = 0;\n");
    _printf_h(h, "    stack.b[0].end = 0;\n");
    _printf_h(h, "    stack.cap = 16;\n");
    _printf_h(h, "    struct StackEntry n = {0, 0, 0};\n");
    _printf_h(h, "    while (1) {\n");
    _printf_h(h, "loop:\n");
    _printf_h(h, "        uint64_t start, end;\n");
    _printf_h(h, "        Token t = peek_token(ctx, &start, &end);\n");
    _printf_h(h, "        uint64_t len = end - start;\n");
    _printf_h(h, "        switch (n.state) {\n");
    for (uint32_t ix = 0; ix < graph->count; ++ix) {
        Node* n = graph->nodes[ix];
        _printf_h(h, "        case %lu: \n", ix);
        _printf_h(h, "            switch (t.id) {\n");
        bool has_literal = false;
        for (uint32_t j = 0; j < n->edge_count; ++j) {
            rule_id r = n->edges[j].token;
            if (rules[r].type == RULE_LITERAL) {
                uint64_t l = strlen(rules[r].name);
                if (!has_literal) {
                    _printf_h(h, "            case TOKEN_LITERAL:\n");
                    _printf_h(h, "                if (len == %llu", l);
                } else {
                    _printf_h(h, " else if (len == %llu", l);
                }
                for (uint64_t i = 0; i < l; ++i) {
                    _printf_h(h, " && t.literal[%lu] == '%c'", i, rules[r].name[i]);
                }
                _printf_h(h, ") {\n");
                output_shift(&n->edges[j], h, 5);
                _printf_h(h, "                }");
                has_literal = true;
            }
        }
        for (uint32_t j = 0; j < n->rows.count; ++j) {
            NodeRow* row = &n->rows.rows[j];
            if (row->ix < rules[row->rule].count) {
                continue;
            }
            for (uint32_t i = 0; i < row->lookahead.size; ++i) {
                rule_id r = row->lookahead.tokens[i];
                if (rules[r].type != RULE_LITERAL) {
                    continue;
                }
                uint64_t l = strlen(rules[r].name);
                if (!has_literal) {
                    _printf_h(h, "            case TOKEN_LITERAL:\n");
                    _printf_h(h, "                if (len == %llu", l);
                } else {
                    _printf_h(h, " else if (len == %llu", l);
                }
                has_literal = true;
                for (uint64_t i2 = 0; i2 < l; ++i2) {
                    _printf_h(h, " && t.literal[%lu] == '%c'", i2, rules[r].name[i2]);
                }
                _printf_h(h, ") {\n");
                output_reduce(row->rule, rules, MAP_IX, mapping_count, false, h, 5);
                _printf_h(h, "                }");
            }
        }
        if (has_literal) {
            _printf_h(h, " else {\n");
            _printf_h(h, "                    goto failure;\n");
            _printf_h(h, "                }\n");
            _printf_h(h, "                break;\n");
        }
        for (uint32_t j = 0; j < n->edge_count; ++j) {
            rule_id id = n->edges[j].token;
            if (id == RULE_ID_ERROR || rules[id].type != RULE_ATOM) {
                continue;
            }
            _printf_h(h, "            case TOKEN_");
            for (const char* c = rules[id].name; *c != '\0'; ++c) {
                _printf_h(h, "%c", upper(*c));
            }
            _printf_h(h, ":\n");
            uint32_t new_state = n->edges[j].dest->ix;
            if (strcmp(rules[id].ctype, "void") != 0) {
                _printf_h(h, "                n.m%lu = t.%s;\n", id, rules[id].name);
            }
            output_shift(&n->edges[j], h, 4);
            _printf_h(h, "                break;\n");
        }
        for (uint32_t j = 0; j < n->rows.count; ++j) {
            NodeRow* row = &n->rows.rows[j];
            if (row->ix < rules[row->rule].count) {
                continue;
            }
            bool has_reduce = false;
            bool has_accept = false;
            for (uint32_t k = 0; k < row->lookahead.size; ++k) {
                rule_id id = row->lookahead.tokens[k];
                if (id == RULE_ID_ERROR || rules[id].type != RULE_ATOM) {
                    continue;
                }
                bool accept = id == rule_count - 1 && n->accept;
                if (accept) {
                    has_accept = true;
                    continue;
                }
                has_reduce = true;
                _printf_h(h, "            case TOKEN_");
                if (id == rule_count - 1) {
                    _printf_h(h, "END");
                } else {
                    for (const char* c = rules[id].name; *c != '\0'; ++c) {
                        _printf_h(h, "%c", upper(*c));
                    }
                }
                _printf_h(h, ":\n");
            }
            if (has_reduce) {
                output_reduce(row->rule, rules, MAP_IX, mapping_count, 
                              false, h, 4);
                _printf_h(h, "                break;\n");
            }
            if (has_accept) {
                _printf_h(h, "            case TOKEN_END:\n");
                output_reduce(row->rule, rules, MAP_IX, mapping_count,
                              true, h, 4);
            }
        }
        _printf_h(h, "            default:\n");
        _printf_h(h, "                goto failure;\n");
        _printf_h(h, "            }\n");
        _printf_h(h, "            break;\n");
    }
    _printf_h(h, "        default:\n");
    _printf_h(h, "            goto failure;\n");
    _printf_h(h, "        }\n");
    _printf_h(h, "        continue;\n");
    _printf_h(h, "    failure:\n");
    _printf_h(h, "        parser_error(ctx, get_syntax_error(n.state, t, start, end));\n");
    output_error_shifts(graph, rules, rule_count, h);
    _printf_h(h, "    }\n");
    _printf_h(h, "accept:\n");
    _printf_h(h, "    Mem_free(stack.b);\n");
    _printf_h(h, "    return true;\n");
    _printf_h(h, "}\n");
}

bool validate_graph(Rule* rules, rule_id rule_count, Graph* graph, Rule* start) {
    bool good = true;
    rule_id s_id = start - rules;
    while (rules[s_id].next != RULE_ID_NONE) {
        s_id = rules[s_id].next;
    }
    uint64_t conf_count = 0;
    for (uint32_t ix = 0; ix < graph->count; ++ix) {
        Node* n = graph->nodes[ix];
        for (rule_id id = 0; id < rule_count; ++id) {
            if (rules[id].type == RULE_MAPPING) {
                for (uint32_t e = 0; e < n->edge_count; ++e) {
                    if (n->edges[e].token == id) {
                        n->edges[e].dest->reduct_reachable = true;
                    }
                }
                continue;
            }
            if (rules[id].type != RULE_LITERAL && rules[id].type != RULE_ATOM) {
                continue;
            }
            bool shift = false;
            bool reduce = false;
            for (uint32_t e = 0; e < n->edge_count; ++e) {
                if (n->edges[e].token == id) {
                    shift = true;
                    n->has_shift = true;
                    break;
                }
            }
            for (uint32_t r = 0; r < n->rows.count; ++r) {
                NodeRow* row = &n->rows.rows[r];
                if (row->ix < rules[row->rule].count) {
                    continue;
                }
                if (TokenSet_contains(&row->lookahead, id)) {
                    if (reduce) {
                        _printf("Reduce - Reduce conflict: ");
                        fmt_rule(rules, id);
                        _printf("\n");
                        fmt_rows(rules, &n->rows);
                        good = false;
                        ++conf_count;
                    }
                    if (shift) {
                        _printf("Shift - Reduce conflict: ");
                        fmt_rule(rules, id);
                        _printf("\n");
                        fmt_rows(rules, &n->rows);
                        ++conf_count;
                        good = false;
                    }
                    rule_id s = row->rule;
                    while (rules[s].next != RULE_ID_NONE) {
                        s = rules[s].next;
                    }
                    if (s == s_id && id == rule_count - 1) {
                        n->accept = true;
                    }
                    reduce = true;
                    n->has_reduce = true;
                }
            }
        }
    }

    if (conf_count > 0) { 
        oprintf_e("Total %lu conflicts\n", conf_count);
    }
    return good;
}

void merge_states(Graph* graph) {
loop:
    for (uint32_t i = 0; i < graph->count; ++i) {
        Node* n1 = graph->nodes[i];
        for (uint32_t j = 0; j < graph->count; ++j) {
            if (i == j) {
                continue;
            }
            Node* n2 = graph->nodes[j];
            if (!NodeRows_equal(&n1->rows, &n2->rows)) {
                continue;
            }
            graph->nodes[j] = graph->nodes[graph->count - 1];
            graph->count -= 1;
            for (uint32_t k = 0; k < graph->count; ++k) {
                Node* n = graph->nodes[k];
                int64_t n1_edge = -1;
                int64_t n2_edge = -1;
                for (uint32_t e = 0; e < n->edge_count; ++e) {
                    if (n->edges[e].dest == n1) {
                        n1_edge = e;
                    } else if (n->edges[e].dest == n2) {
                        n2_edge = e;
                    }
                }
                if (n2_edge != -1) {
                    if (n1_edge == -1) {
                        n->edges[n2_edge].dest = n1;
                    } else {
                        n->edges[n2_edge] = n->edges[n->edge_count - 1];
                        n->edge_count -= 1;
                    }
                }
                n->ix = k;
            }
            for (uint32_t r = 0; r < n1->rows.count; ++r) {
                for (uint32_t t = 0; t < n2->rows.rows[r].lookahead.size; ++t) {
                    TokenSet_insert(&n1->rows.rows[r].lookahead,
                                    n2->rows.rows[r].lookahead.tokens[t]);
                }
            }
            Node_free(n2);
            goto loop;
        }
    }
}

Graph* create_graph(Rule* rules, rule_id rule_count, Rule* start_rule) {
    NodeRows inital_row;
    NodeRows_create(&inital_row);
    TokenSet look;
    TokenSet_create(&look);
    // Add $ marker
    TokenSet_insert(&look, rule_count - 1);
    for (rule_id id = start_rule - rules; id != RULE_ID_NONE; id = rules[id].next) {
        NodeRow row = {id, 0, look};
        if (rules[id].next != RULE_ID_NONE) {
            TokenSet_copy(&look, &row.lookahead);
        }
        NodeRows_append(&inital_row, &row);
    }

    Node* start_node = Node_create(inital_row);
    Node_expand(start_node, rules);
    Graph* graph = Graph_create();

    Graph_addNode(graph, start_node);
    Node_add_children(start_node, graph, rules);

    merge_states(graph);


    if (validate_graph(rules, rule_count, graph, start_rule)) {
        /*for (uint32_t ix = 0; ix < graph->count; ++ix) {
            _printf("%lu\n", ix);
            fmt_rows(rules, &graph->nodes[ix]->rows);
        }*/
        return graph;
    }
    Graph_free(graph);
    return NULL;
}

// Like memchr, but skips '' literals. 
char* scan_memchr(char* s, char c, uint64_t len) {
    for (uint64_t ix = 0; ix < len; ++ix) {
        if (s[ix] == c) {
            return s + ix;
        }
        if (s[ix] == '\'') {
            ++ix;
            for (;ix < len; ++ix) {
                if (s[ix] == '\'') {
                    break;
                }
            }
        }
    }
    return NULL;
}

// 0: success, 1: out of rules, 2: other parse errror
int parse_lang(String* data, Rule* rules, rule_id* rule_len, HashMap* rule_map,
               String* headers) {
    uint64_t rule_count = 1;
    rules[RULE_ID_ERROR] = (Rule) 
        {RULE_ATOM, "error", RULE_ID_NONE, 0, NULL, NULL, NULL};
    HashMap_Insert(rule_map, "error", &rules[0]);

    char* line;
    char* next = data->buffer;
    while (next != NULL) {
        line = next;
        uint64_t offset = line - data->buffer;
        char* end = scan_memchr(line, ';', data->length - offset);
        if (end == NULL) {
            end = data->buffer + data->length;
            next = NULL;
        } else {
            next = end + 1;
        }
        uint64_t len = end - line;

        uint64_t ix = 0;
        while (ix < len && is_space(line[ix])) {
            ++ix;
        }
        if (ix == len) {
            continue;
        }
        if (len - ix >= 8 && memcmp(line + ix, "include:", 8) == 0) {
            ix += 8;
            while (ix < len && is_space(line[ix])) {
                ++ix;
            }
            while (end > line + ix && is_space(end[-1])) {
                --end;
            }
            *end = '\0';
            String_append_count(headers, line + ix, 1 + end - (line + ix));
            continue;
        }
        if (len - ix >= 5 && memcmp(line + ix, "type:", 5) == 0) {
            ix += 5;
            char* sep = memchr(line + ix, '=', len - ix);
            if (sep == NULL) {
                oprintf_e("Invalid type definition\n");
                continue;
            }
            while (ix < len && is_space(line[ix])) {
                ++ix;
            }
            char* name_end = sep;
            while (name_end > line + ix && is_space(name_end[-1])) {
                --name_end;
            }
            if (line + ix == name_end) {
                oprintf_e("Invalid rule name ''\n");
                return 2;
            }
            *name_end = '\0';
            if (line[ix] == '\'') {
                oprintf_e("Invalid rule name '%s'\n", line + ix);
                return 2;
            }
            HashElement* e = HashMap_Get(rule_map, line + ix);
            Rule* rule = e->value;
            if (e->value == NULL) {
                if (rule_count == MAX_RULES) {
                    return 1;
                }
                rules[rule_count].type = RULE_SLOT;
                rules[rule_count].next = RULE_ID_NONE;
                rules[rule_count].redhook = NULL;
                rules[rule_count].name = line + ix;
                rule = rules + rule_count;
                e->value = rule;
                ++rule_count;
            }
            uint64_t type_ix = sep - line + 1;
            while (type_ix < len && is_space(line[type_ix])) {
                ++type_ix;
            }
            while (end > line + type_ix && is_space(end[-1])) {
                --end;
            }
            *end = '\0';
            rule->ctype = line + type_ix;

            continue;
        }
        if (len - ix >= 6 && memcmp(line + ix, "atoms:", 6) == 0) {
            ix += 6;
            char* sep = line;
            while (sep < end) {
                while (len - ix > 0 && is_space(line[ix])) {
                    ++ix;
                }
                sep = memchr(line + ix, ',', len - ix);
                if (sep == NULL) {
                    sep = end;
                }
                *sep = '\0';
                if (rule_count == MAX_RULES) {
                    return 1;
                }
                HashElement* e = HashMap_Get(rule_map, line + ix);
                Rule* r = e->value;
                if (r == NULL) {
                    r = rules + rule_count;
                    r->name = line + ix;
                    r->next = RULE_ID_NONE;
                    r->ctype = NULL;
                    r->redhook = NULL;
                    e->value = r;
                    ++rule_count;
                } else if (r->type != RULE_SLOT) {
                    _printf_e("Duplicate rule name '%s'\n", line + ix);
                    return 2;
                }

                r->type = RULE_ATOM;
                ix = sep - line + 1;
            }
            continue;
        }
        char* name = line + ix;
        char* name_sep = memchr(name, '=', len - ix);
        uint64_t name_len = name_sep - name;
        if (name_sep == NULL) {
            _printf_e("Invalid rule '%.*s'\n", len - ix, line + ix);
            return 2;
        }
        while (name_len > 0 && is_space(name[name_len - 1])) {
            --name_len;
        }
        name[name_len] = '\0';

        ix = name_sep - line + 1;
        if (ix >= len) {
            _printf_e("Invalid rule '%s'\n", name);
            return 2;
        }
        while (ix < len) {
            char* next = scan_memchr(line + ix, '|', len - ix);
            if (next == NULL) {
                next = end;
            }
            char* part_end = next;
            char* hook_start = scan_memchr(line + ix, ':', next - line - ix);
            char* hook_end = NULL;
            if (hook_start != NULL) {
                hook_end = part_end;
                while (hook_end > hook_start && is_space(hook_end[-1])) {
                    --hook_end;
                }
                part_end = hook_start;
                do {
                    ++hook_start;
                } while (hook_start < hook_end && is_space(hook_start[0]));
            }

            while (part_end > line + ix && is_space(part_end[-1])) {
                --part_end;
            }
            while (ix < len && is_space(line[ix])) {
                ++ix;
            }
            char* part = line + ix;
            uint64_t part_len = (part_end - line) - ix;
            uint64_t max_parts = 1;
            for (uint64_t i = 0; i < part_len; ++i) {
                if (part[i] == '+') {
                    ++max_parts;
                }
            }
            rule_id* ids = Mem_xalloc(max_parts * sizeof(rule_id));
            if (ids == NULL) {
                return 3;
            }
            uint64_t part_ix = 0;
            while (part < part_end) {
                char* sep = scan_memchr(part, '+', part_end - part);
                char* cur = part;
                if (sep == NULL) {
                    sep = part_end;
                    part = part_end;
                } else {
                    part = sep + 1;
                }
                while (cur < sep && is_space(cur[0])) {
                    ++cur;
                }
                while (sep > cur && is_space(sep[-1])) {
                    --sep;
                }
                if (cur == sep) {
                    _printf_e("Illegal part emtpy part for '%s'\n", name);
                    continue;
                }
                HashElement* e;
                if (sep - cur >= 2 && cur[0] == '\'' && sep[-1] == '\'') {
                    if (sep - cur == 2) {
                        // Skip empty literal
                        continue;
                    }
                    sep[0] = '\0';
                    e = HashMap_Get(rule_map, cur);
                    sep[-1] = '\0';
                    ++cur;
                    if (e->value == NULL) {
                        if (rule_count == MAX_RULES) {
                            Mem_free(ids);
                            return 1;
                        }
                        rules[rule_count].type = RULE_LITERAL;
                        rules[rule_count].name = cur;
                        rules[rule_count].next = RULE_ID_NONE;
                        rules[rule_count].ctype = NULL;
                        rules[rule_count].redhook = NULL;
                        e->value = rules + rule_count;
                        ++rule_count;
                    } else if (((Rule*)e->value)->type != RULE_LITERAL) {
                        _printf_e("Literal name overlaps with rule %s\n",
                                 ((Rule*)e->value)->name);
                        Mem_free(ids);
                        return 2;
                    }
                } else {
                    *sep = '\0';
                    e = HashMap_Get(rule_map, cur);
                    if (e->value == NULL) {
                        if (rule_count == MAX_RULES) {
                            Mem_free(ids);
                            return 3;
                        }
                        rules[rule_count].type = RULE_SLOT;
                        rules[rule_count].name = cur;
                        rules[rule_count].next = RULE_ID_NONE;
                        rules[rule_count].redhook = NULL;
                        e->value = rules + rule_count;
                        ++rule_count;
                    }
                }
                Rule* r = e->value;
                rule_id id = r - rules;
                ids[part_ix] = id;
                ++part_ix;
            }
            HashElement* e = HashMap_Get(rule_map, name);
            Rule* new_rule = e->value;
            if (e->value == NULL) {
                if (rule_count == MAX_RULES) {
                    Mem_free(ids);
                    return 1;
                }
                rules[rule_count].next = RULE_ID_NONE;
                new_rule = rules + rule_count;
                new_rule->type = RULE_MAPPING;
                new_rule->name = name;
                new_rule->redhook = NULL;
                new_rule->ctype = NULL;
                e->value = new_rule;
                ++rule_count;
            } else if (new_rule->type == RULE_MAPPING) {
                if (rule_count == MAX_RULES) {
                    Mem_free(ids);
                    return 1;
                }
                while (new_rule->next != RULE_ID_NONE) {
                    new_rule = &rules[new_rule->next];
                }
                new_rule->next = rule_count;
                rules[rule_count].ctype = new_rule->ctype;
                new_rule = rules + rule_count;
                new_rule->name = name;
                new_rule->type = RULE_MAPPING;
                new_rule->next = RULE_ID_NONE;
                new_rule->redhook = NULL;
                ++rule_count;
            } else if (new_rule->type == RULE_SLOT) {
                new_rule->type = RULE_MAPPING;
            } else {
                _printf_e("Duplicate rule name '%s'\n", name);
                Mem_free(ids);
                ids = NULL;
            }

            if (ids != NULL) {
                new_rule->data = ids;
                new_rule->count = part_ix;
                if (hook_start != NULL && hook_start != hook_end) {
                    *hook_end = '\0';
                    // xyz => function xyz
                    // $xyz => literal xyz, where $<n> is substituted
                    // e.g $$1,
                    // $ 1 + $1
                    new_rule->redhook = hook_start;
                }
            }

            ix = next - line + 1;
        }
    }
    if (rule_count == MAX_RULES) {
        return 1;
    }
    rules[rule_count].type = RULE_ATOM;
    rules[rule_count].name = "$";
    rules[rule_count].next = RULE_ID_NONE;
    rules[rule_count].ctype = NULL;
    rules[rule_count].redhook = NULL;
    ++rule_count;

    *rule_len = rule_count;
    return 0;
} 

int generate(ochar_t** argv, int argc) {
    if (argc < 1 || argv == NULL) {
        return 1;
    }
    FlagValue startnode = {FLAG_STRING};
    FlagValue outfile = {FLAG_STRING};
    FlagValue header = {FLAG_STRING};
    FlagInfo flags[] = {
        {oL('s'), oL("startnode"), &startnode},
        {oL('o'), oL("output"), &outfile},
        {oL('H'), oL("header"), &header}
    };
    ErrorInfo err;
    if (!find_flags(argv, &argc, flags, 3, &err)) {
        ochar_t* e = format_error(&err, flags, 3);
        if (e != NULL) {
            oprintf_e("%s\n", e);
            oprintf_e("Run '%s' --help for more information\n", argv[0]);
            Mem_free(e);
            return 1;
        }
    }
    if (flags[0].count == 0) {
        startnode.str = oL("PROGRAM");
    }
    if (flags[2].count == 0) {
        header.str = NULL;
    }

    if (argc < 2 || argv == NULL) {
        oprintf_e("Missing argument\n");
        if (argv != NULL && argc > 0) {
            oprintf_e("Run '%s' --help for more information\n", argv[0]);
        }
        return 1;
    }

    const ochar_t* file = argv[1];
    LineCtx ctx;
    if (!LineIter_begin(&ctx, file)) {
        oprintf_e("Failed reading file '%s'", file);
        return 1;
    }

    String data;
    String_create(&data);

    char* line;
    uint64_t len;
    while ((line = LineIter_next(&ctx, &len)) != NULL) {
        uint64_t ix = 0;
        while (ix < len && is_space(line[ix])) {
            ++ix;
        }
        if (ix == len) {
            continue;
        }
        if (line[ix] == '/' && ix + 1 < len && line[ix + 1] == '/') {
            continue;
        }
        if (!String_append_count(&data, line + ix, len - ix)) {
            oprintf_e("Out of memory\n");
            LineIter_abort(&ctx);
            String_free(&data);
            return 3;
        }
    }

    Rule rules[MAX_RULES];

    HashMap rule_map;
    HashMap_Create(&rule_map);

    String includes;
    String_create(&includes);

    rule_id rule_count;
    int status = parse_lang(&data, rules, &rule_count, &rule_map, &includes);
    if (status > 0) {
        String_free(&includes);
        String_free(&data);
        HashMap_Free(&rule_map);
        if (status == 1) {
            oprintf_e("Too many rules used in spec\n");
        }
        return status;
    }

    Rule* start_rule;
#ifdef NARROW_OCHAR
    start_rule = HashMap_Value(&rule_map, startnode.str);
#else
    String s;
    String_create(&s);
    if (!String_from_utf16_str(&s, startnode.str)) {
        String_free(&s);
        HashMap_Free(&rule_map);
        goto fail;
    }
    start_rule = HashMap_Value(&rule_map, s.buffer);
    String_free(&s);
#endif
    HashMap_Free(&rule_map);

    if (start_rule == NULL || start_rule->type != RULE_MAPPING) {
        oprintf_e("Could not find start node '%s'\n", startnode.str);
        goto fail;
    }

    for (uint64_t ix = 0; ix < rule_count; ++ix) {
        if (rules[ix].type == RULE_SLOT) {
            _printf_e("Missing rule '%s'\n", rules[ix].name);
            goto fail;
        }
        if (rules[ix].type != RULE_LITERAL) {
            if (rules[ix].ctype == NULL) {
                rules[ix].ctype = "void";
            }
        }
    }

    Graph* g = create_graph(rules, rule_count, start_rule);
    if (g != NULL) {
        HANDLE out;

        if (header.str != NULL) {
            out = open_file_write(header.str);
            if (out == INVALID_HANDLE_VALUE) {
                oprintf_e("Failed opening '%s'\n", header.str);
                Graph_free(g);
                goto fail;
            }
            // Find filename component
            ochar_t* o = ostrrchr(header.str, '/');
#ifdef WIN32
            ochar_t* o2 = ostrrchr(header.str, '\\');
            if (o2 != NULL && (o == NULL || o2 > o)) {
                o = o2;
            }
#endif
            if (o != NULL) {
                header.str = o + 1;
            }

            output_header(g, rules, rule_count, out, &includes, header.str);
        }

        if (flags[1].count == 0) {
            out = GetStdHandle(STD_OUTPUT_HANDLE);
        } else {
            out = open_file_write(outfile.str);
            if (out == INVALID_HANDLE_VALUE) {
                oprintf_e("Failed opening '%s'\n", outfile.str);
                Graph_free(g);
                goto fail;
            }
        }

        output_code(g, rules, rule_count, out, &includes, header.str);
        if (flags[1].count > 0) {
            CloseHandle(out);
        }
        Graph_free(g);
    }

    String_free(&data);
    String_free(&includes);

    for (rule_id id = 0; id < rule_count; ++id) {
        if (rules[id].type == RULE_MAPPING) {
            Mem_free(rules[id].data);
        }
    }

    return g != NULL ? 0 : 2;
fail:
    String_free(&data);
    String_free(&includes);
    for (rule_id id = 0; id < rule_count; ++id) {
        if (rules[id].type == RULE_MAPPING) {
            Mem_free(rules[id].data);
        }
    }
    return 1;
}


#ifdef WIN32
int main() {
    wchar_t* cmd = GetCommandLineW();
    int argc;
    ochar_t** argv = parse_command_line(cmd, &argc);
    int status = generate(argv, argc);
    Mem_free(argv);

    ExitProcess(status);
}

#else
int main(char** argv, int argc) {
    return compiler(argv, argc);
}
#endif
