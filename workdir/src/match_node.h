#ifndef MATCH_NODE_H_00
#define MATCH_NODE_H_00

#include "glob.h"
#include "whashmap.h"

enum Invalidation {
    INVALID_ALWAYS, INVALID_NEVER, INVALID_CHDIR
};

#define EXTRA_ANY_ITEM 1
#define EXTRA_EXISTING_FILES 2
#define EXTRA_ANY_FILES 4
#define EXTRA_DEFAULT 8

#define HASH_POSTFIX_LEN 5

struct MatchNode;
struct Match;

typedef struct DynamicMatch {
    // First is length of first string, followed by first string
    // After that, length of second string etc...
    wchar_t** matches; // NULL when invalidated
    wchar_t* cmd;
    unsigned match_count;

    // List of postfix and child for all nodes contaning this 
    struct {
        struct MatchNode* parent;
        struct Match* match;
    }* nodes;
    unsigned node_count;
    unsigned node_capacity;

    enum Invalidation invalidation;
    wchar_t separator;
} DynamicMatch;

typedef struct Match {
    enum {
        MATCH_DYNAMIC, MATCH_STATIC, MATCH_EXISTING_FILE, MATCH_ANY_FILE, MATCH_ANY
    } type;

    union {
       DynamicMatch* dynamic_match;
       wchar_t* static_match; // NULL-terminated string
    };
    struct MatchNode* child;
} Match;

typedef struct MatchNode {
    struct Match* matches;
    unsigned match_count;

    int file_ix;
    int any_ix;

    wchar_t hash_postfix[HASH_POSTFIX_LEN]; // Postfix used in global hashmap for finding next node
} MatchNode;

typedef struct NodeIterator {
    MatchNode* node;
    unsigned ix;
    unsigned dyn_ix;
    bool walk_ongoing;
    WalkCtx walk_ctx;
    WString prefix;
    wchar_t* dir_separator;
} NodeIterator;

typedef struct NodeBuilder {
    MatchNode* node;
    unsigned node_cap;
} NodeBuilder;


extern WHashMap gNodes;
extern DynamicMatch** gDynamic_matches;
extern MatchNode* gRoot; // Root of autocomplete graph
extern MatchNode* gAny_file; // Special node used for '>'
extern MatchNode* gExisting_file; // Special node used for '<'

void MatchNode_init();
void MatchNode_free();

void MatchNode_set_root(MatchNode* root);


DynamicMatch* DynamicMatch_create(wchar_t* cmd, enum Invalidation invalidation, wchar_t sep);

void DynamicMatch_evaluate(DynamicMatch* ptr);

void DynamicMatch_invalidate(DynamicMatch* ptr);

void DynamicMatch_invalidate_many(bool chdir);


void NodeIterator_begin(NodeIterator* it, MatchNode* node, const wchar_t* prefix);

const wchar_t* NodeIterator_next(NodeIterator* it);

void NodeIterator_stop(NodeIterator* it);

void NodeIterator_restart(NodeIterator* it);


bool NodeBuilder_create(NodeBuilder* builder);

bool NodeBuilder_add_fixed(NodeBuilder* builder, const wchar_t* str, unsigned len, MatchNode* child);

bool NodeBuilder_add_dynamic(NodeBuilder* builder, DynamicMatch* dyn, MatchNode* child);

bool NodeBuilder_add_files(NodeBuilder* builder, bool exists, MatchNode* child);

bool NodeBuilder_add_any(NodeBuilder* builder, MatchNode* child);

void NodeBuilder_abort(NodeBuilder* builder);

MatchNode* NodeBuilder_finalize(NodeBuilder* builder);

void Node_set_child(MatchNode* node, MatchNode* child);


Match* find_next_match(MatchNode* current, wchar_t* str, unsigned len);

// Find MatchNode* that should be used for autocomplete for cmd
// <offset> will be set to ix in cmd contaning arg to complete.
// <rem> will contain *only* final argument to complete.
// <offset> is at end and <rem> empty in case cmd ends with space.
MatchNode* find_final(const wchar_t *cmd, size_t* offset, WString* rem);

#endif // MATCH_NODE_H_00
