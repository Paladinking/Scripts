#include "args.h"
#include "coff.h"
#include "glob.h"
#include "hashmap.h"
#include "mem.h"
#include "path_utils.h"
#include "printf.h"
#include "stdbool.h"
#include "whashmap.h"

#ifdef EMBEDDED_SYMBOLS
#include "../tools/index.h"
#endif

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
            goto fail;                                               \
        }                                                            \
    } while (0)

#define WRITE_U32(file, u)                                           \
    do {                                                             \
        DWORD w;                                                     \
        uint8_t b[4] = {(u) & 0xFF,         ((u) >> 8) & 0xFF,       \
                        ((u) >> 16) & 0xFF, ((u) >> 24) & 0xFF};     \
        if (!WriteFile(file, b, 4, &w, NULL) || w != 4) {            \
            goto fail;                                               \
        }                                                            \
    } while (0)

#define READ_U64(ptr)                                                  \
    ((((uint64_t)((ptr)[0])))       | (((uint64_t)((ptr)[1])) << 8 ) | \
     (((uint64_t)((ptr)[2])) << 16) | (((uint64_t)((ptr)[3])) << 24) | \
     (((uint64_t)((ptr)[4])) << 32) | (((uint64_t)((ptr)[5])) << 40) | \
     (((uint64_t)((ptr)[6])) << 48) | (((uint64_t)((ptr)[7])) << 56))

#define READ_U32(ptr)                                                  \
    ((((uint32_t)((ptr)[0])))       | (((uint32_t)((ptr)[1])) << 8 ) | \
     (((uint32_t)((ptr)[2])) << 16) | (((uint32_t)((ptr)[3])) << 24))

#define READ_U16(ptr)                                                  \
    ((uint16_t)((((uint16_t)((ptr)[0]))) | (((uint16_t)((ptr)[1])) << 8 )))

static inline bool read_u64(HANDLE file, uint64_t* res) {
    DWORD r;
    uint8_t buf[8];
    if (!ReadFile(file, buf, 8, &r, NULL) || r != 8) {
        return false;
    }
    *res = READ_U64(buf);
    return true;
}

bool index_file_to_map(uint8_t* buf, uint64_t size) {
    if (size < sizeof(HashMap)) {
        return false;
    }

    // Make sure final string is Null-terminated.
    wchar_t last_char;
    memcpy(&last_char, buf + size - sizeof(wchar_t), sizeof(wchar_t));
    if (last_char != L'\0') {
        return false;
    }

    uint64_t bucket_offset = READ_U64(buf);
    uint32_t bucket_count = READ_U32(buf + sizeof(uint64_t));
    uint32_t elem_count = READ_U32(buf + sizeof(uint64_t) + sizeof(uint32_t));

    if (bucket_offset != sizeof(HashMap) || bucket_offset + bucket_count * sizeof(HashBucket) > size) {
        return false;
    }
    uint64_t elem_offset = bucket_offset + bucket_count * sizeof(HashBucket);
    uint64_t elem_end = elem_offset + elem_count * sizeof(HashElement);
    if (elem_offset > 0x40000000 || elem_end > size) {
        return false;
    }
    for (uint32_t b = 0; b < bucket_count; ++b) {
        uint64_t bucket_data = READ_U64(buf + bucket_offset + b * sizeof(HashBucket));
        uint32_t bucket_size = READ_U32(buf + bucket_offset + b * sizeof(HashBucket) + sizeof(uint64_t));
        if (bucket_data > 0x40000000 || bucket_data < elem_offset ||
            bucket_data + bucket_size * sizeof(HashElement) > elem_end) {
            return false;

        }
        HashBucket* bucket = (HashBucket*)(buf + bucket_offset + b * sizeof(HashBucket));
        bucket->data = (HashElement*)(buf + bucket_data);
        bucket->capacity = 0;
    }

    uint64_t str_offset = sizeof(HashMap) +
                           bucket_count * sizeof(HashBucket) +
                           elem_count * sizeof(HashElement);
    for (uint32_t e = 0; e < elem_count; ++e) {
        uint64_t off = elem_offset + e * sizeof(HashElement);
        uint64_t key_offset = READ_U64(buf + off);
        uint64_t val_offset = READ_U64(buf + off + sizeof(uint64_t));
        if (key_offset < str_offset || key_offset >= size ||
            val_offset < str_offset || val_offset >= size - 1) {
            return false;
        }
        HashElement elem = {(char*)(buf + key_offset), (wchar_t*)(buf + val_offset)};
        memcpy(buf + off, &elem, sizeof(HashElement));
    }

    HashMap* map = (HashMap*)buf;
    map->buckets = (HashBucket*)(buf + bucket_offset);
    return true;
}

#ifndef EMBEDDED_SYMBOLS
HashMap* read_index_file(uint64_t* total_size, const wchar_t* filename) {
    HANDLE file =
        CreateFileW(filename, GENERIC_READ, 0, NULL,
                    OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return NULL;
    }
    if (!read_u64(file, total_size)) {
        CloseHandle(file);
        return NULL;
    }
    if (*total_size > 0x40000000) {
        CloseHandle(file);
        return NULL;
    }
    uint8_t* buf = Mem_alloc(*total_size);
    uint64_t read = 0;
    while (read < *total_size) {
        DWORD r;
        if (!ReadFile(file, buf + read, *total_size - read, &r, NULL) || r == 0) {
            Mem_free(buf);
            CloseHandle(file);
            return NULL;
        }
        read += r;
    }
    CloseHandle(file);
    uint64_t size = *total_size;
    if (!index_file_to_map(buf, size)) {
        Mem_free(buf);
        return NULL;
    }

    return (HashMap*)buf;
}


bool write_index_file(uint8_t *buf, uint64_t total_size, const wchar_t* filename) {
    HashMap *map = (HashMap *)buf;
    HANDLE file =
        CreateFileW(filename, GENERIC_READ | GENERIC_WRITE | DELETE, 0, NULL,
                    OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }
    uint64_t struct_size = sizeof(HashMap) +
                           map->bucket_count * sizeof(HashBucket) +
                           map->element_count * sizeof(HashElement);

    uint64_t str_size = total_size - struct_size;
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
            goto fail;
        }
        written += w;
    }

    CloseHandle(file);
    return true;
fail:
    FILE_DISPOSITION_INFO info;
    info.DeleteFileA = TRUE;
    SetFileInformationByHandle(file, FileDispositionInfo, &info, sizeof(info));

    CloseHandle(file);
    return false;
}

HashMap* scrape_symbols(uint64_t *total_size_dest, const wchar_t* env_var,
                        const wchar_t* file_pattern, bool verbose) {
    WString var;
    WString glob;
    if (!WString_create_capacity(&glob, 50)) {
        return NULL;
    }
    if (!create_envvar(env_var, &var)) {
        WString_free(&glob);
        return NULL;
    }
    PathIterator it;
    if (!PathIterator_begin(&it, &var)) {
        WString_free(&var);
        WString_free(&glob);
        return NULL;
    }
    wchar_t *path;
    uint32_t size;
    WHashMap values;
    HashMap map;
    if (!HashMap_Create(&map)) {
        WString_free(&var);
        WString_free(&glob);
        PathIterator_abort(&it);
        return NULL;
    }
    if (!WHashMap_Create(&values)) {
        WString_free(&var);
        WString_free(&glob);
        PathIterator_abort(&it);
        HashMap_Free(&map);
        return NULL;
    }

    while ((path = PathIterator_next(&it, &size)) != NULL) {
        // Skip already visited directories
        // e.g in case of duplicate entries in PATH.
        WHashElement* path_el = WHashMap_Get(&values, path);
        if (path_el == NULL) {
            goto fail;
        }
        if (path_el->value != NULL) {
            continue;
        }
        path_el->value = &values;
        WString_clear(&glob);
        if (!WString_reserve(&glob, size + 6)) {
            goto fail;
        }
        WString_append_count(&glob, path, size);
        if (path[size - 1] != L'\\') {
            WString_append(&glob, L'\\');
        }
        WString_extend(&glob, file_pattern);
        GlobCtx ctx;
        Glob_begin(glob.buffer, &ctx);
        Path *p;
        while (Glob_next(&ctx, &p)) {
            uint32_t count;
            enum SymbolFileType type;
            WHashElement *el = NULL;
            if (verbose) {
                _wprintf(L"%s\n", p->path.buffer);
            }
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
        return NULL;
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
                return NULL;
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
        return NULL;
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

    *total_size_dest = total_size;
    return frozen;
fail:
    WString_free(&var);
    WString_free(&glob);
    PathIterator_abort(&it);
    HashMap_Free(&map);
    WHashMap_Free(&values);
    return NULL;
}

wchar_t* index_filename(const wchar_t* var, const wchar_t* pattern) {
    wchar_t filename[512] = L"index\\index_";
    size_t pattern_len = wcslen(pattern);

    if (pattern_len - 2 > 490) {
        return NULL;
    }
    for (size_t ix = 0; ix < pattern_len - 2; ++ix) {
        filename[12 + ix] = towlower(pattern[ix + 2]);
    }
    memcpy(filename + 10 + pattern_len, L".bin", 4 * sizeof(wchar_t));

    wchar_t* buf = Mem_alloc(1024 * sizeof(wchar_t));
    if (buf == NULL) {
        return NULL;
    }
    if (!find_file_relative(buf, 1024, filename, false)) {
        Mem_free(buf);
        return NULL;
    }
    return buf;
}
#endif


HashMap* find_symbols(const wchar_t* var, const wchar_t* pattern,
                      bool force_index, bool verbose) {
#ifdef EMBEDDED_SYMBOLS
    static bool dll_ready = false, lib_ready = false, obj_ready = false, o_ready = false;
    uint8_t* buf;
    uint64_t total_size;
    bool* fixup;
    if (pattern[2] == L'd') {
        buf = index_dll_bin + sizeof(uint64_t);
        total_size = sizeof(index_dll_bin) - sizeof(uint64_t);
        fixup = &dll_ready;
    } else if (pattern[2] == L'l') {
        buf = index_lib_bin + sizeof(uint64_t);
        total_size = sizeof(index_lib_bin) - sizeof(uint64_t);
        fixup = &lib_ready;
    } else if (pattern[2] == L'o' && pattern[3] == L'b') {
        buf = index_obj_bin + sizeof(uint64_t);
        total_size = sizeof(index_obj_bin) - sizeof(uint64_t);
        fixup = &obj_ready;
    } else {
        buf = index_o_bin + sizeof(uint64_t);
        total_size = sizeof(index_o_bin) - sizeof(uint64_t);
        fixup = &o_ready;
    }

    if (!(*fixup)) {
        if (!index_file_to_map(buf, total_size)) {
            return NULL;
        }
        *fixup = true;
    }
    return (HashMap*)buf;
#else
    wchar_t* index_file = index_filename(var, pattern);
    uint64_t total_size;
    HashMap* res = NULL;
    if (index_file != NULL && !force_index) {
        res = read_index_file(&total_size, index_file);
    }
    if (res != NULL) {
        if (verbose) {
            _wprintf(L"Read index file %s\n", index_file);
        }
        Mem_free(index_file);
        return res;
    }

    if (verbose) {
        _wprintf(L"Searching '%s' variable for '%s' files\n", var, pattern);
    }
    res = scrape_symbols(&total_size, var, pattern, verbose);
    bool created_index = false;
    if (res != NULL && index_file != NULL) {
        wchar_t buf[1024];
        if (find_file_relative(buf, 1024, L"index\\", false)) {
            DWORD attr = GetFileAttributesW(buf);
            if ((attr == INVALID_FILE_ATTRIBUTES && CreateDirectoryW(buf, NULL)) ||
                 attr & FILE_ATTRIBUTE_DIRECTORY) {
                created_index = write_index_file((uint8_t*)res, total_size, index_file);
            }
        }
    }
    if (verbose && created_index) {
        _wprintf(L"Created index file %s\n", index_file);
    }
    Mem_free(index_file);
    return res;
#endif
}

const wchar_t *HELP_MESSAGE =
    L"Usage: %s [OPTION]... [FILE]...\n"
    L"Find what objects / libraries export a specific symbol\n\n"
    L"-a, --all               implies --dll --lib --obj\n"
#ifndef EMBEDDED_SYMBOLS
    L"-d, --dll               search .dll files in PATH environment variable\n"
    L"-f, --force-index       force indexing even if previous index exists\n"
#else
    L"-d, --dll               search .dll files\n"
#endif
    L"-h, --help              display this message and exit\n"
#ifndef EMBEDDED_SYMBOLS
    L"-i, --index-only        only create index, implies --force-index\n"
    L"-l, --lib               search .lib files in LIB environment variable (default)\n"
    L"-o, --obj               search .obj files in LIB environment variable\n"
#else
    L"-l, --lib               search .lib files (default)\n"
    L"-o, --obj               search .obj files\n"
#endif
    L"-v, --verbose           show verbose output\n";


int main() {
    LPWSTR args = GetCommandLineW();
    int argc;
    wchar_t **argv = parse_command_line(args, &argc);
    bool dll = false, lib = false, obj = false, force = false, verbose = false, index = false;

    FlagInfo flags[] = {
        {L'd', L"dll", NULL},
        {L'l', L"lib", NULL},
        {L'o', L"obj", NULL},
        {L'a', L"all", NULL},
        {L'h', L"help", NULL},
        {L'v', L"verbose", NULL}
#ifndef EMBEDDED_SYMBOLS
        ,{L'f', L"force-index", NULL},
        {L'i', L"index-only", NULL}
#endif
    };
    const uint32_t flag_count = sizeof(flags) / sizeof(FlagInfo);

    ErrorInfo err;
    if (!find_flags(argv, &argc, flags, flag_count, &err)) {
        wchar_t *err_msg = format_error(&err, flags, flag_count);
        _wprintf_e(L"%s\n", err_msg);
        if (argc > 0) {
            _wprintf_e(L"Run '%s --help' for more information\n", argv[0]);
        }
        return 1;
    }

    if (flags[4].count > 0) {
        _wprintf(HELP_MESSAGE, argv[0]);
        return 0;
    }

    dll = flags[0].count > 0 || flags[3].count > 0;
    lib = flags[1].count > 0 || flags[3].count > 0;
    obj = flags[2].count > 0 || flags[3].count > 0;

    bool multiple = dll + obj + lib > 1;
    if (!dll && !lib && !obj) {
        lib = true;
    }

    verbose = flags[5].count > 0;
#ifndef EMBEDDED_SYMBOLS
    index = flags[7].count > 0;
    if (index) {
        int status = 0;
        if (dll) {
            HashMap* m = find_symbols(L"PATH", L"*.dll", true, verbose);
            if (m != NULL) {
                Mem_free(m);
            } else {
                status = 1;
                _wprintf_e(L"Failed collecting dll symbols\n");
            }
        }
        if (lib) {
            HashMap* m = find_symbols(L"LIB", L"*.lib", true, verbose);
            if (m != NULL) {
                Mem_free(m);
            } else {
                status = 1;
                _wprintf_e(L"Failed collecting lib symbols\n");
            }
        }
        if (obj) {
            HashMap* obj1 = find_symbols(L"LIB", L"*.obj", true, verbose);
            HashMap* obj2 = find_symbols(L"LIB", L"*.o", true, verbose);
            if (obj1 != NULL) {
                Mem_free(obj1);
            } else {
                status = 1;
                _wprintf_e(L"Failed collecting object symbols\n");
            }
            if (obj2 != NULL) {
                Mem_free(obj2);
            } else if (obj1 != NULL) {
                status = 1;
                _wprintf_e(L"Failed collecting object symbols\n");
            }
        }
        Mem_free(argv);
        return status;
    }
#endif

    if (argc < 2) {
        _wprintf_e(L"Missing argument\n");
        if (argc > 0) {
            _wprintf_e(L"Run '%s --help' for more information\n", argv[0]);
        }
        return 1;
    }
#ifndef EMBEDDED_SYMBOLS
    force = flags[6].count > 0;
#endif

    String s;
    if (!String_create(&s)) {
        return 1;
    }
    String_from_utf16_str(&s, argv[1]);

    if (dll) {
        HashMap* map = find_symbols(L"PATH", L"*.dll", force, verbose);
        if (map == NULL) {
            _wprintf_e(L"Failed collecting dll symbols\n");
        } else {
            wchar_t* match = HashMap_Value(map, s.buffer);
                _wprintf(L"Dll matches for '%s':\n", argv[1]);
            if (match != NULL) {
                _wprintf(L"%s\n", match);
            }
#ifndef EMBEDDED_SYMBOLS
            Mem_free(map);
#endif
        }
    }
    if (lib) {
        HashMap* map = find_symbols(L"LIB", L"*.lib", force, verbose);
        if (map == NULL) {
            _wprintf_e(L"Failed collecting lib symbols\n");
        } else {
            wchar_t* match = HashMap_Value(map, s.buffer);
            _wprintf(L"Lib matches for '%s':\n", argv[1]);
            if (match != NULL) {
                _wprintf(L"%s\n", match);
            }
#ifndef EMBEDDED_SYMBOLS
            Mem_free(map);
#endif
        }
    }
    if (obj) {
        HashMap* m1 = find_symbols(L"LIB", L"*.obj", force, verbose);
        HashMap* m2 = find_symbols(L"LIB", L"*.o", force, verbose);
        if (m1 == NULL || m2 == NULL) {
            _wprintf_e(L"Failed collecting object symbols\n");
            return 1;
        } else {
            _wprintf(L"Object matches for '%s':\n", argv[1]);
            wchar_t* match1 = HashMap_Value(m1, s.buffer);
            if (match1 != NULL) {
                _wprintf(L"%s\n", match1);
            }
            wchar_t* match2 = HashMap_Value(m2, s.buffer);
            if (match2 != NULL) {
                _wprintf(L"%s\n", match2);
            }
        }
#ifndef EMBEDDED_SYMBOLS
        if (m1 != NULL) {
            Mem_free(m1);
        }
        if (m2 != NULL) {
            Mem_free(m2);
        }
#endif
    }

    String_free(&s);

    Mem_free(argv);
    return 0;
}
