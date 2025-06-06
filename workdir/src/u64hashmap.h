#ifndef U64HASHMAP_H_00
#define U64HASHMAP_H_00

#define HASHMAP_U64

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

#undef keyequal
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

#undef HASHMAP_U64

#endif
