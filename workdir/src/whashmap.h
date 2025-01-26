#ifndef WHASHMAP_H_00
#define WHASHMAP_H_00
#undef HASHMAP_H_00

#ifndef HASHMAP_WIDE
#define HASHMAP_WIDE
#endif

#ifdef HASHMAP_WIDE_LINKED
#define HASHMAP_LINKED
#endif

#include "hashmap.h"

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


#ifdef HASHMAP_WIDE_LINKED
#undef HashMapIterator
#undef HashMapIter_Begin
#undef HashMapIter_Next
#endif

#undef HASHMAP_WIDE

#endif
