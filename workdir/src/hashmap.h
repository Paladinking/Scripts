#ifndef HASHMAP_H_00
#define HASHMAP_H_00

#include <stdint.h>
#include "mem.h"

#define HASHMAP_PROCESS_HEAP

#define HASHMAP_ALLOC_ERROR
#define HASHMAP_INIT_BUCKETS 4
#define HASHMAP_INIT_BUCKET_CAP 4
#ifndef HASHMAP_ALLOC_FN
#ifdef HASHMAP_PROCESS_HEAP
#define HASHMAP_ALLOC_FN(size) Mem_alloc(size)
#else
#include <stdlib.h>
#define HASHMAP_ALLOC_FN(size) malloc((size))
#endif
#endif
#ifndef HASHMAP_FREE_FN
#ifdef HASHMAP_PROCESS_HEAP
#define HASHMAP_FREE_FN(ptr) Mem_free(ptr)
#else
#define HASHMAP_FREE_FN(ptr) free((ptr))
#endif
#endif
#ifndef HASHMAP_REALLOC_FN
#ifdef HASHMAP_PROCESS_HEAP
#define HASHMAP_REALLOC_FN(ptr, size) Mem_realloc((ptr), (size))
#else
#define HASHMAP_REALLOC_FN(ptr, size) realloc((ptr), (size))
#endif
#endif

#undef keycmp
#undef keylen
#undef ckey_t

#ifdef HASHMAP_WIDE

#ifdef HASHMAP_LINKED
#ifndef HASHMAP_WIDE_LINKED
#undef HASHMAP_LINKED
#define HASHMAP_WIDE_UNLINKED
#endif
#endif

#define ckey_t wchar_t
#ifdef HASHMAP_CASE_INSENSITIVE
#define keycmp(a, b) _wcsicmp(a, b)
#else
#define keycmp(a, b) wcscmp(a, b)
#endif
#define keylen(k) wcslen(k)
#define HashElement WHashElement
#define HashBucket WHashBucket
#define HashMap WHashMap
#define HashMapFrozen WHashMapFrozen

#define HashMap_Free WHashMap_Free
#define HashMap_Allocate WHashMap_Allocate
#define HashMap_Clear WHashMap_Clear
#define HashMap_Create WHashMap_Create
#define HashMap_Insert WHashMap_Insert
#define HashMap_Find WHashMap_Find
#define HashMap_Get WHashMap_Get
#define HashMap_Value WHashMap_Value
#define HashMap_Remove WHashMap_Remove
#define HashMap_RemoveGet WHashMap_RemoveGet
#define HashMap_Freeze WHashMap_Freeze
#define HashMap_FreeFrozen WHashMap_FreeFrozen

#ifdef HASHMAP_LINKED
#define HashMapIterator WHashMapIterator
#define HashMapIter_Begin WHashMapIter_Begin
#define HashMapIter_Next WHashMapIter_Next
#endif 

#else

#define ckey_t char
#ifdef HASHMAP_CASE_INSENSITIVE
#define keycmp(a, b) _stricmp(a, b)
#else
#define keycmp(a, b) strcmp(a, b)
#endif
#define keylen(k) strlen(k)
#endif


typedef struct HashElement {
    const ckey_t* const key;
    void* value;
#ifdef HASHMAP_LINKED
    uint32_t next_bucket_ix;
    uint32_t next_elem_ix;
    uint32_t prev_bucket_ix;
    uint32_t prev_elem_ix;
#endif
} HashElement;

typedef struct HashBucket {
    HashElement* data;
    uint32_t size;
    uint32_t capacity;
} HashBucket;

typedef struct HashMap {
    HashBucket* buckets;
    uint32_t bucket_count;
    uint32_t element_count;
#ifdef HASHMAP_LINKED
    uint32_t first_bucket_ix;
    uint32_t first_elem_ix;
    uint32_t last_bucket_ix;
    uint32_t last_elem_ix;
#endif
} HashMap;

typedef struct HashMapFrozen {
    HashMap map;

    uint64_t data_size;
} HashMapFrozen;


void HashMap_Free(HashMap* map);

int HashMap_Allocate(HashMap* map, uint32_t bucket_count);

void HashMap_Clear(HashMap* map);

int HashMap_Create(HashMap* map);

int HashMap_Insert(HashMap* map, const ckey_t* key, void* value);

HashElement* HashMap_Find(HashMap* map, const ckey_t* key);

HashElement* HashMap_Get(HashMap* map, const ckey_t* key);

void* HashMap_Value(HashMap* map, const ckey_t* key);

int HashMap_Remove(HashMap* map, const ckey_t* key);

int HashMap_RemoveGet(HashMap* map, const ckey_t* key, void** old_val);

int HashMap_Freeze(const HashMap* map, HashMapFrozen* out);

void HashMap_FreeFrozen(HashMapFrozen* map);


#ifdef HASHMAP_LINKED

typedef struct HashMapIterator {
    HashMap* map;
    uint32_t bucket_ix;
    uint32_t elem_ix;
    uint32_t ix;
} HashMapIterator;

void HashMapIter_Begin(HashMapIterator* it, HashMap* map);

HashElement* HashMapIter_Next(HashMapIterator* it);

#endif

#ifdef HASHMAP_WIDE_UNLINKED
#undef HASHMAP_WIDE_UNLINKED
#define HASHMAP_LINKED
#endif

#endif
