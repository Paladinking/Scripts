#ifndef LINKED_WHASHMAP_H_00
#define LINKED_WHASHMAP_H_00


#ifdef WHASHMAP_H_00
#undef WHASHMAP_H_00
#define WHASHMAP_H_00_UNDEF
#endif

#define HASHMAP_WIDE_LINKED
#define HASHMAP_LINKED

#include "whashmap.h"

#ifdef WHASHMAP_H_00_UNDEF
#undef WHASHMAP_H_00_UNDEF
#define WHASHMAP_H_00
#endif

#undef HashMapIterator
#undef HashMapIter_Begin
#undef HashMapIter_Next

#undef HASHMAP_WIDE_LINKED
#undef HASHMAP_LINKED

#endif
