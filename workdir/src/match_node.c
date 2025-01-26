#include "match_node.h"
#include "subprocess.h"
#include "args.h"


void MatchNode_init() {
    WHashMap_Create(&gNodes);

    NodeBuilder b;
    NodeBuilder_create(&b);
    NodeBuilder_add_files(&b, false, NULL);
    NodeBuilder_add_any(&b, NULL);
    gAny_file = NodeBuilder_finalize(&b);

    NodeBuilder_create(&b);
    NodeBuilder_add_files(&b, true, NULL);
    NodeBuilder_add_any(&b, NULL);
    gExisting_file = NodeBuilder_finalize(&b);
}

void MatchNode_free() {
    // TODO:
    // Currenty no reason to write, since there is no point
    // to call this function.
}

void MatchNode_set_root(MatchNode *root) {
    gRoot = root;
}

void NodeIterator_begin(NodeIterator* it, MatchNode* node) {
    it->node = node;
    it->ix = 0;
    it->dyn_ix = 0;
    it->walk_ongoing = false;
}

const wchar_t* NodeIterator_next(NodeIterator* it) {
    while (1) {
        if (it->ix >= it->node->match_count) {
            it->node = NULL;
            it->ix = 0;
            it->dyn_ix = 0;
            return NULL;
        }
        Match* match = &it->node->matches[it->ix];
        unsigned type = match->type;
        if (type == MATCH_ANY) {
            it->ix += 1;
            continue;
        }
        if (type == MATCH_STATIC) {
            it->ix += 1;
            return match->static_match;
        }
        if (type == MATCH_ANY_FILE || type == MATCH_EXISTING_FILE) {
            if (!it->walk_ongoing) {
                if (!WalkDir_begin(&it->walk_ctx, L".")) {
                    // Error trying to iterate working dir, skip to next
                    it->ix += 1;
                    continue;
                }
                it->walk_ongoing = true;
            }
            Path* path;
            if (!WalkDir_next(&it->walk_ctx, &path)) {
                it->walk_ongoing = false;
                it->ix += 1;
                continue;
            }
            return path->path.buffer;
        }
        // DynamicMatch left
        DynamicMatch* dyn = match->dynamic_match;
        if (it->dyn_ix == 0) {
            DynamicMatch_evaluate(dyn);
        }
        if (it->dyn_ix >= dyn->match_count) {
            it->ix += 1;
            it->dyn_ix = 0;
            continue;
        }
        it->dyn_ix += 1;
        return dyn->matches[it->dyn_ix - 1];
    }
}

// Call if you end loop before NodeIterator_next returns NULL.
void NodeIterator_abort(NodeIterator* it) {
    if (it->walk_ongoing) {
        WalkDir_abort(&it->walk_ctx);
        it->walk_ongoing = false;
    }
    it->node = NULL;
    it->ix = 0;
    it->dyn_ix = 0;
}

WHashMap gNodes;

unsigned gDynamic_count = 0;
unsigned gDynamic_capacity = 0;
DynamicMatch** gDynamic_matches = NULL;

MatchNode* gRoot; // Root of autocomplete graph
MatchNode* gAny_file; // Special node used for '>'
MatchNode* gExisting_file; // Special node used for '<'

bool is_filelike(const wchar_t* str, unsigned len) {
    if (len == 0) {
        return false;
    }
    unsigned ix = 0;
    for (; ix < len; ++ix) {
        if (str[ix] == L':') {
            if (ix != 1) {
                return false;
            }
            if (!(str[ix - 1] >= 'a' && str[ix - 1] <= 'z') &&
                !(str[ix - 1] >= 'A' && str[ix - 1] <= 'Z')) {
                return false;
            }
        } else if (str[ix] == L'|' || str[ix] == L'>' || str[ix] == L'<' || str[ix] == L'*' || str[ix] == L'?') {
            return false;
        }
    }
    return true;
}

bool reserve(void** dst, unsigned* cap, unsigned size, size_t elem_size) {
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
        *dst = HeapAlloc(GetProcessHeap(), 0, *cap * elem_size);
        if (*dst == NULL) {
            *cap = 0;
            return false;
        }
    } else {
        unsigned new_cap = *cap;
        while (new_cap < size) {
            new_cap *= 2;
        }
        void* new_ptr = HeapReAlloc(GetProcessHeap(), 0, *dst, new_cap * elem_size);
        if (new_ptr == NULL) {
            return false;
        }
        *dst = new_ptr;
        *cap = new_cap;
    }
    return true;
}

#define RESERVE(ptr, cap, size, type) reserve((void**)ptr, cap, size, sizeof(type))


// Adds to global structures
DynamicMatch* DynamicMatch_create(wchar_t* cmd, enum Invalidation invalidation, wchar_t sep) {
    DynamicMatch* n = HeapAlloc(GetProcessHeap(), 0, sizeof(DynamicMatch));
    if (n == NULL) {
        return NULL;
    }
    n->match_count = 0;
    n->matches = NULL;
    n->invalidation = invalidation;
    n->separator = sep;
    n->cmd = cmd;
    n->node_count = 0;
    n->node_capacity = 0;
    n->nodes = NULL;
    if (!RESERVE(&n->nodes, &n->node_capacity, 1, *n->nodes)) {
        HeapFree(GetProcessHeap(), 0, n);
    }

    if (!RESERVE(&gDynamic_matches, &gDynamic_capacity, gDynamic_count + 1, DynamicMatch*)) {
        HeapFree(GetProcessHeap(), 0, n->nodes);
        HeapFree(GetProcessHeap(), 0, n);
        return NULL;
    }
    gDynamic_matches[gDynamic_count] = n;
    gDynamic_count += 1;
    return n;
}

// Adds to global structures
void DynamicMatch_evaluate(DynamicMatch* ptr) {
    // Already valid
    if (ptr->matches != NULL) {
        return;
    }

    static wchar_t* empty_match = L"";
    ptr->match_count = 0;
    String out;
    String_create(&out);
    unsigned long exit_code;
    if (!subprocess_run(ptr->cmd, &out, 1000, &exit_code)) {
        String_free(&out);
        ptr->matches = &empty_match;
        return;
    }

    WString buf;
    if (!WString_create(&buf) || !WString_from_utf8_bytes(&buf, out.buffer, out.length)) {
        String_free(&out);
        ptr->matches = &empty_match;
        return;
    }
    String_free(&out);

    unsigned size = 0;
    unsigned len = 0;
    for (unsigned ix = 0; ix <= buf.length; ++ix) {
        if (ptr->separator == L'\n' && buf.buffer[ix] == L'\r') {
            buf.buffer[ix] = L'\n';
        }
        if (buf.buffer[ix] == L'0' || buf.buffer[ix] == ptr->separator) {
            if (len == 0) {
                continue;
            }
            size += sizeof(wchar_t*) + sizeof(wchar_t);
            len = 0;
            ptr->match_count += 1;
            continue;
        }
        size += sizeof(wchar_t);
        len += 1;
    }

    if (ptr->match_count == 0) { // This avoids potential memory leak of zero-sized allocation.
        WString_free(&buf);
        ptr->matches = &empty_match;
        return;
    }

    unsigned char* b = HeapAlloc(GetProcessHeap(), 0, size);
    if (b == 0) {
        WString_free(&buf);
        ptr->matches = &empty_match;
        return;
    }
    wchar_t* data = (wchar_t*)(b + ptr->match_count * sizeof(wchar_t*));
    ptr->matches = (wchar_t**) b;

    ptr->match_count = 0;
    wchar_t* last_data = data;
    len = 0;
    for (unsigned ix = 0; ix <= buf.length; ++ix) {
        if (buf.buffer[ix] == L'0' || buf.buffer[ix] == ptr->separator) {
            if (len == 0 || len > 0xffff) {
                continue;
            }
            ptr->matches[ptr->match_count] = last_data;
            ptr->match_count += 1;
            *data = L'\0';
            ++data;
            last_data = data;
            len = 0;
            continue;
        }
        *data = buf.buffer[ix];
        ++data;
        len += 1;
    }

    for (int j = 0; j < ptr->node_count; ++j) {
        wchar_t* hash_postfix = ptr->nodes[j].parent->hash_postfix;
        for (unsigned ix = 0; ix < ptr->match_count; ++ix) {
            WString_clear(&buf);
            WString_extend(&buf, ptr->matches[ix]);
            WString_append_count(&buf, hash_postfix, HASH_POSTFIX_LEN);

            WHashElement* entry = WHashMap_Get(&gNodes, buf.buffer);
            if (entry->value == NULL) {
                entry->value = ptr->nodes[j].match;
            }
        }
    }

    WString_free(&buf);
}

void DynamicMatch_invalidate(DynamicMatch* ptr) {
    if (ptr->matches == NULL) {
        return;
    }
    if (ptr->match_count == 0) {
        ptr->matches = NULL;
        return;
    }
    WString buf;
    WString_create_capacity(&buf, 20);


    for (int j = 0; j < ptr->node_count; ++j) {
        wchar_t* hash_postfix = ptr->nodes[j].parent->hash_postfix;
        for (unsigned ix = 0; ix < ptr->match_count; ++ix) {
            WString_clear(&buf);
            WString_extend(&buf, ptr->matches[ix]);
            WString_append_count(&buf, hash_postfix, HASH_POSTFIX_LEN);

            Match* m = WHashMap_Value(&gNodes, buf.buffer);
            if (m == ptr->nodes[j].match) {
                WHashMap_Remove(&gNodes, buf.buffer);
            }
        }
    }

    HeapFree(GetProcessHeap(), 0, ptr->matches);
    ptr->matches = NULL;
    ptr->match_count = 0;
}
 

bool NodeBuilder_create(NodeBuilder* builder) {
    static uint32_t postfix = 0;

    MatchNode* node = HeapAlloc(GetProcessHeap(), 0, sizeof(MatchNode));
    if (node == NULL) {
        return false;
    }

    uint32_t b[2] = {postfix, postfix};
    node->hash_postfix[0] = L'\xc';
    memcpy(node->hash_postfix + 1, &b, 4 * sizeof(wchar_t));
    ++postfix;

    node->matches = NULL;
    node->match_count = 0;
    node->file_ix = -1;
    node->any_ix = -1;

    builder->node = node;
    builder->node_cap = 0;

    return true;
}


bool NodeBuilder_add_fixed(NodeBuilder* builder, const wchar_t* str, unsigned len, MatchNode* child) {
    if (len > 0xffff) {
        return false;
    }
    if (child == NULL) {
        child = builder->node;
    }
    unsigned count = builder->node->match_count + 1;
    if (!RESERVE(&builder->node->matches, &builder->node_cap, count, Match)) {
        return false;
    }

    Match* match = &builder->node->matches[builder->node->match_count];
    WString s;
    if (!WString_create_capacity(&s, len)) {
        return false;
    }
    WString_append_count(&s, str, len);
    match->type = MATCH_STATIC;
    match->static_match = s.buffer;
    match->child = child;
    builder->node->match_count = count;

    return true;
}

bool NodeBuilder_add_dynamic(NodeBuilder* builder, DynamicMatch* dyn, MatchNode* child) {
    if (child == NULL) {
        child = builder->node;
    }
    unsigned count = builder->node->match_count + 1;
    if (!RESERVE(&builder->node->matches, &builder->node_cap, count, Match)) {
        return false;
    }
    Match* match = &builder->node->matches[builder->node->match_count];
    match->type = MATCH_DYNAMIC;
    match->dynamic_match = dyn;
    match->child = child;
    builder->node->match_count = count;

    return true;
}

bool NodeBuilder_add_files(NodeBuilder* builder, bool exists, MatchNode* child) {
    if (builder->node->file_ix >= 0) {
        return false;
    }
    if (child == NULL) {
        child = builder->node;
    }
    unsigned count = builder->node->match_count + 1;
    if (!RESERVE(&builder->node->matches, &builder->node_cap, count, Match)) {
        return false;
    }
    Match* match = &builder->node->matches[builder->node->match_count];
    if (exists) {
        match->type = MATCH_EXISTING_FILE;
    } else {
        match->type = MATCH_ANY_FILE;
    }
    match->child = child;
    builder->node->file_ix = count - 1;
    builder->node->match_count = count;

    return true;
}

bool NodeBuilder_add_any(NodeBuilder* builder, MatchNode* child) {
    if (child == NULL) {
        child = builder->node;
    }
    unsigned count = builder->node->match_count + 1;
    if (!RESERVE(&builder->node->matches, &builder->node_cap, count, Match)) {
        return false;
    }
    Match* match = &builder->node->matches[builder->node->match_count];
    match->type = MATCH_ANY;
    builder->node->any_ix = count - 1;
    builder->node->match_count = count;

    return true;
}


void NodeBuilder_abort(NodeBuilder* builder) {
    for (unsigned ix = 0; ix < builder->node->match_count; ++ix) {
        Match* m = &builder->node->matches[ix];
        if (m->type == MATCH_STATIC) {
            WString s;
            s.buffer = m->static_match;
            WString_free(&s);
        }
    }
    if (builder->node->matches != NULL) {
        HeapFree(GetProcessHeap(), 0, builder->node->matches);
    }
    HeapFree(GetProcessHeap(), 0, builder->node);
}


// Creates the node. Adds fixed children to global structures.
// Does not add itself, since this requires knowing parent
MatchNode* NodeBuilder_finalize(NodeBuilder* builder) {
    MatchNode* node = builder->node;

    WString hashbuf;
    WString_create(&hashbuf);

    for (unsigned i = 0; i < node->match_count; ++i) {
        if (node->matches[i].type == MATCH_STATIC) {
            WString_extend(&hashbuf, node->matches[i].static_match);
            WString_append_count(&hashbuf, node->hash_postfix, HASH_POSTFIX_LEN);
            WHashMap_Insert(&gNodes, hashbuf.buffer, &node->matches[i]);
            WString_clear(&hashbuf);
        } else if (node->matches[i].type == MATCH_DYNAMIC) {
            DynamicMatch* dyn = node->matches[i].dynamic_match;
            RESERVE(&dyn->nodes, &dyn->node_capacity, dyn->node_count + 1, *dyn->nodes);
            dyn->nodes[dyn->node_count].parent = builder->node;
            dyn->nodes[dyn->node_count].match = &node->matches[i];
            dyn->node_count += 1;
        }
    }

    WString_free(&hashbuf);

    return node;
}

// Sets all children of <node> to <child>.
void Node_set_child(MatchNode* node, MatchNode* child) {
    for (unsigned ix = 0; ix < node->match_count; ++ix) {
        node->matches[ix].child = child;
    }
}

// Finds next node given complete word <str>
// <str> should have 6 byte space for postfix + null-terminator
// quotes should be gone from str
Match* find_next_match(MatchNode* current, wchar_t* str, unsigned len) {
    // Quick case
    if (current->match_count == 1) {
        return &current->matches[0];
    }
    str[len + 0] = current->hash_postfix[0];
    str[len + 1] = current->hash_postfix[1];
    str[len + 2] = current->hash_postfix[2];
    str[len + 3] = current->hash_postfix[3];
    str[len + 4] = current->hash_postfix[4];
    str[len + 5] = L'\0';
    // This finds all static and dynamic matches
    Match* match = WHashMap_Value(&gNodes, str);
    str[len] = L'\0';
    if (match != NULL) {
        return match;
    }

    if (current->file_ix >= 0) {
        Match* m = &current->matches[current->file_ix];
        if (m->type == MATCH_ANY_FILE && is_filelike(str, len)) {
            return m;
        } else if (is_file(str)) {
            return m;
        }
    }
    if (current->any_ix >= 0) {
        return &current->matches[current->any_ix];
    }

    return NULL;
}

MatchNode* find_final(const wchar_t *cmd, unsigned* offset) {
    WString workbuf;
    WString_create(&workbuf);

    MatchNode* current = gRoot;
    size_t ix = 0;
    size_t len = 0;
    BOOL quoted = FALSE;

    size_t pos = ix;
    unsigned count = 0;

    unsigned options = ARG_OPTION_TERMINAL_OPERANDS | 
                       ARG_OPTION_BACKSLASH_ESCAPE;
    while (get_arg_len(cmd, &pos, &len, &quoted, options)) {
        WString_clear(&workbuf);
        WString_reserve(&workbuf, len + 6);
        get_arg(cmd, &ix, workbuf.buffer, options);

        workbuf.length = len;
        ++count;
        pos = ix;
        if (!quoted && len > 0) {
            if (workbuf.buffer[0] == L'&' || workbuf.buffer[0]  == L'|') {
                current = gRoot;
                continue;
            }
            if (workbuf.buffer[0] == L'<') {
                if (current == gExisting_file || current == gAny_file) {
                    // Make sure complete is not destroyed by writing eg << <<
                    Node_set_child(gExisting_file, current->matches[0].child);
                } else {
                    Node_set_child(gExisting_file, current);
                }
                current = gExisting_file;
                continue;
            } else if (workbuf.buffer[0] == L'>') {
                if (current == gExisting_file || current == gAny_file) {
                    // Make sure complete is not destroyed by writing eg >> >>
                    Node_set_child(gAny_file, current->matches[0].child);
                } else {
                    Node_set_child(gAny_file, current);
                }
                current = gAny_file;
                continue;
            }
        }

        Match* next = find_next_match(current, workbuf.buffer, len);
        if (next != NULL) {
            current = next->child;
        }
    }

    WString_free(&workbuf);
    return current;
}
