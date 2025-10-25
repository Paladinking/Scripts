#include "linker.h"
#include <mem.h>
#include <dynamic_string.h>
#include <glob.h>
#include <printf.h>

const static enum LogCatagory LOG_CATAGORY = LOG_CATAGORY_LINKER;

#include "pe_coff.h"

Symbol* find_symbol(Object* obj, const uint8_t* sym, uint32_t sym_len);

void merge_objects(Object* a, Object* b);

void Libary_create(Library* lib) {
    lib->dest = Mem_alloc(sizeof(Object));
    if (lib->dest == NULL) {
        out_of_memory(NULL);
    }
    Object_create(lib->dest);
    ObjectSet_create(&lib->objects);
}

bool Library_find_symbol(Library* lib, const uint8_t* sym, uint32_t len,
                         SymbolQueue* queue) {
    Symbol* s = find_symbol(lib->dest, sym, len);
    if (s != NULL && s->section != SECTION_IX_NONE) {
        return true;
    }

    uint32_t old_count = lib->dest->symbol_count;

    for (uint32_t i = 0; i < lib->objects.object_count; ++i) {
        Object* o = lib->objects.objects[i];
        Symbol* s = find_symbol(o, sym, len);
        if (s != NULL && s->section != SECTION_IX_NONE) {
            merge_objects(lib->dest, o);
            ObjectSet_remove(&lib->objects, i);
            Mem_free(o);

            for (uint32_t i = old_count; i < lib->dest->symbol_count; ++i) {
                if (!lib->dest->symbols[i].external) {
                    // Section relative symbols can be SECTION_IX_NONE
                    // due to weird import library stuff...
                    // Normaly only external symbols can have SECTION_IX_NONE
                    continue;
                }
                if (lib->dest->symbols[i].section == SECTION_IX_NONE) {
                    SymbolQueue_append(queue, lib->dest->symbols[i].name,
                                       lib->dest->symbols[i].name_len);
                }
            }

            return true;
        }
    }

    return false;
}

void Library_free(Library* lib) {
    if (lib->dest != NULL) {
        Object_free(lib->dest);
        Mem_free(lib->dest);
    }
    ObjectSet_free(&lib->objects);
}

void SymbolQueue_create(SymbolQueue* queue) {
    queue->head = 0;
    queue->tail = 0;
    queue->capacity = 64;
    queue->symbols = Mem_alloc(64 * sizeof(StrWithLength));
    if (queue->symbols == NULL) {
        out_of_memory(NULL);
    }
}

void SymbolQueue_free(SymbolQueue* queue) {
    Mem_free(queue->symbols);
    queue->symbols = NULL;
    queue->capacity = 0;
    queue->head = 0;
    queue->tail = 0;
}

void SymbolQueue_append(SymbolQueue* queue, const uint8_t* sym, uint32_t len) {
    uint64_t size = SymbolQueue_size(queue);
    if (size == queue->capacity) {
        StrWithLength* n = Mem_alloc(queue->capacity * 2);
        if (n == NULL) {
            out_of_memory(NULL);
        }
        uint64_t t_ix = queue->tail % queue->capacity;
        memcpy(n, queue->symbols + t_ix, (size - t_ix) * sizeof(StrWithLength));
        memcpy(n + (size - t_ix), queue->symbols, t_ix * sizeof(StrWithLength));
        queue->tail = 0;
        queue->head = queue->capacity;
        queue->capacity *= 2;
        Mem_free(queue->symbols);
        queue->symbols = n;
    }
    uint64_t h_ix = queue->head % queue->capacity;
    queue->symbols[h_ix].str = sym;
    queue->symbols[h_ix].len = len;
    queue->head += 1;
}

uint64_t SymbolQueue_size(SymbolQueue* queue) {
    return queue->head - queue->tail;
}

StrWithLength SymbolQueue_get(SymbolQueue* queue) {
    assert(queue->head > queue->tail);
    uint64_t t_ix = queue->tail % queue->capacity;
    StrWithLength res = queue->symbols[t_ix];
    queue->tail += 1;
    return res;
}

void ObjectSet_create(ObjectSet* set) {
    set->objects = Mem_alloc(16 * sizeof(Object*));
    if (set->objects == NULL) {
        out_of_memory(NULL);
    }
    set->object_count = 0;
    set->object_cap = 16;
}

void ObjectSet_free(ObjectSet* set) {
    // TODO: free objects
    Mem_free(set->objects);
    set->object_count = 0;
    set->object_cap = 0;
    set->objects = NULL;
}

void ObjectSet_add(ObjectSet* set, Object* obj) {
    uint64_t cap = set->object_cap;
    reserve((void**)&set->objects, set->object_count + 1, 
            sizeof(Object*), &cap);
    set->object_cap = cap;

    set->objects[set->object_count++] = obj;
}

void ObjectSet_remove(ObjectSet* set, uint32_t ix) {
    memmove(set->objects + ix, set->objects + ix + 1,
            (set->object_count - ix - 1) * sizeof(Object*));
    set->object_count -= 1;
    return;
}

void Object_create(Object* object) {
    object->symbols = Mem_alloc(16 * sizeof(Symbol));
    if (object->symbols == NULL) {
        out_of_memory(NULL);
    }
    object->symbol_cap = 16;
    object->symbol_count = 0;

    object->sections = Mem_alloc(4 * sizeof(Section));
    if (object->sections == NULL) {
        out_of_memory(NULL);
    }
    object->section_cap = 4;
    object->section_count = 0;
    object->map = NULL;
    object->map_size = 0;
}

void Object_free(Object* object) {
    for (uint32_t i = 0; i < object->section_count; ++i) {
        if (object->sections[i].type != SECTION_BSS) {
            Mem_free(object->sections[i].relocations);
        }
        Mem_free((uint8_t*)object->sections[i].name);
    }
    Mem_free(object->sections);
    if (object->map != NULL) {
        Mem_free(object->map);
    }

    for (uint32_t i = 0; i < object->symbol_count; ++i) {
        Mem_free((uint8_t*)object->symbols[i].name);
    }
    Mem_free(object->symbols);
}

static uint32_t hash(const uint8_t* str, uint32_t len, uint32_t size) {
    uint64_t hash = 5381;
    int c;
    for (uint32_t ix = 0; ix < len; ++ix) {
        c = str[ix];
        hash = ((hash << 5) + hash) + c;
    }
    return hash % size;
}

symbol_ix* Object_build_map(Object* object) {
    if (object->map != NULL) {
        return object->map;
    }
    if (object->symbol_count == 0) {
        symbol_ix* map = Mem_alloc(1 * sizeof(symbol_ix));
        if (map == NULL) {
            out_of_memory(NULL);
        }
        object->map_size = 1;
        object->map = map;
        map[0] = SYMBOL_IX_NONE;
        return map;
    }

    symbol_ix* map = Mem_alloc(object->symbol_count * 2 * sizeof(symbol_ix));
    if (map == NULL) {
        out_of_memory(NULL);
    }
    for (symbol_ix i = 0; i < 2 * object->symbol_count; ++i) {
        map[i] = SYMBOL_IX_NONE;
    }

    for (symbol_ix i = 0; i < object->symbol_count; ++i) {
        if (object->symbols[i].external) {
            uint32_t h = hash(object->symbols[i].name,
                              object->symbols[i].name_len,
                              object->symbol_count * 2);
            while (map[h] != SYMBOL_IX_NONE) {
                h = (h + 1) % (object->symbol_count * 2);
            }
            map[h] = i;
        }
    }
    object->map = map;
    object->map_size = object->symbol_count * 2;
    return map;
}

section_ix Object_create_named_section(Object* object, enum SectionType type,
                                       uint8_t* name) {
    if (object->section_count == object->section_cap) {
        object->sections = Mem_realloc(object->sections, 2 * sizeof(Section) * 
                object->section_count);
        if (object->sections == NULL) {
            out_of_memory(NULL);
        }
        object->section_cap *= 2;
    }
    Section* s = object->sections + object->section_count;
    object->section_count += 1;
    s->type = type;
    s->name = name;
    if (s->type == SECTION_BSS) {
        s->data.data = NULL;
        s->data.size = 0;
        s->data.capacity = 0;
        s->relocations = NULL;
        s->relocation_cap = 0;
    } else {
        Buffer_create(&s->data);
        s->relocations = Mem_alloc(16 * sizeof(Relocation));
        if (s->relocations == NULL) {
            out_of_memory(NULL);
        }
        s->relocation_cap = 16;
    }
    s->relocation_count = 0;
    if (type == SECTION_CODE) {
        s->align = 16;
    } else {
        s->align = 1;
    }
    return object->section_count - 1;
}

section_ix Object_create_section(Object* object, enum SectionType type) {
    uint8_t* name = Mem_alloc(8);
    if (name == NULL) {
        out_of_memory(NULL);
    }
    if (type == SECTION_BSS) {
        memcpy(name, ".bss", 5);
    } else if (type == SECTION_CODE) {
        memcpy(name, ".text", 6);
    } else if (type == SECTION_RDATA) {
        memcpy(name, ".rdata", 7);
    } else if (type == SECTION_DATA) {
        memcpy(name, ".data", 6);
    } else if (type == SECTION_IDATA) {
        memcpy(name, ".idata", 7);
    } else {
        assert(false);
    }
    return Object_create_named_section(object, type, name);
}

void Object_add_relocation(Object* object, section_ix section, Relocation reloc) {
    Section* s = object->sections + section;
    assert(s->type != SECTION_BSS);
    if (s->relocation_cap == s->relocation_count) {
        Relocation* r = Mem_realloc(s->relocations, 
                2 * sizeof(Relocation) * s->relocation_cap);
        if (r == NULL) {
            out_of_memory(NULL);
        }
        s->relocations = r;
        s->relocation_cap *= 2;
    }
    s->relocations[s->relocation_count] = reloc;
    s->relocation_count += 1;
}

void Object_add_reloc(Object* object, section_ix section, symbol_ix symbol,
                      enum RelocationType type) {
    assert(section != SECTION_IX_NONE);

    Section* s = object->sections + section;
    Relocation r;
    r.type = type;
    r.symbol = symbol;
    r.offset = s->data.size;
    Object_add_relocation(object, section, r);
}

symbol_ix Object_add_symbol(Object* object, section_ix section, const uint8_t* name,
                            uint32_t name_len, bool external, enum SymbolType type,
                            uint32_t offset) {
    if (object->symbol_count == object->symbol_cap) {
        object->symbols = Mem_realloc(object->symbols, object->symbol_cap * 
                                2 * sizeof(Symbol));
        if (object->symbols == NULL) {
            out_of_memory(NULL);
        }
        object->symbol_cap *= 2;
    }

    Symbol* s = object->symbols + object->symbol_count;
    object->symbol_count += 1;

    s->name = name;
    s->name_len = name_len;
    s->section = section;
    s->offset = offset;
    s->type = type;
    s->external = external;
    s->section_relative = false;

    return object->symbol_count - 1;
}

symbol_ix Object_reserve_var(Object *object, section_ix section, 
                             const uint8_t *name, uint32_t name_len, 
                             uint32_t size, uint32_t align, bool external) {
    uint8_t* name_ptr = Mem_alloc(name_len);
    if (name_ptr == NULL) {
        out_of_memory(NULL);
    }
    memcpy(name_ptr, name, name_len);

    assert(section != SECTION_IX_NONE);
    Section* s = object->sections + section;
    assert(s->type == SECTION_BSS);

    uint32_t offset = ALIGNED_TO(s->data.size, align);
    s->data.size += size;
    if (align > s->align) {
        s->align = align;
    }

    return Object_add_symbol(object, section, name_ptr, name_len, external,
                             SYMBOL_STANDARD, offset);
}

symbol_ix Object_declare_var(Object* object, section_ix section,
                             const uint8_t* name, uint32_t name_len,
                             uint32_t align, bool external) {
    uint8_t* name_ptr = Mem_alloc(name_len);
    if (name_ptr == NULL && name_len > 0) {
        out_of_memory(NULL);
    }
    memcpy(name_ptr, name, name_len);

    uint32_t offset;
    if (section != SECTION_IX_NONE) {
        Section* s = object->sections + section;
        assert(s->type != SECTION_BSS);
        offset = ALIGNED_TO(s->data.size, align);
        while (s->data.size < offset) {
            Buffer_append(&s->data, 0);
        }
        if (align > s->align) {
            s->align = align;
        }
    } else {
        offset = 0;
    }

    return Object_add_symbol(object, section, name_ptr, name_len,
                             external, SYMBOL_STANDARD, offset);
}

symbol_ix Object_declare_import(Object* object, section_ix section,
                                const uint8_t* name, uint32_t name_len) {
    uint32_t offset;
    if (section != SECTION_IX_NONE) {
        offset = object->sections[section].data.size;
    } else {
        offset = 0;
    }

    uint8_t* name_ptr = Mem_alloc(name_len + 6);
    if (name_ptr == NULL) {
        out_of_memory(NULL);
    }
    memcpy(name_ptr, "__imp_", 6);
    memcpy(name_ptr + 6, name, name_len);

    return Object_add_symbol(object, section, name_ptr, name_len + 6, true,
                             SYMBOL_STANDARD, offset);
}

symbol_ix Object_declare_fn(Object* object, section_ix section,
                            const uint8_t* name, uint32_t name_len,
                            bool external) {
    uint32_t offset;
    if (section != SECTION_IX_NONE) {
        Section* s = object->sections + section;
        assert(s->type == SECTION_CODE);
        offset = s->data.size;
    } else {
        assert(external);
        offset = 0;
    }

    uint8_t* name_ptr = Mem_alloc(name_len);
    if (name_ptr == NULL) {
        out_of_memory(NULL);
    }
    memcpy(name_ptr, name, name_len);

    return Object_add_symbol(object, section, name_ptr, name_len,
                             external, SYMBOL_FUNCTION, offset);
}


void Object_append_byte(Object* object, section_ix section, uint8_t byte) {
    assert(section != SECTION_IX_NONE);
    Section* s = object->sections + section;
    assert(s->type != SECTION_BSS);

    Buffer_append(&s->data, byte);
}

void Object_append_data(Object* object, section_ix section,
                        const uint8_t* data, uint32_t size) {
    assert(section != SECTION_IX_NONE);
    Section* s = object->sections + section;
    assert(s->type != SECTION_BSS);

    Buffer_extend(&s->data, data, size);
}

// Combines two objects into one containing sections and symbols from both
// Does not yet merge sections into one
// b is destroyed (do not call Object_free for it)
void merge_objects(Object* a, Object* b) {
    symbol_ix* map = Object_build_map(a);
    uint32_t map_count = a->map_size;

    for (symbol_ix i = 0; i < b->symbol_count; ++i) {
        const uint8_t* name = b->symbols[i].name;
        uint32_t name_len = b->symbols[i].name_len;
        section_ix section = SECTION_IX_NONE;
        if (b->symbols[i].section != SECTION_IX_NONE) {
            section = b->symbols[i].section + a->section_count;
        }

        if (!b->symbols[i].external) {
            //assert(section != SECTION_IX_NONE);
            symbol_ix sym = Object_add_symbol(a, section, name,
                name_len, false, b->symbols[i].type, b->symbols[i].offset);
            // Remap section
            a->symbols[sym].section_relative = b->symbols[i].section_relative;
            b->symbols[i].section = sym;
            continue;
        }

        uint32_t h = hash(name, name_len, map_count);
        while (1) {
            symbol_ix sym = map[h];
            if (sym == SYMBOL_IX_NONE) {
                symbol_ix s = Object_add_symbol(a, section, name, name_len, 
                        true, b->symbols[i].type, b->symbols[i].offset);
                b->symbols[i].section = s;
                break;
            }
            if (a->symbols[sym].name_len == name_len &&
                memcmp(a->symbols[sym].name, name, name_len) == 0) {
                // External symbol present in both a and b
                if (section != SYMBOL_IX_NONE) {
                    if (a->symbols[sym].section != SYMBOL_IX_NONE) {
                        // TODO: this should not be fatal
                        fatal_error(NULL, LINKER_ERROR_DUPLICATE_SYMBOL,
                                    LINE_INFO_NONE);
                        break;
                    }
                    a->symbols[sym].section = section;
                    b->symbols[i].section = sym;
                }
                Mem_free((uint8_t*)name);
                b->symbols[i].section = sym;
                break;
            }
            h = (h + 1) % map_count;
        }
        continue;
    }
    if (a->section_cap < a->section_count + b->section_count) {
        uint32_t new_cap = a->section_cap;
        while (new_cap < a->section_count + b->section_count) {
            new_cap *= 2;
        }
        Section* secs = Mem_realloc(a->sections, new_cap * sizeof(Section));
        if (secs == NULL) {
            out_of_memory(NULL);
        }
        a->sections = secs;
        a->section_cap = new_cap;
    }

    RESERVE32(a->sections, a->section_count + b->section_count, a->section_cap);

    for (section_ix sec = 0; sec < b->section_count; ++sec) {
        Section* section = a->sections + a->section_count + sec;
        section->type = b->sections[sec].type;
        section->name = b->sections[sec].name;
        section->align = b->sections[sec].align;
        section->data = b->sections[sec].data;
        section->relocations = b->sections[sec].relocations;
        section->relocation_count = b->sections[sec].relocation_count;
        section->relocation_cap = b->sections[sec].relocation_cap;

        for (uint32_t ix = 0; ix < section->relocation_count; ++ix) {
            symbol_ix old_sym = section->relocations[ix].symbol;
            symbol_ix new_sym = b->symbols[old_sym].section;
            section->relocations[ix].symbol = new_sym;
        }

    }
    a->section_count += b->section_count;

    Mem_free(b->sections);
    Mem_free(b->symbols);
    if (b->map != NULL) {
        Mem_free(b->map);
        b->map = NULL;
    }
    Mem_free(map);
    a->map = NULL;
}

const char* section_name(Section* s, uint32_t* name_len) {
    const char* name = (const char*)s->name;
    *name_len = strlen(name);
    char* dollar = memchr(name, '$', *name_len);
    if (dollar != NULL) {
        *name_len = dollar - name;
    }
    return name;
}

void combine_sections(Object* obj) {
    Object dest;
    Object_create(&dest);
    for (section_ix i = 0; i < obj->section_count; ++i) {
        if (obj->sections[i].type == SECTION_INVALID) {
            continue;
        }
        uint32_t name_len;
        const char* name = section_name(obj->sections + i, &name_len);

        section_ix match = SECTION_IX_NONE;
        const char* match_name = NULL;

        uint8_t* name_ptr = Mem_alloc(name_len + 1);
        if (name_ptr == NULL) {
            out_of_memory(NULL);
        }
        memcpy(name_ptr, name, name_len);
        name_ptr[name_len] = '\0';

        section_ix sec = Object_create_named_section(&dest, obj->sections[i].type,
                name_ptr);
        while (1) {
            match = SECTION_IX_NONE;
            for (section_ix j = i; j < obj->section_count; ++j) {
                if (obj->sections[j].type == SECTION_INVALID) {
                    continue;
                }
                uint32_t l2;
                const char* n2 = section_name(obj->sections + j, &l2);
                if (name_len == l2 && memcmp(name, n2, l2) == 0) {
                    if (match == SECTION_IX_NONE || strcmp(n2, match_name) < 0) {
                        match = j;
                        match_name = n2;
                    }
                    continue;
                }
            }
            if (match == SECTION_IX_NONE) {
                break;
            }
            assert(dest.sections[sec].type == obj->sections[match].type);
            // Mark section as taken
            obj->sections[match].type = SECTION_INVALID;

            uint32_t offset;
            if (obj->sections[match].align > dest.sections[sec].align) {
                dest.sections[sec].align = obj->sections[match].align;
            }
            if (dest.sections[sec].type == SECTION_BSS) {
                while (dest.sections[sec].data.size % obj->sections[match].align != 0) {
                    ++dest.sections[sec].data.size;
                }
                offset = dest.sections[sec].data.size;
                dest.sections[sec].data.size += obj->sections[match].data.size;
            } else {
                uint8_t pad = dest.sections[sec].type == SECTION_CODE ? 0x90 : 0x0;
                while (dest.sections[sec].data.size % obj->sections[match].align != 0) {
                    Buffer_append(&dest.sections[sec].data, pad);
                }
                offset = dest.sections[sec].data.size;
                Buffer_extend(&dest.sections[sec].data, obj->sections[match].data.data,
                              obj->sections[match].data.size);
            }

            for (uint32_t i = 0; i < obj->sections[match].relocation_count; ++i) {
                Relocation r = obj->sections[match].relocations[i];
                r.offset += offset;
                Object_add_relocation(&dest, sec, r);
            }
            // relocation_count is reused to store mapping to new section
            obj->sections[match].relocation_count = sec;
            // align is reused to store mapping to new offset
            obj->sections[match].align = offset;
        }
    }
    for (section_ix i = 0; i < obj->section_count; ++i) {
        assert(obj->sections[i].type == SECTION_INVALID);
    }

    for (symbol_ix i = 0; i < obj->symbol_count; ++i) {
        section_ix sec = SECTION_IX_NONE;
        uint32_t offset = 0;
        if (obj->symbols[i].section != SECTION_IX_NONE) {
            sec = obj->sections[obj->symbols[i].section].relocation_count;
            offset = obj->sections[obj->symbols[i].section].align;
        }
        symbol_ix sym = Object_add_symbol(&dest, sec, obj->symbols[i].name, 
                                     obj->symbols[i].name_len, 
                                     obj->symbols[i].external,
                                     obj->symbols[i].type, 
                                     obj->symbols[i].offset + offset);
        assert(sym == i);
    }
    obj->symbol_count = 0;
    Object_free(obj);
    *obj = dest;
}

void Object_serialize(Object* obj, String* dest) {
    for (uint32_t i = 0; i < obj->section_count; ++i) {
        String_format_append(dest, "Section %u %s:\n", i, obj->sections[i].name);
        String_extend(dest, "  Type: ");
        if (obj->sections[i].type == SECTION_CODE) {
            String_extend(dest, "Code\n");
        } else if (obj->sections[i].type == SECTION_BSS) {
            String_extend(dest, "Uninitialized data\n");
        } else if (obj->sections[i].type == SECTION_DATA) {
            String_extend(dest, "Initialized data\n");
        } else if (obj->sections[i].type == SECTION_RDATA) {
            String_extend(dest, "Read-only data\n");
        } else if (obj->sections[i].type == SECTION_IDATA) {
            String_extend(dest, "Import table\n");
        } else {
            String_extend(dest, "Unkown\n");
        }
        String_format_append(dest, "  Align: %u\n", obj->sections[i].align);
        String_format_append(dest, "  Size: %u\n", obj->sections[i].data.size);
        String_format_append(dest, "  Relocations: %u\n", obj->sections[i].relocation_count);
        for (uint32_t ix = 0; ix < obj->sections[i].relocation_count; ++ix) {
            Relocation* r = obj->sections[i].relocations + ix;
            if (r->type == RELOCATE_ABS) {
                String_format_append(dest, "    SYMBOL ABS    %.3d %-12.*s %u\n", 
                        r->symbol, obj->symbols[r->symbol].name_len, 
                        obj->symbols[r->symbol].name, r->offset);
            } else if (r->type == RELOCATE_REL32) {
                String_format_append(dest, "    SYMBOL REL32  %.3d %-12.*s %u\n", 
                        r->symbol, obj->symbols[r->symbol].name_len, 
                        obj->symbols[r->symbol].name, r->offset);
            } else if (r->type == RELOCATE_RVA32) {
                String_format_append(dest, "    SYMBOL RVA32  %.3d %-12.*s %u\n",
                        r->symbol, obj->symbols[r->symbol].name_len,
                        obj->symbols[r->symbol].name, r->offset);
            } else if (r->type == RELOCATE_ADR64) {
                String_format_append(dest, "    SYMBOL ADR64  %.3d %-12.*s %u\n",
                        r->symbol, obj->symbols[r->symbol].name_len,
                        obj->symbols[r->symbol].name, r->offset);
            } else {
                String_format_append(dest, "    UNKOWN\n");
            }
        }
        String_append(dest, '\n');
    }

    String_format_append(dest, "Symbols: %u\n", obj->symbol_count);
    uint32_t max_len = 8;
    for (uint32_t ix = 0; ix < obj->symbol_count; ++ix) {
        if (obj->symbols[ix].name_len > max_len) {
            max_len = obj->symbols[ix].name_len;
        }
    }
    if (max_len > 25) {
        max_len = 25;
    }
    for (uint32_t ix = 0; ix < obj->symbol_count; ++ix) {
        String_format_append(dest, " %.3d", ix);
        if (obj->symbols[ix].type == SYMBOL_STANDARD) {
            String_extend(dest, " STANDARD");
        } else if (obj->symbols[ix].type == SYMBOL_FUNCTION) {
            String_extend(dest, " FUNCTION");
        } else {
            String_extend(dest, " UNKNOWN ");
        }
        String_format_append(dest, " %-*.*s", max_len, 
                obj->symbols[ix].name_len,
                obj->symbols[ix].name);
        if (obj->symbols[ix].external) {
            String_extend(dest, " External");
        } else {
            String_extend(dest, " Static  ");
        }
        if (obj->symbols[ix].section == SECTION_IX_NONE) {
            String_extend(dest, " Undefined\n");
        } else {
            String_format_append(dest, " Section %u, Offset %u\n", 
                    obj->symbols[ix].section, obj->symbols[ix].offset);
        }
    }
}

Symbol* find_symbol(Object* obj, const uint8_t* sym, uint32_t sym_len) {
    symbol_ix* map = Object_build_map(obj);
    uint32_t map_size = obj->map_size;

    uint32_t h = hash(sym, sym_len, map_size);
    while (1) {
        if (map[h] == SYMBOL_IX_NONE) {
            return NULL;
        }
        Symbol* s = &obj->symbols[map[h]];
        if (s->name_len == sym_len && memcmp(sym, s->name, sym_len) == 0) {
            return s;
        }
        h = (h + 1) % map_size;
    }
}

bool Object_has_section(Object* obj, enum SectionType type) {
    for (uint32_t i = 0; i < obj->section_count; ++i) {
        if (obj->sections[i].type == type) {
            return true;
        }
    }
    return false;
}

void remap_section_relative(Object* obj) {
    for (uint32_t ix = 0; ix < obj->symbol_count; ++ix) {
        if (obj->symbols[ix].section_relative) {
            assert(obj->symbols[ix].offset == 0);
            const uint8_t* n;
            uint32_t len;
            if (obj->symbols[ix].section == SECTION_IX_NONE) {
                n = obj->symbols[ix].name;
                len = obj->symbols[ix].name_len;
            } else {
                n = obj->sections[obj->symbols[ix].section].name;
                len = strlen((const char*)n);
            }
            for (uint32_t j = 0; j < obj->section_count; ++j) {
                uint32_t nlen = strlen((const char*)obj->sections[j].name);
                if (len == nlen && memcmp(obj->sections[j].name, n, len) == 0) {
                    obj->symbols[ix].section = j;
                    break;
                }
            }
            assert(obj->symbols[ix].section != SECTION_IX_NONE);
        }
    }

}


void Linker_run(ObjectSet* objects, const ochar_t** argv, uint32_t argc,
                bool serialize_obj) {
    if (objects->object_count == 0) {
        LOG_ERROR("No linker input");
        return;
    }

    Library* libs = Mem_alloc(argc * sizeof(Library));
    if (libs == NULL) {
        out_of_memory(NULL);
    }

    LOG_INFO("Reading libraries");
    String filebuf;
    for (uint32_t ix = 0; ix < argc; ++ix) {
        if (!read_text_file(&filebuf, argv[ix])) {
            LOG_ERROR("Failed reading library '%s'", argv[ix]);
            return;
        }
        Libary_create(&libs[ix]);
        if (!PeLibrary_read(&libs[ix].objects, (const uint8_t*)filebuf.buffer,
                            filebuf.length)) {
            return;
        }
        String_free(&filebuf);
    }
    LOG_INFO("Done reading libraries");

    SymbolQueue queue;
    SymbolQueue_create(&queue);

    Object* dest = objects->objects[0];

    for (uint32_t i = 1; i < objects->object_count; ++i) {
        merge_objects(dest, objects->objects[i]);
        Mem_free(objects->objects[i]);
    }

    if (find_symbol(dest, (const uint8_t*)"main", 4) == NULL) {
        // main not defined in input
        SymbolQueue_append(&queue, (const uint8_t*)"main", 4);
    }

    for (uint32_t i = 0; i < dest->symbol_count; ++i) {
        if (dest->symbols[i].section == SECTION_IX_NONE) {
            SymbolQueue_append(&queue, dest->symbols[i].name,
                               dest->symbols[i].name_len);
        }
    }

    bool missing_symbol = false;

    while (SymbolQueue_size(&queue) > 0) {
        StrWithLength sym = SymbolQueue_get(&queue);

        Symbol* s = find_symbol(dest, sym.str, sym.len);
        if (s != NULL && s->section != SECTION_IX_NONE) {
            continue;
        }

        bool found = false;
        uint32_t old_sym_count = dest->symbol_count;

        for (uint32_t i = 0; i < argc; ++i) {
            if (Library_find_symbol(&libs[i], sym.str, sym.len, &queue)) {
                LOG_INFO("Found symbol '%*.*s' in lib %u", sym.len, sym.len, sym.str, i);
                found = true;
                break;
            }
        }
        if (!found) {
            missing_symbol = true;
            LOG_USER_ERROR("Missing symbol '%*.*s'", sym.len, sym.len, sym.str);
        }
    }

    if (missing_symbol) {
        return;
    }

    objects->object_count = 1;

    for (uint32_t i = 0; i < argc; ++i) {
        remap_section_relative(libs[i].dest);
        LOG_INFO("Reading library '%s'", argv[i]);

        LOG_INFO("Merging %s (%u sections)", argv[i], libs[i].dest->section_count);
        merge_objects(dest, libs[i].dest);
        Mem_free(libs[i].dest);
        libs[i].dest = NULL;
        Library_free(&libs[i]);
    }

    Mem_free(libs);

    // TODO: This is should be in pe_coff.c?
    if (Object_has_section(dest, SECTION_IDATA)) {
        uint8_t* name = Mem_alloc(9);
        if (name == NULL) {
            out_of_memory(NULL);
        }
        memcpy(name, ".idata$3", 9);
        section_ix s = Object_create_named_section(dest, SECTION_IDATA, name);
        // 20 == sizeof(ImportDirectoryEntry)
        for (uint32_t i = 0; i < 20; ++i) {
            Object_append_byte(dest, s, 0);
        }
        name = Mem_alloc(8);
        if (name == NULL) {
            out_of_memory(NULL);
        }
        memcpy(name, ".idata$3", 8);
        Object_add_symbol(dest, s, name, 8, false, SYMBOL_STANDARD, 0);

        for (section_ix ix = 0; ix < dest->section_count; ++ix) {
            if (strcmp((const char*)dest->sections[ix].name, ".idata$7") == 0) {
                uint32_t zero = 0;
                if (dest->sections[ix].data.size == 4 && 
                    memcmp(dest->sections[ix].data.data, &zero, 4) == 0) {
                    LOG_INFO("Removable .idata$7");
                    dest->sections[ix].relocation_count = 0;
                    dest->sections[ix].data.size = 0;
                }
            }
        }
    }
    for (symbol_ix i = 0; i < dest->symbol_count; ++i) {
        assert(dest->symbols[i].section != SECTION_IX_NONE);
    }

    combine_sections(dest);

    Symbol* entry = find_symbol(dest, (const uint8_t*)"main", 4);
    assert(entry != NULL);

    String s;
    if (serialize_obj && String_create(&s)) {
        Object_serialize(dest, &s);
        outputUtf8(s.buffer, s.length);
    }

    ByteBuffer data;
    Buffer_create(&data);
    PeExectutable_create(dest, &data, entry - dest->symbols);

    write_file("out.exe", data.data, data.size);
    LOG_USER_INFO("Created 'out.exe'");
}
