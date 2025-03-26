#include "args.h"
#include "coff.h"
#include "glob.h"
#include "hashmap.h"
#include "mem.h"
#include "path_utils.h"
#include "printf.h"
#include "stdbool.h"
#include "whashmap.h"

struct FrozenMap {
    HashMap map;
    uint8_t data[];
};

#define WRITE_U64(file, u)                                           \
    do {                                                             \
        DWORD w;                                                     \
        uint8_t b[8] = {(u) & 0xFF,         ((u) >> 8) & 0xFF,       \
                        ((u) >> 16) & 0xFF, ((u) >> 24) & 0xFF,      \
                        ((u) >> 32) & 0xFF, ((u) >> 40) & 0xFF,      \
                        ((u) >> 48) & 0xFF, ((u) >> 56) & 0xFF};     \
        if (!WriteFile(file, b, 8, &w, NULL) || w != 8) {            \
            CloseHandle(file);                                       \
            return false;                                            \
        }                                                            \
    } while (1)

#define WRITE_U32(file, u)                                           \
    do {                                                             \
        DWORD w;                                                     \
        uint8_t b[4] = {(u) & 0xFF,         ((u) >> 8) & 0xFF,       \
                        ((u) >> 16) & 0xFF, ((u) >> 24) & 0xFF};     \
        if (!WriteFile(file, b, 4, &w, NULL) || w != 4) {            \
            CloseHandle(file);                                       \
            return false;                                            \
        }                                                            \
    } while (1)

static inline bool read_u64(HANDLE file, uint64_t* res) {
    DWORD r;
    uint8_t buf[8];
    if (!ReadFile(file, buf, 8, &r, NULL) || r != 8) {
        return false;
    }
    *res = (((uint64_t)buf[0]) | (((uint64_t)(buf[1])) << 8) | 
        (((uint64_t)(buf[2])) << 16) | (((uint64_t)(buf[3])) << 24) |
        (((uint64_t)(buf[4])) << 32) | (((uint64_t)(buf[5])) << 40) |
        (((uint64_t)(buf[6])) << 48) | (((uint64_t)(buf[7])) << 56));
    return true;
} 

uint8_t* read_index_file(uint64_t* total_size) {
    HANDLE file =
        CreateFileW(L"index_file.bin", GENERIC_READ, 0, NULL,
                    OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return NULL;
    }
    if (!read_u64(file, total_size)) {
        CloseHandle(file);
        return false;
    }
    if (*total_size > 1024 * 1024 * 1024) {
        CloseHandle(file);
        return false;
    }
    uint8_t* buf = Mem_alloc(*total_size);

}


bool write_index_file(uint8_t *buf, uint64_t total_size) {
    HashMap *map = (HashMap *)buf;
    HANDLE file =
        CreateFileW(L"index_file.bin", GENERIC_READ | GENERIC_WRITE, 0, NULL,
                    OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }
    uint64_t struct_size = sizeof(HashMap) +
                           map->bucket_count * sizeof(HashBucket) +
                           map->element_count * sizeof(HashElement);

    uint64_t str_size = total_size - str_size;
    WRITE_U64(file, total_size);

    uint64_t bucket_offset = ((uint8_t*)map->buckets) - buf;
    WRITE_U64(file, bucket_offset);
    WRITE_U32(file, map->bucket_count);
    WRITE_U32(file, map->element_count);
    for (uint32_t b = 0; b < map->bucket_count; ++b) {
        HashBucket* bucket = &map->buckets[b];
        uint64_t elem_offset = ((uint8_t*)(bucket->data)) - buf;
        WRITE_U64(file, elem_offset);
        WRITE_U32(file, bucket->size);
        WRITE_U32(file, bucket->capacity);
    }
    HashElement *elems = (HashElement*)(buf + sizeof(HashMap) +
                                         map->bucket_count * sizeof(HashBucket));
    for (uint32_t ix = 0; ix < map->element_count; ++ix) {
        HashElement* elem = &elems[ix];
        uint64_t key_offset = ((uint8_t*)(elem->key)) - buf;
        uint64_t val_offset = ((uint8_t*)(elem->value)) - buf;
        WRITE_U64(file, key_offset);
        WRITE_U64(file, val_offset);
    }
    uint8_t *str_start = buf + struct_size;

    uint64_t written = 0;
    while (written < str_size) {
        uint64_t to_write = str_size - written;
        if (to_write > 1024 * 1024 * 16) {
            to_write = 1024 * 1024 * 16;
        }
        DWORD w;
        if (!WriteFile(file, str_start + written, to_write, &w, NULL) || w == 0) {
            CloseHandle(file);
            return false;
        }
        written += w;
    }

    CloseHandle(file);
    return true;
}

bool scrape_dll() {
    WString var;
    WString glob;
    if (!WString_create_capacity(&glob, 50)) {
        return false;
    }
    if (!create_envvar(L"PATH", &var)) {
        WString_free(&glob);
        return false;
    }
    PathIterator it;
    if (!PathIterator_begin(&it, &var)) {
        WString_free(&var);
        WString_free(&glob);
        return false;
    }
    wchar_t *path;
    uint32_t size;
    WHashMap values;
    HashMap map;
    if (!HashMap_Create(&map)) {
        WString_free(&var);
        WString_free(&glob);
        PathIterator_abort(&it);
        return false;
    }
    if (!WHashMap_Create(&values)) {
        WString_free(&var);
        WString_free(&glob);
        PathIterator_abort(&it);
        HashMap_Free(&map);
        return false;
    }

    while ((path = PathIterator_next(&it, &size)) != NULL) {
        WString_clear(&glob);
        if (!WString_reserve(&glob, size + 6)) {
            goto fail;
        }
        WString_append_count(&glob, path, size);
        if (path[size - 1] != L'\\') {
            WString_append(&glob, L'\\');
        }
        WString_append_count(&glob, L"*.dll", 5);
        GlobCtx ctx;
        Glob_begin(glob.buffer, &ctx);
        Path *p;
        while (Glob_next(&ctx, &p)) {
            uint32_t count;
            enum SymbolFileType type;
            WHashElement *el = NULL;
            char **syms = symbol_dump(p->path.buffer, &count, &type);
            if (syms == NULL) {
                continue;
            }
            if (count > 0) {
                el = WHashMap_Get(&values, p->path.buffer);
                if (el == NULL) {
                    Glob_abort(&ctx);
                    Mem_free(syms);
                    goto fail;
                }
            }
            for (uint32_t ix = 0; ix < count; ++ix) {
                HashElement *sym = HashMap_Get(&map, syms[ix]);
                if (sym == NULL) {
                    Glob_abort(&ctx);
                    Mem_free(syms);
                    goto fail;
                }

                el = WHashMap_Get(&values, p->path.buffer);
                if (sym->value != NULL) {
                    uint32_t old_len = wcslen(sym->value);
                    uint32_t new_len = wcslen(el->key);
                    uint32_t size = new_len + old_len + 1;
                    wchar_t *buf = Mem_alloc((size + 1) * sizeof(wchar_t));
                    if (buf == NULL) {
                        Glob_abort(&ctx);
                        Mem_free(syms);
                        goto fail;
                    }
                    memcpy(buf, sym->value, old_len * sizeof(wchar_t));
                    buf[old_len] = L'\n';
                    memcpy(buf + old_len + 1, el->key,
                           (new_len + 1) * sizeof(wchar_t));
                    WHashElement *el2 = WHashMap_Get(&values, buf);
                    Mem_free(buf);
                    if (el2 == NULL) {
                        Glob_abort(&ctx);
                        Mem_free(syms);
                        goto fail;
                    }
                    sym->value = (void *)el2->key;
                } else {
                    sym->value = (void *)el->key;
                }
            }

            Mem_free(syms);
        }
    }

    WString_free(&glob);
    WString_free(&var);

    WHashMap new_vals;
    if (!WHashMap_Create(&new_vals)) {
        WHashMap_Free(&values);
        HashMap_Free(&map);
        return false;
    }

    uint64_t filename_size = 0;
    uint64_t symbol_size = 0;
    uint64_t hashstruct_size = sizeof(HashMap) +
                               map.bucket_count * sizeof(HashBucket) +
                               map.element_count * sizeof(HashElement);

    // Keep one copy of each value
    for (uint32_t b = 0; b < map.bucket_count; ++b) {
        for (uint32_t e = 0; e < map.buckets[b].size; ++e) {
            const wchar_t *s = map.buckets[b].data[e].value;
            const char *sym = map.buckets[b].data[e].key;
            symbol_size += strlen(sym) + 1;
            WHashElement *el = WHashMap_Get(&new_vals, s);
            if (el == NULL) {
                WHashMap_Free(&new_vals);
                WHashMap_Free(&values);
                HashMap_Free(&map);
                return false;
            }
            if (el->value == NULL) {
                filename_size += (wcslen(s) + 1) * sizeof(wchar_t);
                el->value = (void *)(uintptr_t)(new_vals.element_count);
            }
            map.buckets[b].data[e].value = (void *)el->key;
        }
    }
    WHashMap_Free(&values);
    values = new_vals;

    uint64_t total_size = filename_size + symbol_size + hashstruct_size;
    uint8_t *ptr = Mem_alloc(total_size);
    if (ptr == NULL) {
        HashMap_Free(&map);
        WHashMap_Free(&values);
        return false;
    }
    HashBucket *buckets = (HashBucket *)(ptr + sizeof(HashMap));
    HashElement *elems = (HashElement *)(ptr + sizeof(HashMap) +
                                         map.bucket_count * sizeof(HashBucket));
    char *keys = (char *)(ptr + hashstruct_size);
    wchar_t *vals = (wchar_t *)(ptr + hashstruct_size + symbol_size);
    HashMap *frozen = (HashMap *)ptr;
    frozen->buckets = buckets;
    frozen->element_count = map.element_count;
    frozen->bucket_count = map.bucket_count;
    uint32_t el_offset = 0;
    uint64_t key_offset = 0;
    uint64_t val_offset = 0;

    for (uint32_t b = 0; b < values.bucket_count; ++b) {
        for (uint32_t e = 0; e < values.buckets[b].size; ++e) {
            const wchar_t *val = values.buckets[b].data[e].key;
            uint64_t val_len = wcslen(val) + 1;
            memcpy(vals + val_offset, val, val_len * sizeof(wchar_t));
            values.buckets[b].data[e].value = vals + val_offset;
            val_offset += val_len;
        }
    }

    for (uint32_t ix = 0; ix < map.bucket_count; ++ix) {
        buckets[ix].size = map.buckets[ix].size;
        buckets[ix].capacity = 0;
        buckets[ix].data = elems + el_offset;
        for (uint32_t e = 0; e < map.buckets[ix].size; ++e) {
            const wchar_t *val = map.buckets[ix].data[e].value;
            wchar_t *new_val = WHashMap_Value(&values, val);
            HashElement el = {keys + key_offset, new_val};
            memcpy(&buckets[ix].data[e], &el, sizeof(HashElement));
            uint64_t key_len = strlen(map.buckets[ix].data[e].key);
            memcpy(keys + key_offset, map.buckets[ix].data[e].key, key_len + 1);
            key_offset += key_len + 1;
        }
        el_offset += buckets[ix].size;
    }

    HashMap_Free(&map);
    WHashMap_Free(&values);

    Mem_free(ptr);
    _wprintf(L"Good\n");
    return true;
fail:
    WString_free(&var);
    WString_free(&glob);
    PathIterator_abort(&it);
    HashMap_Free(&map);
    WHashMap_Free(&values);
    return false;
}

int main() {
    LPWSTR args = GetCommandLineW();
    int argc;
    wchar_t **argv = parse_command_line(args, &argc);

    bool dll = false, lib = false, obj = false;
    for (uint32_t ix = 1; ix < argc; ++ix) {
        if (_wcsicmp(argv[ix], L"dll") == 0) {
            dll = true;
        } else if (_wcsicmp(argv[ix], L"lib") == 0) {
            lib = true;
        } else if (_wcsicmp(argv[ix], L"obj") == 0) {
            obj = true;
        } else {
            _wprintf_e(L"Invalid argument %s\n", argv[ix]);
        }
    }

    if (dll) {
        scrape_dll();
    }

    Mem_free(argv);
    return 0;
}
