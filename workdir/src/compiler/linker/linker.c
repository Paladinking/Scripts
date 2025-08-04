#include "linker.h"
#include <mem.h>
#include <dynamic_string.h>
#include <coff.h>
#include <glob.h>
#include <printf.h>

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
}

void Object_free(Object* object) {
    Mem_free(object->symbols);
    for (uint32_t i = 0; i < object->section_count; ++i) {
        if (object->sections[i].type != SECTION_BSS) {
            Mem_free(object->sections[i].relocations);
        }
        if (object->sections[i].name != NULL) {
            Mem_free((uint8_t*)object->sections[i].name);
        }
    }
    Mem_free(object->sections);
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
    return map;
}

section_ix Object_create_section(Object* object, enum SectionType type) {
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
    s->name = NULL;
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

symbol_ix alloc_symbol(Object* object, section_ix section, const uint8_t* name,
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

    return object->symbol_count - 1;
}

symbol_ix Object_reserve_var(Object *object, section_ix section, 
                             const uint8_t *name, uint32_t name_len, 
                             uint32_t size, uint32_t align, bool external) {
    assert(section != SECTION_IX_NONE);
    Section* s = object->sections + section;
    assert(s->type == SECTION_BSS);

    uint32_t offset = ALIGNED_TO(s->data.size, align);
    s->data.size += size;
    if (align > s->align) {
        s->align = align;
    }

    return alloc_symbol(object, section, name, name_len, external,
                        SYMBOL_STANDARD, offset);
}

symbol_ix Object_declare_var(Object* object, section_ix section,
                             const uint8_t* name, uint32_t name_len,
                             uint32_t align, bool external) {
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

    return alloc_symbol(object, section, name, name_len, 
                        external, SYMBOL_STANDARD, offset);
}

symbol_ix Object_declare_fn(Object* object, const uint8_t* name, uint32_t name_len,
                            section_ix section, bool external) {
    uint32_t offset;
    if (section != SECTION_IX_NONE) {
        Section* s = object->sections + section;
        assert(s->type == SECTION_CODE);
        offset = s->data.size;
    } else {
        assert(external);
        offset = 0;
    }

    return alloc_symbol(object, section, name, name_len, 
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

// Combines two objects into one containing sections from both
// Does not yet merge sections into one
void merge_objects(Object* a, Object* b) {
    symbol_ix* map = Object_build_map(a);
    uint32_t map_count = a->symbol_count * 2;

    for (symbol_ix i = 0; i < b->symbol_count; ++i) {
        const uint8_t* name = b->symbols[i].name;
        uint32_t name_len = b->symbols[i].name_len;
        section_ix section = SECTION_IX_NONE;
        if (b->symbols[i].section != SECTION_IX_NONE) {
            section = b->symbols[i].section + a->section_count;
        }

        if (!b->symbols[i].external) {
            assert(section != SECTION_IX_NONE);
            symbol_ix sym = alloc_symbol(a, section, name,
                   name_len, false, b->symbols[i].type, b->symbols[i].offset);
            // Remap section
            b->symbols[i].section = sym;
            continue;
        }

        uint32_t h = hash(name, name_len, map_count);
        while (1) {
            symbol_ix sym = map[h];
            if (sym == SYMBOL_IX_NONE) {
                symbol_ix s = alloc_symbol(a, section, name, name_len, 
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
                    // TODO: free name?
                    a->symbols[sym].section = section;
                    a->symbols[sym].type = 
                    b->symbols[i].section = sym;
                    
                }
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

    Mem_free(b->sections);
    Mem_free(b->symbols);
    Mem_free(map);
}

const char* std_name(enum SectionType type, uint32_t* name_len) {
    switch (type) {
    case SECTION_BSS:
        *name_len = 4;
        return ".bss";
    case SECTION_CODE:
        *name_len = 5;
        return ".text";
    case SECTION_DATA:
        *name_len = 5;
        return ".data";
    case SECTION_RDATA:
        *name_len = 6;
        return ".rdata";
    default:
        assert(false);
    }
}

const char* section_name(Section* s, uint32_t* name_len) {
    const char* name = (const char*)s->name;
    if (name == NULL) {
        return std_name(s->type, name_len);
    }
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
        if (obj->sections[i].type < 0) {
            continue;
        }
        uint32_t name_len;
        const char* name = section_name(obj->sections + i, &name_len);

        section_ix match = SECTION_IX_NONE;
        const char* match_name = NULL;
        section_ix sec = Object_create_section(&dest, obj->sections[i].type);
        uint8_t* name_ptr = Mem_alloc(name_len + 1);
        if (name_ptr == NULL) {
            out_of_memory(NULL);
        }
        memcpy(name_ptr, name, name_len);
        name_ptr[name_len] = '\0';
        dest.sections[sec].name = name_ptr;
        while (1) {
            for (section_ix j = i; j < obj->section_count; ++j) {
                if (obj->sections[j].type < 0) {
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
            obj->sections[match].type = -1;

            uint32_t offset;
            if (obj->sections[match].align > dest.sections[i].align) {
                dest.sections[sec].align = obj->sections[match].align;
            }
            if (dest.sections[sec].type == SECTION_BSS) {
                while (dest.sections[sec].data.size % obj->sections[match].align != 0) {
                    ++dest.sections[sec].data.size;
                }
                offset = dest.sections[sec].data.size;
                dest.sections[sec].data.size += obj->sections[match].data.size;
            } else {
                while (dest.sections[sec].data.size % obj->sections[match].align != 0) {
                    Buffer_append(&dest.sections[sec].data, 0);
                }
                offset = dest.sections[sec].data.size;
                Buffer_extend(&dest.sections[sec].data, obj->sections[match].data.data,
                              obj->sections[match].data.size);
            }

            for (uint32_t i = 0; i < obj->sections[match].relocation_count; ++i) {
                // Note: this does not yet correct section value of 
                // RELOCATE_SECTION_* relocations
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
        assert(obj->sections[i].type < 0);
    }

    for (symbol_ix i = 0; i < obj->symbol_count; ++i) {
        section_ix sec = SECTION_IX_NONE;
        uint32_t offset = 0;
        if (obj->symbols[i].section != SECTION_IX_NONE) {
            sec = obj->sections[obj->symbols[i].section].relocation_count;
            offset = obj->sections[obj->symbols[i].section].align;
        }
        symbol_ix sym = alloc_symbol(&dest, sec, obj->symbols[i].name, 
                                     obj->symbols[i].name_len, 
                                     obj->symbols[i].external,
                                     obj->symbols[i].type, offset);
        assert(sym == i);
    }

    for (section_ix i = 0; i < dest.section_count; ++i) {
        for (uint32_t r = 0; r < dest.sections[i].relocation_count; ++r) {
            Relocation* reloc = dest.sections[i].relocations + r;
            if (reloc->type == RELOCATE_SECTION_RVA32 ||
                reloc->type == RELOCATE_SECTION_REL32) {
                reloc->section = obj->sections[reloc->section].relocation_count;
            }
        }
    }
    Object_free(obj);
    *obj = dest;
}


const uint8_t* pe_section_name(const SectionTableEntry* section, const uint8_t* buf, 
                               uint64_t str_table, uint64_t size, uint32_t* len) {
    const uint8_t* name;
    uint32_t name_len = 0;
    *len = 0;
    if (section->name[0] == '/') {
        // Parse from string table
        uint64_t offset;
        uint32_t ix = 1;
        if (section->name[ix] < '0' || section->name[ix] > '9') {
            LOG_WARNING("Invalid section name found");
            return NULL;
        }
        offset = (section->name[ix++] - '0');
        while (ix < 8 && section->name[ix] >= '0' && section->name[ix] <= '0') {
            offset = offset * 10 + section->name[ix++] - '0';
        }
        if (offset < 4 || str_table + offset > size) {
            LOG_WARNING("Invalid section name found");
            return NULL;
        }
        name = buf + str_table + offset;
        while (str_table + offset + name_len < size && name[name_len] != '\0') {
            ++name_len;
        }
    } else {
        name = section->name;
        while (name_len < 8 && name[name_len] != '\0') {
            ++name_len;
        }
    }
    *len = name_len;
    return name;
}


bool parse_pe_object(Object* obj, const uint8_t* buf, uint32_t size) {
    const CoffHeader* header = (const CoffHeader*)buf;
    if (header->machine != 0x8664) {
        return false;
    }
    if (header->size_of_optional_header != 0) {
        return false;
    }
    uint64_t symtab_start = header->pointer_to_symbol_table;
    uint64_t symtab_end = symtab_start + header->number_of_symbols * 18;
    if (symtab_end > size) {
        return false;
    }
    uint64_t sectab_start = sizeof(CoffHeader);
    uint64_t sectab_end = sectab_start +
        header->number_of_sections * sizeof(SectionTableEntry);
    if (sectab_end > size) {
        return false;
    }
    if (symtab_start < sectab_end) {
        return false;
    }

    Object_create(obj);
    uint64_t str_table = symtab_end;
    struct SymMap {
        uint32_t ix;
        bool is_section;
        bool is_auxilary;
    };
    struct SymMap* map = Mem_alloc(header->number_of_symbols * sizeof(struct SymMap));
    if (map == NULL) {
        out_of_memory(NULL);
    }

    for (uint32_t ix = 0; ix < header->number_of_symbols; ++ix) {
        StandardSymbol sym;
        memcpy(&sym, buf + symtab_start + 18 * ix, 18);
        const uint8_t* name;
        uint32_t name_len = 0;
        if (sym.name[0] == '\0') {
            uint32_t offset;
            memcpy(&offset, sym.name + 4, sizeof(uint32_t));
            if (str_table + offset > size || offset < 4) {
                LOG_WARNING("Invalid symbol name");
                goto fail;
            }
            name = buf + str_table + offset;
            while (str_table + offset + name_len < size && name[name_len] != '\0') {
                ++name_len;
            }
        } else {
            name = sym.name;
            while (name_len < 8 && name[name_len] != '\0') {
                ++name_len;
            }
        }

        if (name_len > 65535 || name_len == 0) {
            LOG_WARNING("Invalid symbol name");
            goto fail;
        }
        if (sym.storage_class != 2 && sym.storage_class != 3) {
            LOG_WARNING("Unkown symbol storage class %u", sym.storage_class);
            goto fail;
        }
        if (sym.section_number == 0xffff) {
            map[ix].is_auxilary = true;
            map[ix].is_section = false;
            continue;
        }
        if (sym.section_number < 0 || 
            sym.section_number >= header->number_of_sections) {
            LOG_WARNING("Unkown symbol section number %u", sym.section_number);
            goto fail;
        }
        bool external = sym.storage_class == 2;
        enum SymbolType type = SYMBOL_STANDARD;
        if ((sym.type & 0xF0) == 0x20) {
            type = SYMBOL_FUNCTION;
        }
        section_ix section_index = SECTION_IX_NONE;
        if (sym.section_number > 0) {
            section_index = sym.section_number - 1;
        }
        uint8_t* name_ptr = Mem_alloc(name_len);
        if (name_ptr == NULL) {
            out_of_memory(NULL);
        }
        memcpy(name_ptr, name, name_len);
        if (!external && section_index != SECTION_IX_NONE && sym.value == 0) {
            const SectionTableEntry* section = (const SectionTableEntry*)
                (buf + sectab_start + section_index * sizeof(SectionTableEntry));
            uint32_t sname_len = 0;
            const uint8_t* sname = pe_section_name(section, buf, str_table, size, &sname_len);
            if (sname == NULL) {
                goto fail;
            }
            if (sname_len == name_len && memcmp(name, sname, name_len) == 0) {
                // This is a section symbol
                map[ix].is_section = true;
                map[ix].ix = section_index;
            }
        } else {
            symbol_ix symbol = alloc_symbol(obj, section_index, name_ptr, 
                    name_len, external, type, sym.value);
            map[ix].is_section = false;
            map[ix].ix = symbol;
        }
        for (uint32_t i = 0; i < sym.number_of_aux_symbols; ++i) {
            ++ix;
            map[ix].is_auxilary = true;
            map[ix].is_section = false;
        }
    }

    for (uint32_t i = 0; i < header->number_of_sections; ++i) {
        const SectionTableEntry* section = (const SectionTableEntry*)
            (buf + sectab_start + i * sizeof(SectionTableEntry));
        uint32_t name_len = 0;
        const uint8_t* name = pe_section_name(section, buf, str_table, size, &name_len);
        if (name == NULL) {
            goto fail;
        }
        uint32_t align = SECTION_ALIGNMENT(section->characteristics);
        if (align == 0 || align > 8192) {
            LOG_WARNING("Invalid section alignment");
            goto fail;
        }

        uint8_t* name_ptr = Mem_alloc(name_len);
        if (name_ptr == NULL) {
            out_of_memory(NULL);
        }
        memcpy(name_ptr, name, name_len);
        name_ptr[name_len] = '\0';

        enum SectionType type;
        if (section->size_of_raw_data > 0 && section->pointer_to_raw_data == 0) {
            if (section->characteristics & IMAGE_SCN_MEM_EXECUTE) {
                LOG_WARNING(".bss section is executable");
                goto fail;
            }
            type = SECTION_BSS;
        } else if (section->characteristics & IMAGE_SCN_CNT_CODE) {
            type = SECTION_CODE;
        } else if (section->characteristics & IMAGE_SCN_CNT_UNINITIALIZED_DATA) {
            if (section->pointer_to_raw_data != 0 && section->size_of_raw_data != 0) {
                LOG_WARNING("Uninitialized data section contains data");
                goto fail;
            }
            if (section->characteristics & IMAGE_SCN_MEM_EXECUTE) {
                LOG_WARNING(".bss section is executable");
                goto fail;
            }
            type = SECTION_BSS;
        } else if (section->characteristics & IMAGE_SCN_MEM_WRITE) {
            type = SECTION_DATA;
        } else {
            type = SECTION_RDATA;
        }

        section_ix sec = Object_create_section(obj, type);
        obj->sections[sec].name = name;
        obj->sections[sec].align = align;
        if (type == SECTION_BSS) {
            obj->sections[sec].data.size = section->size_of_raw_data;
        } else {
            uint64_t data_end = section->pointer_to_raw_data + 
                                section->size_of_raw_data;
            if (data_end > size) {
                LOG_WARNING("Section data out of bounds");
                goto fail;
            }
            Object_append_data(obj, sec, buf + section->pointer_to_raw_data,
                               section->size_of_raw_data);
        }
        uint64_t reloc_start = section->pointer_to_relocations;
        uint32_t reloc_count = section->number_of_relocations;
        if (section->characteristics & IMAGE_SCN_LNK_NRELOC_OVFL) {
            if (reloc_start + 10 > size) {
                LOG_WARNING("Section relocations out of bounds");
                goto fail;
            }
            CoffRelocation r;
            memcpy(&r, buf + reloc_start, 10);
            reloc_count = r.vritual_address;
            reloc_start += 10;
        }
        uint64_t reloc_end = reloc_start + reloc_count * 10;
        if (reloc_end > size) {
            LOG_WARNING("Section relocations out of bounds");
            goto fail;
        }
        if (type == SECTION_BSS && reloc_count > 0) {
            LOG_WARNING("Uninitialized data section has relocations");
            goto fail;
        }
        for (uint32_t j = 0; j < reloc_count; ++j) {
            CoffRelocation r;
            memcpy(&r, buf + reloc_start + i * 10, 10);
            Relocation reloc;
            reloc.offset = r.vritual_address - section->virtual_address;
            if (reloc.offset + 4 > obj->sections[sec].data.size) {
                LOG_WARNING("Relocation offset out of bounds");
                goto fail;
            }
            if (r.type == IMAGE_REL_AMD64_ABSOLUTE) {
                continue;
            }
            if (r.symbol_table_index >= header->number_of_symbols ||
                map[r.symbol_table_index].is_auxilary) {
                LOG_WARNING("Invalid relocation symbol");
                goto fail;
            }
            if (r.type == IMAGE_REL_AMD64_ADDR64) {
                if (reloc.offset + 8 > obj->sections[sec].data.size) {
                    LOG_WARNING("Relocation offset out of bounds");
                    goto fail;
                }
                if (map[r.symbol_table_index].is_section) {
                    reloc.section = map[r.symbol_table_index].ix;
                    reloc.type = RELOCATE_SECTION_ADR64;
                } else {
                    reloc.symbol = map[r.symbol_table_index].ix;
                    reloc.type = RELOCATE_ADR64;
                }
            } else if (r.type == IMAGE_REL_AMD64_ADDR32NB) {
                if (map[r.symbol_table_index].is_section) {
                    reloc.section = map[r.symbol_table_index].ix;
                    reloc.type = RELOCATE_SECTION_RVA32;
                } else {
                    reloc.symbol = map[r.symbol_table_index].ix;
                    reloc.type = RELOCATE_RVA32;
                }
            } else if (r.type == IMAGE_REL_AMD64_REL32) {
                if (map[r.symbol_table_index].is_section) {
                    reloc.section = map[r.symbol_table_index].ix;
                    reloc.type = RELOCATE_SECTION_REL32;
                } else {
                    reloc.symbol = map[r.symbol_table_index].ix;
                    reloc.type = RELOCATE_REL32;
                }
            } else {
                LOG_WARNING("Unkown section");
                goto fail;
            }
            Object_add_relocation(obj, sec, reloc);
        }
    }
    Mem_free(map);
    return true;
fail:
    Object_free(obj);
    Mem_free(map);
    return false;
}

void serialize_object(Object* obj, String* dest) {
    for (uint32_t i = 0; i < obj->section_count; ++i) {
        if (obj->sections[i].name == NULL) {
            uint32_t l;
            String_format_append(dest, "Section %u %s:\n", i,
                    std_name(obj->sections[i].type, &l));
        } else {
            String_format_append(dest, "Section %u %s:\n", i, obj->sections[i].name);
        }
        String_extend(dest, "  Type: ");
        if (obj->sections[i].type == SECTION_CODE) {
            String_extend(dest, "Code\n");
        } else if (obj->sections[i].type == SECTION_BSS) {
            String_extend(dest, "Uninitialized data\n");
        } else if (obj->sections[i].type == SECTION_DATA) {
            String_extend(dest, "Initialized data\n");
        } else if (obj->sections[i].type == SECTION_RDATA) {
            String_extend(dest, "Read-only data\n");
        } else {
            String_extend(dest, "Unkown\n");
        }
        String_format_append(dest, "  Align: %u\n", obj->sections[i].align);
        String_format_append(dest, "  Size: %u\n", obj->sections[i].data.size);
        String_format_append(dest, "  Relocations: %u\n", obj->sections[i].relocation_count);
        for (uint32_t ix = 0; ix < obj->sections[i].relocation_count; ++ix) {
            Relocation* r = obj->sections[i].relocations + ix;
            if (r->type == RELOCATE_REL32) {
                String_format_append(dest, "    SYMBOL REL32  %.3d %-12.*s %u\n", r->symbol,
                        obj->symbols[r->symbol].name_len, obj->symbols[r->symbol].name,
                        r->offset);
            } else if (r->type == RELOCATE_RVA32) {
                String_format_append(dest, "    SYMBOL RVA32  %.3d %-12.*s %u\n", r->symbol,
                        obj->symbols[r->symbol].name_len, obj->symbols[r->symbol].name,
                        r->offset);
            } else if (r->type == RELOCATE_SECTION_REL32) {
                uint32_t len;
                const char* s = section_name(obj->sections + r->section, &len);
                String_format_append(dest, "    SECTION RVA32 %.3d %-12s %u\n", r->section,
                        s, r->offset);
            } else if (r->type == RELOCATE_REL32) {
                uint32_t len;
                const char* s = section_name(obj->sections + r->section, &len);
                String_format_append(dest, "    SECTION RVA32 %.3d %-12s %u\n", r->section,
                        s, r->offset);
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
        if (obj->symbols[ix].type == SYMBOL_STANDARD) {
            String_extend(dest, "  STANDARD");
        } else if (obj->symbols[ix].type == SYMBOL_FUNCTION) {
            String_extend(dest, "  FUNCTION");
        } else {
            String_extend(dest, "  UNKNOWN ");
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


void Linker_run(Object** objects, uint32_t object_count, const ochar_t** argv, uint32_t argc) {
    {
        Object o;
        String s;
        if (read_text_file(&s, oL("build\\embed.obj"))) {
            if (parse_pe_object(&o, (uint8_t*)s.buffer, s.length)) {
                String_clear(&s);
                serialize_object(&o, &s);
                outputUtf8(s.buffer, s.length);
                Object_free(&o);
            }

        }
    }
    assert(object_count == 1);
    String s;
    if (String_create(&s)) {
        serialize_object(objects[0], &s);
        outputUtf8(s.buffer, s.length);
    } 
}
