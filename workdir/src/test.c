#include "glob.h"
#include "json.h"
#include "printf.h"
#include "hashmap.h"
#include "whashmap.h"
#include "subprocess.h"

enum Invalidation {
    ALWAYS, NEVER, CHDIR
};

#define EXTRA_ANY_ITEM 1
#define EXTRA_EXISTING_FILES 2
#define EXTRA_ANY_FILES 4
#define EXTRA_DEFAULT 8

struct CmpNode;

typedef struct DynamicMatches {
    // First is length of first string, followed by first string
    // After that, length of second string etc...
    wchar_t* matches; // NULL when invalidated
    wchar_t* cmd;
    unsigned match_count;
    enum Invalidation invalidation;
    wchar_t separator;
} DynamicMatches;


typedef struct DynamicNode {
    DynamicMatches* matches;
    struct CmpNode* node;
} DynamicNode;

typedef struct CmpNode {
    // First is length of first string, followed by first string
    // After that, length of second string etc...
    wchar_t* fixed_matches;
    unsigned fixed_count;

    DynamicNode* dynamic;
    unsigned dynamic_count;

    unsigned extra;
    // If extra has EXTRA_ANY_ITEM, contains one child that
    // all matches lead to. All other flags are ignored.
    // If extra has EXTRA_ANY_FILES or EXTRA_EXISTING_FILES, this is one child
    // If extra has EXTRA_DEFAULT, next child is default child.
    struct CmpNode* extra_nodes[2];
    wchar_t hash_postfix[5]; // Prefix used in global hashmap for finding next node
} CmpNode;



WHashMap gNodes;

unsigned gDynamic_count = 0;
unsigned gDynamic_capacity = 0;
DynamicMatches** gDynamic_matches = NULL;
HANDLE gDynamic_thread = NULL;



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


typedef struct NodeBuilder {
    CmpNode* node;
    WString fixed;
    CmpNode** fixed_nodes;
    unsigned node_cap;
    unsigned dynamic_cap;
} NodeBuilder;


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
DynamicMatches* DynamicMatches_create(wchar_t* cmd, enum Invalidation invalidation, wchar_t sep) {
    DynamicMatches* n = HeapAlloc(GetProcessHeap(), 0, sizeof(DynamicMatches));
    if (n == NULL) {
        return NULL;
    }
    n->match_count = 0;
    n->matches = NULL;
    n->invalidation = invalidation;
    n->separator = sep;
    n->cmd = cmd;
    if (!RESERVE(&gDynamic_matches, &gDynamic_capacity, gDynamic_count + 1, DynamicMatches*)) {
        HeapFree(GetProcessHeap(), 0, n);
        return NULL;
    }
    gDynamic_matches[gDynamic_count] = n;
    gDynamic_count += 1;
    return n;
}


void DynamicMatches_evaluate(DynamicMatches* ptr) {
    // Already valid
    if (ptr->cmd != NULL) {
        return;
    }
    WString buf;
    WString_create(&buf);
}
 

bool Node_create(NodeBuilder* builder) {
    static uint32_t postfix = 0;

    CmpNode* node = HeapAlloc(GetProcessHeap(), 0, sizeof(CmpNode));
    if (node == NULL) {
        return false;
    }

    uint32_t b[2] = {postfix, postfix};
    node->hash_postfix[0] = L'\xc';
    memcpy(node->hash_postfix + 1, &b, 4 * sizeof(wchar_t));
    ++postfix;

    node->fixed_matches = NULL;
    node->fixed_count = 0;
    node->dynamic = NULL;
    node->dynamic_count = 0;
    node->extra = 0;
    node->extra_nodes[0] = NULL;
    node->extra_nodes[1] = NULL;

    builder->node = node;
    if (!WString_create(&builder->fixed)) {
        HeapFree(GetProcessHeap(), 0, node);
        return false;
    }
    builder->node_cap = 0;
    builder->dynamic_cap = 0;
    builder->fixed_nodes = NULL;

    return true;
}


bool Node_add_fixed(NodeBuilder* builder, const wchar_t* str, unsigned len, CmpNode* child) {
    if (len > 0xffff || builder->node->extra & EXTRA_ANY_ITEM) {
        return false;
    }
    if (child == NULL) {
        child = builder->node;
    }
    if (!RESERVE(&builder->fixed_nodes, &builder->node_cap, builder->node->fixed_count + 1, CmpNode*)) {
        return false;
    }

    if (!WString_append(&builder->fixed, len)) {
        return false;
    }
    if (!WString_append_count(&builder->fixed, str, len)) {
        WString_pop(&builder->fixed, 1);
        return false;
    }
    builder->fixed_nodes[builder->node->fixed_count] = child;
    builder->node->fixed_count += 1;

    return true;
}

bool Node_add_dynamic(NodeBuilder* builder, DynamicMatches* dyn, CmpNode* child) {
    // Any item may not have other matches
    if (builder->node->extra & EXTRA_ANY_ITEM) {
        return false;
    }
    if (child == NULL) {
        child = builder->node;
    }
    unsigned dynamic = builder->node->dynamic_count;
    if (!RESERVE(&builder->node->dynamic, &builder->dynamic_cap, dynamic + 1, DynamicNode)) {
        return false;
    }
    builder->node->dynamic[dynamic].node = child;
    builder->node->dynamic[dynamic].matches = dyn;

    builder->node->dynamic_count += 1;
    return true;
}

bool Node_add_files(NodeBuilder* builder, bool exists, CmpNode* child) {
    if (builder->node->extra & (EXTRA_ANY_ITEM | EXTRA_ANY_FILES | EXTRA_EXISTING_FILES)) {
        return false;
    }
    if (child == NULL) {
        child = builder->node;
    }
    builder->node->extra_nodes[0] = child;
    if (exists) {
        builder->node->extra |= EXTRA_EXISTING_FILES;
    } else {
        builder->node->extra |= EXTRA_ANY_FILES;
    }
    return true;
}

bool Node_add_default(NodeBuilder* builder, CmpNode* child) {
    if (builder->node->extra & (EXTRA_ANY_ITEM | EXTRA_DEFAULT)) {
        return false;
    }
    if (child == NULL) {
        child = builder->node;
    }
    builder->node->extra_nodes[1] = child;
    builder->node->extra |= EXTRA_DEFAULT;
    return true;
}

bool Node_add_any(NodeBuilder* builder, CmpNode* child) {
    if (builder->node->dynamic_count != 0 || builder->node->fixed_count != 0 || builder->node->extra != 0) {
        return false;
    }
    if (child == NULL) {
        child = builder->node;
    }
    builder->node->extra_nodes[0] = child;
    builder->node->extra = EXTRA_ANY_ITEM;
    return true;
}


void Node_abort(NodeBuilder* builder) {
    WString_free(&builder->fixed);
    HeapFree(GetProcessHeap(), 0, builder->fixed_nodes);
}


// Creates the node. Adds fixed children to global structures.
// Does not add itself, since this requires knowing parent
CmpNode* Node_finalize(NodeBuilder* builder) {
    CmpNode* node = builder->node;
    node->fixed_matches = builder->fixed.buffer;
    builder->fixed.buffer = NULL;

    WString hashbuf;
    WString_create(&hashbuf);

    unsigned ix = 0;
    for (unsigned i = 0; i < node->fixed_count; ++i) {
        wchar_t count = node->fixed_matches[ix];
        ++ix;
        WString_append_count(&hashbuf, node->fixed_matches + ix, count);
        WString_append_count(&hashbuf, node->hash_postfix, 5);
        WHashMap_Insert(&gNodes, hashbuf.buffer, builder->fixed_nodes[i]);
        ix += count;
        WString_clear(&hashbuf);
    }

    WString_free(&hashbuf);
    HeapFree(GetProcessHeap(), 0, builder->fixed_nodes);

    return node;
}

// str should have 6 byte space for postfix + null-terminator
// quotes should be gone from str
CmpNode* find_next_node(CmpNode* current, wchar_t* str, unsigned len) {
    if (current->extra & EXTRA_ANY_ITEM) {
        return current->extra_nodes[0];
    }
    str[len + 0] = current->hash_postfix[0];
    str[len + 1] = current->hash_postfix[1];
    str[len + 2] = current->hash_postfix[2];
    str[len + 3] = current->hash_postfix[3];
    str[len + 4] = current->hash_postfix[4];
    str[len + 5] = L'\0';
    // This finds all static and dynamic matches
    CmpNode* match = WHashMap_Value(&gNodes, str);
    str[len] = L'\0';
    if (match != NULL) {
        return match;
    }

    if (current->extra & EXTRA_ANY_FILES) {
        if (is_filelike(str, len)) {
            return current->extra_nodes[0];
        }
    } else if (current->extra & EXTRA_EXISTING_FILES) {
        if (is_file(str)) {
            return current->extra_nodes[0];
        }
    }
    if (current->extra & EXTRA_DEFAULT) {
        return current->extra_nodes[1];
    }
    return current;
}

CmpNode* root;

int main() {
    WHashMap_Create(&gNodes);

    NodeBuilder b;
    Node_create(&b);
    Node_add_files(&b, false, NULL);
    CmpNode* git_add = Node_finalize(&b);

    Node_create(&b);
    CmpNode* git_status = Node_finalize(&b);

    Node_create(&b);
    Node_add_fixed(&b, L"add", wcslen(L"add"), git_add);
    Node_add_fixed(&b, L"status", wcslen(L"status"), git_status);
    CmpNode* git = Node_finalize(&b);

    Node_create(&b);
    Node_add_files(&b, true, NULL);
    CmpNode* grep = Node_finalize(&b);
    
    Node_create(&b);
    Node_add_fixed(&b, L"grep", wcslen(L"grep"), grep);
    Node_add_fixed(&b, L"git", wcslen(L"git"), git);
    Node_add_files(&b, true, NULL);

    root = Node_finalize(&b);

    _wprintf(L"Created nodes\n");

    wchar_t str[20] = L"git";
    CmpNode* node = find_next_node(root, str, 3);

    _wprintf(L"%*.*s\n", node->fixed_matches[0], node->fixed_matches[0], node->fixed_matches + 1);


    String res;
    String_create(&res);
    if (!subprocess_run(L"git for-each-ref --format=%(refname:short) refs/heads", &res, 1000)) {
        _wprintf(L"Error\n");
    }
    _wprintf(L"%S", res.buffer);

    String json_str;
    if (!read_text_file(&json_str, L"autocmp.json")) {
        return 1;
    }


    JsonObject obj;
    if (!json_parse_object(json_str.buffer, &obj, NULL)) {
        _wprintf(L"Failed parsing json file\n");
        String_free(&json_str);
        return 1;
    }
    String_free(&json_str);

    JsonList* desc = JsonObject_get_list(&obj, "description");
    if (desc != NULL) {
        for (DWORD i = 0; i < desc->size; ++i) {
            String* line = JsonList_get_string(desc, i);
            if (line != NULL) {
                _wprintf(L"%S\n", line->buffer);
            }
        }
    }


    return 0;
    WString s;
    WString_create(&s);
    get_workdir(&s);
    HashMap m1;
    WHashMap m2;
    
    HashMap_Create(&m1);
    WHashMap_Create(&m2);

    WalkCtx ctk;
    WalkDir_begin(&ctk, s.buffer);
    Path* path;
    while (WalkDir_next(&ctk, &path) > 0) {
        String s;
        String_create(&s);
        String_from_utf16_str(&s, path->path.buffer);
        HashMap_Insert(&m1, s.buffer, (void*)path->is_dir);
        String_free(&s);
        WHashMap_Insert(&m2, path->path.buffer, (void*)path->is_dir);
        _wprintf(L"%s: %u\n", path->path.buffer, path->is_dir);
    }
    _wprintf(L"-----------------------------\n");

    WalkDir_begin(&ctk, s.buffer);
    while (WalkDir_next(&ctk, &path) > 0) {
        String s;
        String_create(&s);
        String_from_utf16_str(&s, path->path.buffer);
        bool dir = (bool)WHashMap_Value(&m2, path->path.buffer);
        bool dir2 = (bool)HashMap_Value(&m1, s.buffer);
        _wprintf(L"%s: %u %u %u\n", path->path.buffer, path->is_dir, dir, dir2);
        String_free(&s);
    }
    WHashMapFrozen m3;
    WHashMap_Freeze(&m2, &m3);
    WHashMap_Free(&m2);
    HashMapFrozen m4;
    HashMap_Freeze(&m1, &m4);
    HashMap_Free(&m1);

    _wprintf(L"-----------------------------\n");
    WalkDir_begin(&ctk, s.buffer);
    while (WalkDir_next(&ctk, &path) > 0) {
        String s;
        String_create(&s);
        String_from_utf16_str(&s, path->path.buffer);
        bool dir = (bool)WHashMap_Value(&m3.map, path->path.buffer);
        bool dir2 = (bool)HashMap_Value(&m4.map, s.buffer);
        _wprintf(L"%s: %u %u %u\n", path->path.buffer, path->is_dir, dir, dir2);
        String_free(&s);
    }
    WHashElement *e = WHashMap_Find(& m3.map, L"JSON.EXE");
    if (e == NULL) {
        _wprintf(L"null\n");
    } else {
        _wprintf(L"%s: %u\n", e->key, (bool)e->value);
    }

    WHashMap_FreeFrozen(&m3);
    HashMap_FreeFrozen(&m4);

    return 0;
}
