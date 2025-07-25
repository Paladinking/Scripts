#ifndef VARSET_H
#define VARSET_H
#include "tables.h"

typedef struct VarSet {
    var_id max;
    uint64_t* bitmap;
} VarSet;

VarSet VarSet_create(var_id max);

void VarSet_clearall(VarSet* s);

// Compute dest = dest U src
void VarSet_union(VarSet* dest, const VarSet* src);

// Copy src into dest
void VarSet_copy(VarSet* dest, const VarSet* src);

// Compute dest = dest - src
void VarSet_diff(VarSet* dest, const VarSet* src);

void VarSet_grow(VarSet* s, var_id new_max);

void VarSet_free(VarSet* s);

// Return smallest value in the set, or VAR_ID_INVALID
var_id VarSet_get(VarSet* s);

// Return smallest value in the set bigger than last, or VAR_ID_INVALID
var_id VarSet_getnext(VarSet* s, var_id last);

bool VarSet_equal(const VarSet* a, const VarSet* b);

static inline void VarSet_set(VarSet* s, var_id v) {
    assert(v < s->max);
    s->bitmap[v >> 6] |= ((uint64_t)1 << ((uint64_t)v & 0b111111));
}

static inline void VarSet_clear(VarSet* s, var_id v) {
    assert(v < s->max);
    s->bitmap[v >> 6] &= ~((uint64_t)1 << ((uint64_t)v & 0b111111));
}

static inline bool VarSet_contains(const VarSet* s, var_id v) {
    assert(v < s->max);
    return s->bitmap[v >> 6] & ((uint64_t)1 << ((uint64_t)v & 0b111111));
}


#endif
