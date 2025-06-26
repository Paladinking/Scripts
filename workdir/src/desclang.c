#include "mem.h"
#include "args.h"
#include "glob.h"
#include "printf.h"
#include "hashmap.h"
#include "ochar.h"

typedef uint32_t rule_id;

#define MAX_RULES 512
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

bool reserve(void** dst, uint32_t* cap, uint32_t size, size_t elem_size) {
    if (size <= *cap) {
        return true;
    }
    if (size >= 0x7fffffff) {
        return false;
    }
    if (*cap == 0) {
        *cap = 4;
        while (*cap < size) {
            *cap *= 2;
        }
        *dst = Mem_alloc(*cap * elem_size);
        if (*dst == NULL) {
            *cap = 0;
            return false;
        }
    } else {
        uint32_t new_cap = *cap;
        while (new_cap < size) {
            new_cap *= 2;
        }
        void* new_ptr = Mem_realloc(*dst, new_cap * elem_size);
        if (new_ptr == NULL) {
            return false;
        }
        *dst = new_ptr;
        *cap = new_cap;
    }
    return true;
}

#define RESERVE(ptr, cap, size) reserve((void**)&(ptr), &(cap), size, sizeof(*(ptr)))

bool TokenSet_create(TokenSet* set) {
    set->tokens = Mem_alloc(8 * sizeof(rule_id));
    if (set->tokens == NULL) {
        return false;
    }
    set->cap = 8;
    set->size = 0;
    return true;
}

void TokenSet_free(TokenSet* set) {
    Mem_free(set->tokens);
    set->tokens = NULL;
    set->size = 0;
    set->cap = 0;
}

void TokenSet_clear(TokenSet* set) {
    set->size = 0;
}

bool TokenSet_copy(TokenSet* new, TokenSet* old) {
    new->tokens = Mem_alloc(old->cap * sizeof(rule_id));
    if (new->tokens == NULL) {
        return false;
    }
    memcpy(new->tokens, old->tokens, old->size * sizeof(rule_id));
    new->size = old->size;
    new->cap = old->cap;
    return true;
}

bool TokenSet_equals(TokenSet* a, TokenSet* b) {
    if (a->size != b->size) {
        return false;
    }
    return memcmp(a->tokens, b->tokens, a->size * sizeof(rule_id)) == 0;
}

bool TokenSet_insert(TokenSet* set, rule_id id) {
    if (set->cap == set->size) {
        rule_id* new_ptr = Mem_realloc(set->tokens, set->cap * 
                                       2 * sizeof(rule_id));
        if (new_ptr == NULL) {
            return false;
        }
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
            return true;
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

bool NodeRow_get_advanced(NodeRow* row, NodeRow* dest) {
    dest->rule = row->rule;
    dest->ix = row->ix + 1;
    if (!TokenSet_copy(&dest->lookahead, &row->lookahead)) {
        return false;
    }
    return true;
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

bool NodeRows_create(NodeRows* rows) {
    rows->rows = Mem_alloc(8 * sizeof(NodeRow));
    if (rows->rows == NULL) {
        return false;
    }
    rows->count = 0;
    rows->cap = 8;
    return true;
}

bool NodeRows_append(NodeRows* rows, NodeRow* row) {
    if (rows->count == rows->cap) {
        NodeRow* p = Mem_realloc(rows->rows, rows->cap * 2 * sizeof(NodeRow));
        if (p == NULL) {
            return false;
        }
        rows->rows = p;
        rows->cap *= 2;
    }
    rows->rows[rows->count] = *row;
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
} Node;

Node* Node_create(NodeRows rows) {
    Node* node = Mem_alloc(sizeof(Node));
    if (node == NULL) {
        return NULL;
    }
    node->rows = rows;
    node->accept = false;
    node->edges = Mem_alloc(8 * sizeof(NodeEdge));
    if (node->edges == NULL) {
        Mem_free(node);
        return NULL;
    }
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

bool Graph_create(Graph* g) {
    g->nodes = Mem_alloc(8 * sizeof(Node*));
    if (g->nodes == NULL) {
        return false;
    }
    g->count = 0;
    g->cap = 8;
    return true;
}

bool Graph_addNode(Graph* g, Node* n) {
    if (!reserve((void**)&g->nodes, &g->cap, g->count + 1, sizeof(Node*))) {
        return false;
    }
    g->nodes[g->count] = n;
    n->ix = g->count;
    g->count += 1;
    return true;
}

bool Node_add_children(Node* node, Graph* g, Rule* rules);

bool Graph_addEdge(Graph* g, Node* source, Node* dest, rule_id token,
                   Rule* rules) {
    if (!RESERVE(source->edges, source->edge_cap,
                 source->edge_count + 1)) {
        return false;
    }
    for (uint32_t ix = 0; ix < g->count; ++ix) {
        if (g->nodes[ix]->rows.count != dest->rows.count) {
            continue;
        }
        bool same = true;
        for (uint32_t j = 0; j < dest->rows.count; ++j) {
            if (g->nodes[ix]->rows.rows[j].rule != dest->rows.rows[j].rule) {
                same = false;
                break;
            }
            if (g->nodes[ix]->rows.rows[j].ix != dest->rows.rows[j].ix) {
                same = false;
                break;
            }
            if (!TokenSet_equals(&g->nodes[ix]->rows.rows[j].lookahead,
                                 &dest->rows.rows[j].lookahead)) {
                same = false;
                break;
            }
        }
        if (same) {
            Mem_free(dest);
            source->edges[source->edge_count].dest = g->nodes[ix];
            source->edges[source->edge_count].token = token;
            source->edge_count += 1;
            return true;
        }
    }
    if (!Graph_addNode(g, dest)) {
        return false;
    }
    source->edges[source->edge_count].dest = dest;
    source->edges[source->edge_count].token = token;
    source->edge_count += 1;
    if (!Node_add_children(dest, g, rules)) {
        return false;
    }
    return true;
}

void Graph_free(Graph* g) {
    for (uint32_t ix = 0; ix < g->count; ++ix) {
        Node_free(g->nodes[ix]);
    }
    Mem_free(g->nodes);
    g->nodes = NULL;
    g->count = 0;
    g->cap = 0;
}

bool Node_expand(Node* node, Rule* rules) {
    bool changed = true;

    uint8_t checked[MAX_RULES];
    rule_id* stack = Mem_alloc(16 * sizeof(rule_id));
    if (stack == NULL) {
        return false;
    }
    uint32_t stack_size = 0;
    uint32_t stack_cap = 16;

    while (changed) {
        changed = false;
    for (uint32_t ix = 0; ix < node->rows.count; ++ix) {
        NodeRow* row = &node->rows.rows[ix];
        rule_id r, next_rule;
        uint32_t ncount = NodeRow_get_next_rules(row, rules, &r, &next_rule);
        if (ncount == 0 || rules[r].type != RULE_MAPPING) {
            continue;
        }
        for (;r != RULE_ID_NONE; r = rules[r].next) {
            TokenSet look;
            if (!TokenSet_copy(&look, &row->lookahead)) {
                Mem_free(stack);
                return false;
            }
            if (ncount > 1 && rules[next_rule].type == RULE_MAPPING) {
                TokenSet_clear(&look);
                memset(checked, 0, sizeof(checked));
                checked[next_rule] = 1;
                stack_size = 0;
                for (rule_id i = next_rule; i != RULE_ID_NONE; i = rules[i].next) {
                    ++stack_size;
                }
                if (!RESERVE(stack, stack_cap, stack_size)) {
                    Mem_free(stack);
                    TokenSet_free(&look);
                    return false;
                }
                stack_size = 0;
                for (rule_id i = next_rule; i != RULE_ID_NONE; i = rules[i].next) {
                    stack[stack_size++] = i;
                }
                while (stack_size > 0) {
                    rule_id top = stack[stack_size - 1];
                    --stack_size;
                    if (rules[top].count == 0) {
                        continue;
                    }
                    top = rules[top].data[0];
                    if (rules[top].type != RULE_MAPPING) {
                        if (!TokenSet_insert(&look, top)) {
                            Mem_free(stack);
                            TokenSet_free(&look);
                            return false;
                        }
                        continue;
                    }
                    if (checked[top]) {
                        continue;
                    }
                    checked[top] = 1;
                    rule_id i = top;
                    uint32_t c = 0;
                    for (; i != RULE_ID_NONE; i = rules[i].next, ++c) {}
                    if (!RESERVE(stack, stack_cap, stack_size + c)) {
                        Mem_free(stack);
                        TokenSet_free(&look);
                        return false;
                    }
                    for (i = top; i != RULE_ID_NONE; i = rules[i].next) {
                        stack[stack_size++] = i;
                    }
                }
            } else if (ncount > 1) {
                TokenSet_clear(&look);
                if (!TokenSet_insert(&look, next_rule)) {
                    Mem_free(stack);
                    TokenSet_free(&look);
                    return false;
                }
            }
            uint32_t i = 0;
            for (; i < node->rows.count; ++i) {
                if (node->rows.rows[i].rule == r && 
                    node->rows.rows[i].ix == 0) {
                    TokenSet* old = &node->rows.rows[i].lookahead;
                    uint32_t old_len = old->size;
                    for (uint32_t j = 0; j < look.size; ++j) {
                        if (!TokenSet_insert(old, look.tokens[j])) {
                            Mem_free(stack);
                            TokenSet_free(&look);
                            return false;
                        }
                    }
                    if (old->size > old_len) {
                        changed = true;
                    }
                    break;
                }
            }
            if (i == node->rows.count) {
                changed = true;
                NodeRow new_row = {r, 0, look};
                if (!NodeRows_append(&node->rows, &new_row)) {
                    Mem_free(stack);
                    TokenSet_free(&look);
                    return false;
                }
            }
        }
    }
    }
    return true;
}

bool Node_add_children(Node* node, Graph* g, Rule* rules) {
    struct NewNode {
        rule_id token;
        NodeRows rows;
    };

    struct NewNode* consumed = Mem_alloc(4 * sizeof(struct NewNode));
    if (consumed == NULL) {
        return false;
    }
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
        if (!NodeRow_get_advanced(row, &advanced)) {
            goto fail;
        }
        if (!RESERVE(consumed, cap, size + 1)) {
            TokenSet_free(&advanced.lookahead);
            goto fail;
        }
        uint32_t ix = 0;
        for (; ix < size; ++ix) {
            if (consumed[ix].token == to_consume) {
                if (!NodeRows_append(&consumed[ix].rows, &advanced)) {
                    TokenSet_free(&advanced.lookahead);
                    goto fail;
                }
                break;
            }
        }
        if (ix == size) {
            if (!NodeRows_create(&consumed[size].rows)) {
                goto fail;
            }
            consumed[size].token = to_consume;
            ++size;
            if (!NodeRows_append(&consumed[size - 1].rows, &advanced)) {
                goto fail;
            }
        }
    }

    while (cleared < size) {
        Node* n = Node_create(consumed[cleared].rows);
        if (n == NULL) {
            goto fail;
        }
        ++cleared;
        if (!Node_expand(n, rules)) {
            Node_free(n);
            goto fail;
        }
        if (!Graph_addEdge(g, node, n, consumed[cleared - 1].token, rules)) {
            Node_free(n);
            goto fail;
        }

    }
    Mem_free(consumed);
    return true;
fail:
    for (uint32_t ix = cleared; ix < size; ++ix) {
        NodeRows_free(&consumed[ix].rows);
    }
    Mem_free(consumed);
    return false;
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

bool output_code(Graph* graph, Rule* rules, uint32_t rule_count) {
    _printf("#include <stdint.h>\n");
    _printf("#include \"mem.h\"\n\n");
    _printf("enum TokenType {\n");
    _printf("    TOKEN_LITERAL, TOKEN_END");
    for (uint32_t ix = 0; ix < rule_count - 1; ++ix) {
        if (rules[ix].type != RULE_ATOM) {
            continue;
        }
        _printf(", TOKEN_");
        for (const char *c = rules[ix].name; c[0] != '\0'; ++c) {
            char up = toupper(*c);
            _printf("%c", up);
        }
    }
    _printf("\n};\n");

    _printf("typedef struct Token {\n");
    _printf("    enum TokenType id;\n    union {\n");
    _printf("        const char* literal;\n");
    for (uint32_t ix = 0; ix < rule_count - 1; ++ix) {
        if (rules[ix].type != RULE_ATOM) {
            continue;
        }
        _printf("        %s %s;\n", rules[ix].ctype, rules[ix].name);
    }
    _printf("    };\n");
    _printf("} Token;\n\n");

    _printf("struct StackEntry {\n");
    _printf("    int32_t state;\n");
    _printf("    union {\n");
    for (uint32_t ix = 0; ix < rule_count - 1; ++ix) {
        if (rules[ix].type == RULE_LITERAL ||
            (rules[ix].type == RULE_MAPPING && 
             rules[ix].next != RULE_ID_NONE)) {
            continue;
        }
        _printf("        %s m%lu;\n", rules[ix].ctype, ix);
    }
    _printf("    };\n");
    _printf("};\n\n");
    _printf("struct Stack {\n");
    _printf("    struct StackEntry* b;\n");
    _printf("    uint32_t size;\n");
    _printf("    uint32_t cap;\n");
    _printf("};\n\n");

    uint32_t mapping_count = 0;
    uint32_t MAP_IX[MAX_RULES];
    for (uint32_t ix = 0; ix < rule_count - 1; ++ix) {
        if (rules[ix].type != RULE_MAPPING) {
            continue;
        }
        MAP_IX[ix] = mapping_count;
        ++mapping_count;
        if (rules[ix].redhook == NULL) {
            continue;
        }
        _printf("%s %s(void* ctx, uint64_t start, uint64_t end", 
                rules[ix].ctype, rules[ix].redhook);
        for (uint32_t j = 0; j < rules[ix].count; ++j) {
            _printf(", %s", rules[rules[ix].data[j]].ctype);
        }
        _printf(");\n\n");
    }

    uint32_t state_count = graph->count;
    _printf("const static int32_t reduce_table[%lu] = {\n", 
            state_count * mapping_count);
    for (uint32_t ix = 0; ix < graph->count; ++ix) {
        Node* n = graph->nodes[ix];
        _printf("    ");
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
                    _printf("%lu", n_ix);
                    break;
                }
            }
            if (!found) {
                _printf("-1");
            }
            if (MAP_IX[i] + 1 < mapping_count) {
                _printf(", ");
            }
        } 
        if (ix + 1 < graph->count) {
            _printf(",\n");
        }
    }
    _printf("\n};\n\n");

    _printf("Token peek_token(void* ctx, uint64_t* start, uint64_t* end);\n\n");
    _printf("void consume_token(void* ctx);\n\n");
    _printf("static void push_state(struct Stack* stack, struct StackEntry n) {\n");
    _printf("    if (stack->size == stack->cap) {\n");
    _printf("        struct StackEntry* p = Mem_realloc(stack->b,\n");
    _printf("                           2 * stack->cap * sizeof(struct StackEntry));\n");
    _printf("        if (p == NULL) {\n");
    _printf("            // TODO\n");
    _printf("        }\n");
    _printf("        stack->b = p;\n");
    _printf("        stack->cap *= 2;\n");
    _printf("    }\n");
    _printf("    stack->b[stack->size] = n;\n");
    _printf("    stack->size += 1;\n");
    _printf("}\n\n");


    _printf("bool parse(void* ctx) {\n");
    _printf("    int32_t state = 0;\n");
    _printf("    struct Stack stack;\n");
    _printf("    stack.b = Mem_alloc(16 * sizeof(struct StackEntry));\n");
    _printf("    stack.size = 1;\n");
    _printf("    stack.b[0].state = 0;\n");
    _printf("    stack.cap = 16;\n");
    _printf("    while (1) {\n");
    _printf("        struct StackEntry n;\n");
    _printf("        uint64_t start, end;\n");
    _printf("        Token t = peek_token(ctx, &start, &end);\n");
    _printf("        uint64_t len = end - start;\n");
    _printf("        switch (state) {\n");
    for (uint32_t ix = 0; ix < graph->count; ++ix) {
        Node* n = graph->nodes[ix];
        _printf("        case %lu: {\n", ix);
        _printf("            switch (t.id) {\n");
        bool has_literal = false;
        for (uint32_t j = 0; j < n->edge_count; ++j) {
            rule_id r = n->edges[j].token;
            if (rules[r].type == RULE_LITERAL) {
                uint64_t l = strlen(rules[r].name);
                if (!has_literal) {
                    _printf("            case TOKEN_LITERAL:\n");
                    _printf("                if (len == %llu", l);
                } else {
                    _printf(" else if (len == %llu", l);
                }
                for (uint64_t i = 0; i < l; ++i) {
                    _printf(" && t.literal[%lu] == '%c'", i, rules[r].name[i]);
                }
                _printf(") {\n");
                uint32_t new_state = n->edges[j].dest->ix;
                _printf("                    n.state = state = %lu;\n", new_state);
                _printf("                    push_state(&stack, n);\n");
                _printf("                    consume_token(ctx);\n");
                _printf("                }");
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
                    _printf("            case TOKEN_LITERAL:\n");
                    _printf("                if (len == %llu", l);
                } else {
                    _printf(" else if (len == %llu", l);
                }
                has_literal = true;
                for (uint64_t i2 = 0; i2 < l; ++i2) {
                    _printf(" && t.literal[%lu] == '%c'", i2, rules[r].name[i2]);
                }
                _printf(") {\n");
                if (rules[row->rule].redhook != NULL) {
                    rule_id id = row->rule;
                    while (rules[id].next != RULE_ID_NONE) {
                        id = rules[id].next;
                    }
                    _printf("                    n.m%lu = %s(ctx, start, end",
                            id, rules[row->rule].redhook);
                    for (uint32_t i2 = 0; i2 < rules[row->rule].count; ++i2) {
                        if (rules[rules[row->rule].data[i2]].type == RULE_LITERAL) {
                            continue;
                        }
                        rule_id id = rules[row->rule].data[i2];
                        if (rules[id].type == RULE_MAPPING) {
                            while (rules[id].next != RULE_ID_NONE) {
                                id = rules[id].next;
                            }
                        }
                        uint32_t s_ix = rules[row->rule].count - i2;
                        _printf(", stack.b[stack.size - %lu].m%lu",
                                s_ix, id);
                    }
                    _printf(");\n");
                }
                if (rules[row->rule].count > 0) {
                    _printf("                    stack.size -= %lu;\n",
                            rules[row->rule].count);
                    _printf("                    ");
                    _printf("state = stack.b[stack.size - 1].state;\n");
                }
                _printf("                    ");
                _printf("n.state = state = reduce_table[state * %lu + %lu];\n",
                        mapping_count, MAP_IX[row->rule]);
                _printf("                    ");
                if (rules[row->rule].count == 0) {
                    _printf("push_state(&stack, n);\n");
                } else {
                    _printf("stack.b[stack.size] = n;\n");
                    _printf("                    ++stack.size;\n");
                }
                _printf("                }");
            }
        }
        if (has_literal) {
            _printf(" else {\n");
            _printf("                    goto failure;\n");
            _printf("                }\n");
            _printf("                break;\n");
        }
        for (uint32_t j = 0; j < n->edge_count; ++j) {
            rule_id id = n->edges[j].token;
            if (rules[id].type != RULE_ATOM) {
                continue;
            }
            _printf("            case TOKEN_");
            for (const char* c = rules[id].name; *c != '\0'; ++c) {
                _printf("%c", toupper(*c));
            }
            _printf(":\n");
            uint32_t new_state = n->edges[j].dest->ix;
            _printf("                n.state = state = %lu;\n", new_state);
            _printf("                n.m%lu = t.%s;\n", id, rules[id].name);
            _printf("                push_state(&stack, n);\n");
            _printf("                consume_token(ctx);\n");
            _printf("                break;\n");
        }
        for (uint32_t j = 0; j < n->rows.count; ++j) {
            NodeRow* row = &n->rows.rows[j];
            if (row->ix < rules[row->rule].count) {
                continue;
            }
            for (uint32_t k = 0; k < row->lookahead.size; ++k) {
                rule_id id = row->lookahead.tokens[k];
                if (rules[id].type != RULE_ATOM) {
                    continue;
                }
                _printf("            case TOKEN_");
                if (id == rule_count - 1) {
                    _printf("END");
                } else {
                    for (const char* c = rules[id].name; *c != '\0'; ++c) {
                        _printf("%c", toupper(*c));
                    }
                }
                _printf(":\n");
                if (rules[row->rule].redhook != NULL) {
                    rule_id id = row->rule;
                    while (rules[id].next != RULE_ID_NONE) {
                        id = rules[id].next;
                    }
                    _printf("                n.m%lu = %s(ctx, start, end",
                            id, rules[row->rule].redhook);
                    for (uint32_t i2 = 0; i2 < rules[row->rule].count; ++i2) {
                        if (rules[rules[row->rule].data[i2]].type == RULE_LITERAL) {
                            continue;
                        }
                        rule_id id = rules[row->rule].data[i2];
                        if (rules[id].type == RULE_MAPPING) {
                            while (rules[id].next != RULE_ID_NONE) {
                                id = rules[id].next;
                            }
                        }
                        uint32_t s_ix = rules[row->rule].count - i2;
                        _printf(", stack.b[stack.size - %lu].m%lu",
                                s_ix, id);
                    }
                    _printf(");\n");
                }
                if (rules[row->rule].count > 0) {
                    _printf("                stack.size -= %lu;\n",
                            rules[row->rule].count);
                    _printf("                ");
                    _printf("state = stack.b[stack.size - 1].state;\n");
                }
                if (n->accept && id == rule_count - 1) {
                    _printf("                goto accept;\n");
                    continue;
                }
                _printf("                ");
                _printf("n.state = state = reduce_table[state * %lu + %lu];\n",
                        mapping_count, MAP_IX[row->rule]);
                _printf("                ");
                if (rules[row->rule].count == 0) {
                    _printf("push_state(&stack, n);\n");
                } else {
                    _printf("stack.b[stack.size] = n;\n");
                    _printf("                ++stack.size;\n");
                }
                _printf("                break;\n");
            }
        }
        _printf("            default:\n");
        _printf("                goto failure;\n");
        _printf("            }\n");
        _printf("            break;\n");
        _printf("        }\n");
    }
    _printf("        default:\n");
    _printf("            goto failure;\n");
    _printf("        }\n");
    _printf("    }\n");
    _printf("accept:\n");
    _printf("    Mem_free(stack);\n");
    _printf("    return true;\n");
    _printf("failure:\n");
    _printf("    __debugbreak();\n");
    _printf("}\n");
    
    return true;
}

bool validate_graph(Rule* rules, rule_id rule_count, Graph* graph, Rule* start) {
    bool good = true;
    bool accepting = false;
    rule_id s_id = start - rules;
    while (rules[s_id].next != RULE_ID_NONE) {
        s_id = rules[s_id].next;
    }
    for (uint32_t ix = 0; ix < graph->count; ++ix) {
        Node* n = graph->nodes[ix];
        for (rule_id id = 0; id < rule_count; ++id) {
            if (rules[id].type != RULE_LITERAL && rules[id].type != RULE_ATOM) {
                continue;
            }
            bool shift = false;
            bool reduce = false;
            for (uint32_t e = 0; e < n->edge_count; ++e) {
                if (n->edges[e].token == id) {
                    shift = true;
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
                    }
                    if (shift) {
                        _printf("Shift - Reduce conflict: ");
                        fmt_rule(rules, id);
                        _printf("\n");
                        fmt_rows(rules, &n->rows);
                        good = false;
                    }
                    rule_id s = row->rule;
                    while (rules[s].next != RULE_ID_NONE) {
                        s = rules[s].next;
                    }
                    if (s == s_id && id == rule_count - 1) {
                        n->accept = true;
                        _printf("Node %lu accepts %lu\n", ix, id);
                        accepting = true;
                    }
                    reduce = true;
                }
            }
        }
    }
    return good;
}

bool create_graph(Rule* rules, rule_id rule_count, Rule* start_rule) {
    NodeRows inital_row;
    if (!NodeRows_create(&inital_row)) {
        return false;
    }
    TokenSet look;
    if (!TokenSet_create(&look)) {
        NodeRows_free(&inital_row);
        return false;
    }
    // Add $ marker
    TokenSet_insert(&look, rule_count - 1);
    for (rule_id id = start_rule - rules; id != RULE_ID_NONE; id = rules[id].next) {
        NodeRow row = {id, 0, look};
        if (!NodeRows_append(&inital_row, &row)) {
            NodeRows_free(&inital_row);
            TokenSet_free(&look);
            return false;
        }
        if (rules[id].next != RULE_ID_NONE) {
            if (!TokenSet_copy(&look, &row.lookahead)) {
                NodeRows_free(&inital_row);
                return false;
            }
        }
    }

    Node* start_node = Node_create(inital_row);
    if (start_node == NULL) {
        NodeRows_free(&inital_row);
        return false;
    }

    if (!Node_expand(start_node, rules)) {
        Node_free(start_node);
        return false;
    }
    Graph graph;
    if (!Graph_create(&graph)) {
        Node_free(start_node);
        return false;
    }

    if (!Graph_addNode(&graph, start_node) || 
        !Node_add_children(start_node, &graph, rules)) {
        Graph_free(&graph);
        return false;
    }

    for (uint32_t ix = 0; ix < graph.count; ++ix) {
        _printf("%lu\n", ix);
        fmt_rows(rules, &graph.nodes[ix]->rows);
    }

    if (validate_graph(rules, rule_count, &graph, start_rule)) {
        output_code(&graph, rules, rule_count);
        Graph_free(&graph);
        return true;
    }
    Graph_free(&graph);
    return false;
}


int generate(ochar_t** argv, int argc) {
    if (argc < 1 || argv == NULL) {
        return 1;
    }
    FlagValue startnode = {FLAG_STRING};
    FlagInfo flags[] = {
        {oL('s'), oL("startnode"), &startnode},
    };
    ErrorInfo err;
    if (!find_flags(argv, &argc, flags, 1, &err)) {
        ochar_t* e = format_error(&err, flags, 1);
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
    if (!String_create(&data)) {
        oprintf_e("Out of memory\n");
        LineIter_abort(&ctx);
        return 2;
    }
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
            return 2;
        }
    }

    Rule rules[MAX_RULES];

    HashMap rule_map;
    if (!HashMap_Create(&rule_map)) {
        oprintf_e("Out of memory\n");
        String_free(&data);
        return 2;
    }

    uint64_t rule_count = 0;

    char* next = data.buffer;
    while (next != NULL) {
        line = next;
        uint64_t offset = line - data.buffer;
        char* end = memchr(line, ';', data.length - offset);
        if (end == NULL) {
            end = data.buffer + data.length;
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
                continue;
            }
            *name_end = '\0';
            if (line[ix] == '\'') {
                oprintf_e("Invalid rule name '%s'\n", line + ix);
                continue;
            }
            HashElement* e = HashMap_Get(&rule_map, line + ix);
            if (e == NULL) {
                goto err_out_of_mem;
            }
            Rule* rule = e->value;
            if (e->value == NULL) {
                if (rule_count == MAX_RULES) {
                    goto err_out_of_rules;
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
            _printf_e("Found type for '%s': '%s'\n", rule->name, rule->ctype);

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
                    goto err_out_of_rules;
                }
                HashElement* e = HashMap_Get(&rule_map, line + ix);
                if (e == NULL) {
                    goto err_out_of_mem;
                }
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
                    goto fail;
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
            continue;
        }
        while (name_len > 0 && is_space(name[name_len - 1])) {
            --name_len;
        }
        name[name_len] = '\0';

        ix = name_sep - line + 1;
        if (ix >= len || memchr(line + ix, '=', len - ix) != NULL) {
            _printf_e("Invalid rule '%s'\n", name);
            continue;
        }
        while (ix < len) {
            char* next = memchr(line + ix, '|', len - ix);
            if (next == NULL) {
                next = end;
            }
            char* part_end = next;
            char* hook_start = memchr(line + ix, ':', next - line - ix);
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
            rule_id* ids = Mem_alloc(max_parts * sizeof(rule_id));
            if (ids == NULL) {
                goto err_out_of_mem;
            }
            uint64_t part_ix = 0;
            while (part < part_end) {
                char* sep = memchr(part, '+', part_end - part);
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
                    e = HashMap_Get(&rule_map, cur);
                    sep[-1] = '\0';
                    ++cur;
                    if (e == NULL) {
                        Mem_free(ids);
                        goto err_out_of_mem;
                    }
                    if (e->value == NULL) {
                        if (rule_count == MAX_RULES) {
                            Mem_free(ids);
                            goto err_out_of_rules;
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
                        goto fail;
                    }
                } else {
                    *sep = '\0';
                    e = HashMap_Get(&rule_map, cur);
                    if (e == NULL) {
                        Mem_free(ids);
                        goto err_out_of_mem;
                    }
                    if (e->value == NULL) {
                        if (rule_count == MAX_RULES) {
                            Mem_free(ids);
                            goto err_out_of_rules;
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
            HashElement* e = HashMap_Get(&rule_map, name);
            if (e == NULL) {
                Mem_free(ids);
                goto err_out_of_mem;
            }
            Rule* new_rule = e->value;
            if (e->value == NULL) {
                if (rule_count == MAX_RULES) {
                    Mem_free(ids);
                    goto err_out_of_rules;
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
                    goto err_out_of_rules;
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
                    new_rule->redhook = hook_start;
                    _printf("Found hook %s for rule %s\n", hook_start, name);
                }
            }

            ix = next - line + 1;
        }
    }
    if (rule_count == MAX_RULES) {
        goto err_out_of_rules;
    }
    rules[rule_count].type = RULE_ATOM;
    rules[rule_count].name = "$";
    rules[rule_count].next = RULE_ID_NONE;
    rules[rule_count].ctype = NULL;
    rules[rule_count].redhook = NULL;
    ++rule_count;

    Rule* start_rule;
#ifdef NARROW_OCHAR
    start_rule = HashMap_Value(&rule_map, startnode.str);
#else
    String s;
    if (!String_create(&s)) {
        goto err_out_of_mem;
    }
    if (!String_from_utf16_str(&s, startnode.str)) {
        String_free(&s);
        goto err_out_of_mem;
    }
    start_rule = HashMap_Value(&rule_map, s.buffer);
    String_free(&s);
#endif
    HashMap_Free(&rule_map);

    if (start_rule == NULL || start_rule->type != RULE_MAPPING) {
        oprintf_e("Could not find start node '%s'\n", startnode.str);
        goto fail2;
    }

    for (uint64_t ix = 0; ix < rule_count; ++ix) {
        if (rules[ix].type == RULE_SLOT) {
            _printf_e("Missing rule '%s'\n", rules[ix].name);
            goto fail2;
        }
        if (rules[ix].type != RULE_LITERAL) {
            if (rules[ix].ctype == NULL) {
                rules[ix].ctype = "int64_t";
            }
        }
        //fmt_rule(rules, ix);
        //_printf("\n");
    }

    bool status = create_graph(rules, rule_count, start_rule);
    String_free(&data);

    for (rule_id id = 0; id < rule_count; ++id) {
        if (rules[id].type == RULE_MAPPING) {
            Mem_free(rules[id].data);
        }
    }

    return status ? 0 : 2;
err_out_of_rules:
    oprintf_e("Too many rules used\n");
    goto fail;
err_out_of_mem:
    oprintf_e("Out of memory\n");
fail:
    HashMap_Free(&rule_map);
fail2:
    String_free(&data);
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
