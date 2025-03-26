#ifndef LINKED_HASHMAP_00
#define LINKED_HASHMAP_00

#define HASHMAP_LINKED

#ifdef HASHMAP_H_00
#undef HASHMAP_H_00
#define HASHMAP_H_00_UNDEF
#endif

#include "hashmap.h"

#ifdef HASHMAP_H_00_UNDEF
#define HASHMAP_H_00
#undef HASHMAP_H_00_UNDEF
#else
#undef HASHMAP_H_00
#endif

#undef keycmp
#undef keylen
#undef HashElement
#undef HashBucket
#undef HashMap
#undef HashMapFrozen

#undef HashMap_Free
#undef HashMap_Allocate
#undef HashMap_Clear
#undef HashMap_Create
#undef HashMap_Insert
#undef HashMap_Find
#undef HashMap_Get
#undef HashMap_Value
#undef HashMap_Remove
#undef HashMap_RemoveGet
#undef HashMap_Freeze
#undef HashMap_FreeFrozen

#undef HASHMAP_LINKED

#endif
