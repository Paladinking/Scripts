#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#include "hashmap.h"

#ifdef HASHMAP_ALLOC_ERROR
#define CHECKED_CALL(c) if (!(c)) {return 0;}
#define CHECKED_ALLOC(name, size) do {name = HASHMAP_ALLOC_FN(size); if (name == NULL) { return 0; } } while(0)
#else
#define CHECKED_CALL(c) c
#define CHECKED_ALLOC(name, size) name = HASHMAP_ALLOC_FN(size)
#endif

void HashMap_Free(HashMap* map) {
    for (uint32_t i = 0; i < map->bucket_count; ++i) {
        for (uint32_t j = 0; j < map->buckets[i].size; ++j) {
            HASHMAP_FREE_FN((ckey_t*)map->buckets[i].data[j].key);
        } 
        HASHMAP_FREE_FN(map->buckets[i].data);
    }
    map->element_count = 0;
    map->bucket_count = 0;
#ifdef HASHMAP_LINKED
    map->first_bucket_ix = 0;
    map->first_elem_ix = 0;
    map->last_bucket_ix = 0;
    map->last_elem_ix = 0;
#endif
    HASHMAP_FREE_FN(map->buckets);
    map->buckets = NULL;
}

int HashMap_Allocate(HashMap* map, uint32_t bucket_count) {
    map->element_count = 0;
    map->bucket_count = bucket_count;
    map->buckets = HASHMAP_ALLOC_FN(bucket_count * sizeof(HashBucket));
#ifdef HASHMAP_LINKED
    map->first_bucket_ix = 0;
    map->first_elem_ix = 0;
    map->last_bucket_ix = 0;
    map->last_elem_ix = 0;
#endif
#ifdef HASHMAP_ALLOC_ERROR
    if (map->buckets == NULL) {
        map->bucket_count = 0;
        return 0;
    }
#endif
    for (uint32_t i = 0; i < bucket_count; ++i) {
        map->buckets[i].size = 0;
        map->buckets[i].capacity = HASHMAP_INIT_BUCKET_CAP;
        map->buckets[i].data = HASHMAP_ALLOC_FN(HASHMAP_INIT_BUCKET_CAP * sizeof(HashElement));
#ifdef HASHMAP_ALLOC_ERROR
        if (map->buckets[i].data == NULL) {
            map->bucket_count = i;
            HashMap_Free(map);
            return 0;
        }
#endif
    }
    return 1;
}

void HashMap_Clear(HashMap* map) {
    for (uint32_t i = 0; i < map->bucket_count; ++i) {
        for (uint32_t j = 0; j < map->buckets[i].size; ++j) {
            HASHMAP_FREE_FN((ckey_t*)map->buckets[i].data[j].key);
        }
        map->buckets[i].size = 0;
    }
    map->element_count = 0;
#ifdef HASHMAP_LINKED
    map->first_bucket_ix = 0;
    map->first_elem_ix = 0;
    map->last_bucket_ix = 0;
    map->last_elem_ix = 0;
#endif
}

int HashMap_Create(HashMap* map) {
    return HashMap_Allocate(map, HASHMAP_INIT_BUCKETS);
}

static uint64_t hash(const ckey_t* str) {
    uint64_t hash = 5381;
    int c;
    while ((c = *str++)) {
#ifdef HASHMAP_CASE_INSENSITIVE
#ifdef HASHMAP_WIDE
        c = towlower(c);
#else
        c = tolower(c);
#endif
#endif
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}

static HashElement* HashMap_GetElement(HashMap* map, const ckey_t* key, HashBucket** bucket) {
    uint64_t h = hash(key);
    *bucket = &map->buckets[h % map->bucket_count];
    for (uint32_t i = 0; i < (*bucket)->size; ++i) {
        if (keycmp(key, (*bucket)->data[i].key) == 0) {
            return &((*bucket)->data[i]);
        }
    }
    return NULL;
}

static int HashMap_Rehash(HashMap* map) {
    HashMap tmp;
    CHECKED_CALL(HashMap_Allocate(&tmp, map->bucket_count * 2));
    for (uint32_t b = 0; b < map->bucket_count; ++b) {
        for (uint32_t ix = 0; ix < map->buckets[b].size; ++ix) {
            int status = HashMap_Insert(&tmp, map->buckets[b].data[ix].key, map->buckets[b].data[ix].value);
#ifdef HASHMAP_ALLOC_ERROR
            if (!status) {
                HashMap_Free(&tmp);
                return 0;
            }
#endif
        }
    }
    HashMap_Free(map);
    *map = tmp;
    return 1;
}

static int HashMap_AddElement(HashMap* map, HashBucket* bucket, HashElement element) {
    if (bucket->size == bucket->capacity) {
        HashElement* new_data;
        CHECKED_ALLOC(new_data, bucket->size * 2 * sizeof(HashElement));
        memcpy(new_data, bucket->data, bucket->size * sizeof(HashElement));
        HASHMAP_FREE_FN(bucket->data);
        bucket->data = new_data;
        bucket->capacity = bucket->size * 2;
    }
#ifdef HASHMAP_LINKED
    uint32_t bucket_ix = bucket - map->buckets;
    if (map->element_count == 0) {
        map->first_bucket_ix = bucket_ix;
        map->first_elem_ix = bucket->size;
    } else {
        element.prev_bucket_ix = map->last_bucket_ix;
        element.prev_elem_ix = map->last_elem_ix;
        HashElement* last = &map->buckets[map->last_bucket_ix].data[map->last_elem_ix];
        last->next_bucket_ix = bucket_ix;
        last->next_elem_ix = bucket->size;
    }
    map->last_bucket_ix = bucket_ix;
    map->last_elem_ix = bucket->size;
#endif
    memcpy(bucket->data + bucket->size, &element, sizeof(HashElement));
    ++(bucket->size);
    ++(map->element_count);
    return 1;
}

int HashMap_Insert(HashMap* map, const ckey_t* key, void* value) {
    if (map->bucket_count == map->element_count) {
        CHECKED_CALL(HashMap_Rehash(map));
    }
    HashBucket* bucket;
    HashElement* elem = HashMap_GetElement(map, key, &bucket);
    if (elem != NULL) {
        elem->value = value;
        return 1;
    }
    uint32_t len = keylen(key);
    ckey_t* buf;
    CHECKED_ALLOC(buf, (len + 1) * sizeof(ckey_t));
    memcpy(buf, key, (len + 1) * sizeof(ckey_t));
    HashElement he = {buf, value};
    CHECKED_CALL(HashMap_AddElement(map, bucket, he));
    return 1;
}

HashElement* HashMap_Get(HashMap* map, const ckey_t* key) {
    if (map->bucket_count == map->element_count) {
        CHECKED_CALL(HashMap_Rehash(map));
    }
    HashBucket* bucket;
    HashElement* elem = HashMap_GetElement(map, key, &bucket);
    if (elem != NULL) {
        return elem;
    }
    uint32_t len = keylen(key);
    ckey_t* buf;
    CHECKED_ALLOC(buf, (len + 1) * sizeof(ckey_t));
    memcpy(buf, key, (len + 1) * sizeof(ckey_t));
    HashElement he = {buf, NULL};
    CHECKED_CALL(HashMap_AddElement(map, bucket, he));
    return &(bucket->data[bucket->size - 1]);
}

HashElement* HashMap_Find(HashMap* map, const ckey_t* key) {
    HashBucket* bucket;
    return HashMap_GetElement(map, key, &bucket);
}

void* HashMap_Value(HashMap* map, const ckey_t* key) {
    HashBucket *bucket;
    HashElement* element = HashMap_GetElement(map, key, &bucket);
    if (element == NULL) {
        return NULL;
    }
    return element->value;
}

static void HashMap_RemoveElement(HashMap* map, HashBucket* bucket, HashElement* element) {
    uint32_t elem_ix = element - bucket->data;
#ifdef HASHMAP_LINKED
    uint32_t bucket_ix = bucket - map->buckets;
    if (bucket_ix == map->first_bucket_ix && elem_ix == map->first_elem_ix) {
        map->first_bucket_ix = element->next_bucket_ix;
        map->first_elem_ix = element->next_elem_ix;
    } else {
        HashElement* prev = &map->buckets[element->prev_bucket_ix].data[element->prev_elem_ix];
        prev->next_bucket_ix = element->next_bucket_ix;
        prev->next_elem_ix = element->next_elem_ix;
    }
    if (bucket_ix == map->last_bucket_ix && elem_ix == map->last_elem_ix) {
        map->last_bucket_ix = element->prev_bucket_ix;
        map->last_elem_ix = element->prev_elem_ix;
    } else {
        HashElement* next = &map->buckets[element->next_bucket_ix].data[element->next_elem_ix];
        next->prev_bucket_ix = element->prev_bucket_ix;
        next->prev_elem_ix = element->prev_elem_ix;
    }

#endif
    HASHMAP_FREE_FN((char*)element->key);
    memmove(element, element + 1, (bucket->size - elem_ix - 1) * sizeof(HashElement));
    --bucket->size;
    --map->element_count;
}

int HashMap_Remove(HashMap* map, const ckey_t* key) {
    HashBucket* bucket;
    HashElement* element = HashMap_GetElement(map, key, &bucket);
    if (element == NULL) {
        return 0;
    }
    HashMap_RemoveElement(map, bucket, element);
    return 1;
}

int HashMap_RemoveGet(HashMap* map, const ckey_t* key, void** old_val) {
    HashBucket* bucket;
    HashElement* element = HashMap_GetElement(map, key, &bucket);
    if (element == NULL) {
        return 0;
    }
    *old_val = element->value;
    HashMap_RemoveElement(map, bucket, element);
    return 1;
}

int HashMap_Freeze(const HashMap* map, HashMapFrozen *out) {
#ifdef HASHMAP_LINKED
    return 0; // TODO: support this
#else
    uint64_t size = sizeof(HashBucket) * map->bucket_count + sizeof(HashElement) * map->element_count;
    for (uint32_t i = 0; i < map->bucket_count; ++i) {
        for (uint32_t j = 0; j < map->buckets[i].size; ++j) {
            HashElement* elem = &map->buckets[i].data[j];
            size += (keylen(elem->key) + 1) * sizeof(ckey_t);
            /*if (elem->value != NULL) {
                size += strlen(elem->value) + 1;
            }*/
        }
    }

    unsigned char* ptr;
    CHECKED_ALLOC(ptr, size);
    unsigned char* bucket_ptr = ptr;
    unsigned char* elem_ptr = ptr + map->bucket_count * sizeof(HashBucket);
    unsigned char* str_ptr = elem_ptr + map->element_count * sizeof(HashElement);
    for (uint32_t i = 0; i < map->bucket_count; ++i) {
        HashBucket* old_bucket = &map->buckets[i];
        HashBucket bucket = {(HashElement*)elem_ptr, old_bucket->size, old_bucket->size};
        memcpy(bucket_ptr, &bucket, sizeof(HashBucket));
        bucket_ptr += sizeof(HashBucket);
        for (uint32_t j = 0; j < old_bucket->size; ++j) {
            uint32_t key_len = keylen(old_bucket->data[j].key) + 1;
            memcpy(str_ptr, old_bucket->data[j].key, key_len * sizeof(ckey_t));
            uint32_t val_len = 0;
            HashElement e = {(ckey_t*)str_ptr, old_bucket->data[j].value};
            /*if (old_bucket->data[j].value != NULL) {
                val_len = strlen(old_bucket->data[j].value) + 1;
                memcpy(str_ptr + key_len, old_bucket->data[j].value, val_len);
                e.value = str_ptr + key_len;
            }*/
            memcpy(elem_ptr, &e, sizeof(e));
            elem_ptr += sizeof(e);
            str_ptr += key_len * sizeof(ckey_t) + val_len;
        }
    }

    out->map.buckets = (HashBucket*)ptr;
    out->map.bucket_count = map->bucket_count;
    out->map.element_count = map->element_count;
    out->data_size = size;

    return 1;
#endif
}

void HashMap_FreeFrozen(HashMapFrozen *map) {
    HASHMAP_FREE_FN(map->map.buckets);
    map->map.element_count = 0;
    map->map.bucket_count = 0;
    map->data_size = 0;
}


#ifdef HASHMAP_LINKED
void HashMapIter_Begin(HashMapIterator* it, HashMap* map) {
    it->map = map;
    it->ix = 0;
    it->bucket_ix = map->first_bucket_ix;
    it->elem_ix = map->first_elem_ix;
}

HashElement* HashMapIter_Next(HashMapIterator* it) {
    if (it->ix >= it->map->element_count) {
        return NULL;
    }
    HashElement* elem = &it->map->buckets[it->bucket_ix].data[it->elem_ix];
    it->ix += 1;
    it->bucket_ix = elem->next_bucket_ix;
    it->elem_ix = elem->next_elem_ix;

    return elem;
}
#endif
