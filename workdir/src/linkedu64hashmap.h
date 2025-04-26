#ifndef LINKED_U64HASHMAP_H_00
#define LINKED_U64HASHMAP_H_00


#ifdef U64HASHMAP_H_00
#undef U64HASHMAP_H_00
#define U64HASHMAP_H_00_UNDEF
#endif

#define HASHMAP_U64_LINKED
#define HASHMAP_LINKED

#include "u64hashmap.h"

#ifdef U64HASHMAP_H_00_UNDEF
#undef U64HASHMAP_H_00_UNDEF
#define U64HASHMAP_H_00
#else
#undef U64HASHMAP_H_00
#endif

#undef HashMapIterator
#undef HashMapIter_Begin
#undef HashMapIter_Next

#undef HASHMAP_U64_LINKED
#undef HASHMAP_LINKED

#endif
