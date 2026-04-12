#include "autocmp-lang.h"
#include "dynamic_string.h"
#include "printf.h"
#include "mem.h"
#include "glob.h"
#include "linkedhashmap.h"

#define MAX_NODE_DEPTH 32

typedef struct AutoCmpParser {
    char* indata;
    uint64_t ix;

    Token token;
    uint64_t start;
    uint64_t end;

    String string_token;

    struct {
        NodeBuilder node;
        Command* cmd;
    } node_stack[MAX_NODE_DEPTH];
    uint32_t node_stack_size;
    LinkedHashMap dynamic;

    WString workbuf;

    bool error;
} AutoCmpParser;

#define PARSER(ctx) ((AutoCmpParser*)(ctx))

Token peek_token(void* ctx, uint64_t* start, uint64_t* end) {
    AutoCmpParser* p = PARSER(ctx);

    *start = p->start;
    *end = p->end;
    return p->token;
}

void consume_token(void* ctx, uint64_t* start, uint64_t* end) {
    AutoCmpParser* p = PARSER(ctx);
    peek_token(ctx, start, end);
    while (p->indata[p->ix] == ' ' || p->indata[p->ix] == '\t') {
        ++p->ix;
    }

    if (p->indata[p->ix] == '\0') {
        p->start = p->end = p->ix;
        p->token.id = TOKEN_END;
        *start = p->start;
        return;
    }
    if (p->indata[p->ix] == '\r' || p->indata[p->ix] == '\n') {
        p->start = p->ix;
        if (p->indata[p->ix] == '\r' && p->indata[p->ix + 1] == '\n') {
            p->ix += 2;
        } else {
            p->ix += 1;
        }
        p->end = p->ix;
        p->token.id = TOKEN_NEWLINE;
        return;
    }
    p->start = p->ix;

    char c = p->indata[p->ix];
    if (c == '(' || c == ')' || c == '|' || c == '>' || c == '<') {
        p->token = literal_token(p->indata, p->ix + 1, &p->ix);
        p->end = p->ix;
        return;
    }

    bool in_string = false;
    String_clear(&p->string_token);

    while (1) {
        if (p->indata[p->ix] == '\0' || p->indata[p->ix] == '\r' || p->indata[p->ix] == '\n') {
            break;
        }
        if (in_string && p->indata[p->ix] == '"') {
            p->ix++;
            break;
        }
        if (!in_string && (p->indata[p->ix] == ' ' || p->indata[p->ix] == '\t' ||
                           p->indata[p->ix] == '(' || p->indata[p->ix] == ')' ||
                           p->indata[p->ix] == '<' || p->indata[p->ix] == '>' ||
                           p->indata[p->ix] == '|')) {
            break;
        }
        if (p->indata[p->ix] == '\\') {
            if (p->indata[p->ix + 1] == '\"') {
                p->ix += 2;
                String_append(&p->string_token, '"');
            } else if (p->indata[p->ix + 1] == '\\') {
                p->ix += 2;
                String_append(&p->string_token, '\\');
            } else {
                p->ix += 1;
                String_append(&p->string_token, '\\');
            }
            continue;
        }
        if (p->indata[p->ix] == '"') {
            in_string = true;
            p->ix++;
        } else {
            String_append(&p->string_token, p->indata[p->ix++]);
        }
    }
    p->end = p->ix;

    char* s = Mem_alloc((p->string_token.length + 1) * sizeof(*s));
    if (s == NULL) {
        p->error = true;
        _wprintf_e(L"Out of memory\n");
        p->token.id = TOKEN_END;
    } else {
        memcpy(s, p->string_token.buffer, p->string_token.length);
        s[p->string_token.length] = '\0';
        p->token.id = TOKEN_STRING;
        p->token.string = s;;
        _printf_e("Emiting string token '%s'\n", p->token.string);
    }
}

void parser_error(void* ctx, SyntaxError e) {
    AutoCmpParser* p = PARSER(ctx);

    uint64_t row = 1;
    uint64_t col = 1;
    for (uint64_t ix = 0; ix < e.start; ++ix) {
        if (p->indata[ix] == '\r') {
            if (p->indata[ix + 1] == '\n') {
                ++ix;
            }
            col = 1;
            ++row;
        } else if (p->indata[ix] == '\n') {
            col = 1;
            ++row;
        } else {
            ++col;
        }
    }

    _printf_e("Parse error: at row %llu, column %llu: %d\n", row, col, e.token.id);
    p->error = true;
}

ParenList* OnParenList(void* ctx, uint64_t start, uint64_t end, char* str) {
    ParenList* res = Mem_alloc(sizeof(*res));
    if (res == NULL) {
        PARSER(ctx)->error = true;
        Mem_free(str);
        return NULL;
    }
    *res = (ParenList){.str = str, .next = NULL};
    return res;
}

ParenList* OnAddParenList(void* ctx, uint64_t start, uint64_t end, ParenList* strs, char* str) {
    if (strs == NULL) {
        Mem_free(str);
        return NULL;
    }
    ParenList* next = OnParenList(ctx, start, end, str);
    next->next = strs;

    return next;
}


Command* OnCommand(void* ctx, uint64_t start, uint64_t end, char* str) {
    Command* c = Mem_alloc(sizeof(Command) + sizeof(char*));
    if (c == NULL) {
        PARSER(ctx)->error = true;
        Mem_free(str);
        return NULL;
    }
    c->parts[0] = str;
    c->count = 1;
    return c;
}

Command* OnListCommand(void* ctx, uint64_t start, uint64_t end, ParenList* strs) {
    if (strs == NULL) {
        // Memory error
        return NULL;
    }
    uint64_t count = 1;
    ParenList* l = strs;
    while (l->next != NULL) {
        l = l->next;
        ++count;
    }

    Command* c = Mem_alloc(sizeof(Command) + count * sizeof(char*));
    if (c == NULL) {
        l = strs;
        while (l != NULL) {
            Mem_free(l->str);
            ParenList* next = l->next;
            Mem_free(l);
            l = next;
        }
        PARSER(ctx)->error = true;
        return NULL;
    }
    c->count = count;
    for (uint64_t ix = 0; ix < count; ++ix) {
        c->parts[ix] = strs->str;
        l = strs->next;
        Mem_free(strs);
        strs = l;
    }
    return c;
}

Line OnLine(void* ctx, uint64_t start, uint64_t end, Command* cmd) {
    return (Line){.command = cmd, .loop = NULL};
}

Line OnLineWithLoop(void* ctx, uint64_t start, uint64_t end, Command* cmd, Command* loop) {
    return (Line){.command = cmd, .loop = loop};
}

void OnProgramLine(void* ctx, uint64_t start, uint64_t end, Line line) {
    OnProgramLineWithPrefix(ctx, start, end, 0, line);
}

bool add_node(NodeBuilder* builder, Command* cmd, LinkedHashMap* extr_map, MatchNode* node, WString* workbuf) {
    bool success = true;
    for (uint32_t ix = 0; ix < cmd->count; ++ix) {
        char* key = cmd->parts[ix];
        if (strcmp(key, "&FILE") == 0) {
            if (!NodeBuilder_add_files(builder, true, node)) {
                success = false;
            }
        } else if (strcmp(key, "&FILELIKE") == 0) {
            if (!NodeBuilder_add_files(builder, false, node)) {
                success = false;
            }
        } else if (strcmp(key, "&DEFAULT") == 0) {
            if (!NodeBuilder_add_any(builder, node)) {
                success = false;
            }
        } else if (key[0] == '&') {
            DynamicMatch* dyn = LinkedHashMap_Value(extr_map, key);
            if (dyn != NULL) {
                if (!NodeBuilder_add_dynamic(builder, dyn, node)) {
                    success = false;
                }
            } else {
                _wprintf(L"Invalid extern \"%S\"\n", key);
                success = false;
            }
        } else {
            if (!WString_from_utf8_str(workbuf, key)) {
                _wprintf(L"Invalid static \"%S\"\n", key);
                success = false;
            } else if (!NodeBuilder_add_fixed(builder, workbuf->buffer, workbuf->length, node)) {
                success = false;
            }
        }
        Mem_free(key);
    }
    Mem_free(cmd);
    return success;
}

void free_line(Line line) {
    if (line.loop != NULL) {
        for (uint32_t ix = 0; ix < line.loop->count; ++ix) {
            Mem_free(line.loop->parts[ix]);
        }
        Mem_free(line.loop);
    }
    if (line.command != NULL) {
        for (uint32_t ix = 0; ix < line.command->count; ++ix) {
            Mem_free(line.command->parts[ix]);
        }
        Mem_free(line.command);
    }
    line.loop = line.command = NULL;
}

void OnProgramLineWithPrefix(void* ctx, uint64_t start, uint64_t end, int32_t prefix, Line line) {
    if (line.command == NULL) {
        free_line(line);
        // Was empty line
        return;
    }
    AutoCmpParser* p = PARSER(ctx);

    NodeBuilder node;
    if (!NodeBuilder_create(&node)) {
        p->error = true;
        free_line(line);
        return;
    } else if (line.loop != NULL) {
        if (!add_node(&node, line.loop, &p->dynamic, NULL, &p->workbuf)) {
            p->error = true;
        }
        line.loop = NULL;
    }
    if (p->error) {
        free_line(line);
        NodeBuilder_abort(&node);
        return;
    }

    ++prefix;
    if (prefix <= 0 || prefix >= MAX_NODE_DEPTH || prefix > p->node_stack_size) {
        p->error = true;
        _wprintf_e(L"Bad prefix size: %d\n", prefix);
        free_line(line);
        NodeBuilder_abort(&node);
        return;
    }

    if (prefix == p->node_stack_size) {
        ++p->node_stack_size;
    } else if (prefix < p->node_stack_size) {
        for (uint64_t ix = p->node_stack_size - 1; ix >= prefix; ix--) {
            NodeBuilder* parent = &p->node_stack[ix - 1].node;
            MatchNode* node = NodeBuilder_finalize(&p->node_stack[ix].node);
            if (!add_node(parent, p->node_stack[ix].cmd, &p->dynamic, node, &p->workbuf)) {
                p->error = true;
            }
        }
        p->node_stack_size = prefix + 1;
    }
    p->node_stack[prefix].node = node;
    p->node_stack[prefix].cmd = line.command;
}


DynamicMatch* OnDynamic(void* ctx, uint64_t start, uint64_t end, char* name, char* cmd,
                        char* separator, char* invalidation) {
    AutoCmpParser* p = PARSER(ctx);

    wchar_t sep = '\0';
    if (strcmp(separator, "EOL") == 0) {
        sep = '\n';
    } else if (strcmp(separator, "NULL") == 0) {
        sep = '\0';
    } else {
        sep = separator[0];
    }
    enum Invalidation invalid = INVALID_ALWAYS;
    if (strcmp(invalidation, "Always") == 0) {
        invalid = INVALID_ALWAYS;
    } else if (strcmp(invalidation, "Never") == 0) {
        invalid = INVALID_NEVER;
    } else if (strcmp(invalidation, "Chdir") == 0) {
        invalid = INVALID_CHDIR;
    }

    Mem_free(separator);
    Mem_free(invalidation);

    if (!WString_from_utf8_str(&p->workbuf, cmd)) {
        Mem_free(cmd);
        Mem_free(name);
        p->error = true;
        return NULL;
    }
    Mem_free(cmd);

    LinkedHashElement* el = LinkedHashMap_Get(&p->dynamic, name);
    Mem_free(name);

    if (el == NULL) {
        p->error = true;
        return NULL;
    }
    if (el->value != NULL) {
        _printf_e("Duplicate dynamic entry: %s\n", el->key);
        p->error = true;
        return NULL;
    }

    DynamicMatch* match = DynamicMatch_create(p->workbuf.buffer, invalid, sep);
    el->value = match;

    return match;
}

void OnProgramDynamic(void* ctx, uint64_t start, uint64_t end, DynamicMatch* match) {

}

bool parse_file(ochar_t* filename) {
    String indata;

    MatchNode_reset();

    if (!read_text_file(&indata, filename)) {
        return false;
    }

    String token_buf;
    if (!String_create(&token_buf)) {
        String_free(&indata);
        return false;
    }

    bool success = false;

    AutoCmpParser p = {
        .indata = indata.buffer,
        .ix = 0,
        .start = 0,
        .end = 0,
        .string_token = token_buf,
        .node_stack_size = 1,
        .error = false
    };

    if (LinkedHashMap_Create(&p.dynamic)) {
        if (WString_create(&p.workbuf)) {
            NodeBuilder root;
            if (NodeBuilder_create(&root)) {
                p.node_stack[0].node = root;

                uint64_t start, end;
                consume_token(&p, &start, &end);

                success = parse(&p);
                if (p.error) {
                    success = false;
                }

                for (uint64_t ix = p.node_stack_size - 1; ix >= 1; ix--) {
                    NodeBuilder* parent = &p.node_stack[ix - 1].node;
                    MatchNode* node = NodeBuilder_finalize(&p.node_stack[ix].node);
                    if (!add_node(parent, p.node_stack[ix].cmd, &p.dynamic, node, &p.workbuf)) {
                        success = false;
                    }
                }
            }
            MatchNode* root_node = NodeBuilder_finalize(&p.node_stack[0].node);
            MatchNode_set_root(root_node);
            if (!success) {
                MatchNode_reset();
            } else {
                _wprintf(L"Loaded file\n");
            }

            WString_free(&p.workbuf);
        }
        LinkedHashMap_Free(&p.dynamic);
    }
    String_free(&indata);
    String_free(&p.string_token);

    return success;
}


int main() {
    MatchNode_init();
    parse_file(L"autocmp.txt");
}
