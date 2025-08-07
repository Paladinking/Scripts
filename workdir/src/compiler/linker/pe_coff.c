#include "linker.h"
#include "portable_executable.h"
#include "pe_coff.h"
#include <mem.h>
// for parse_uint
#include <args.h>
#include <printf.h>

#define SECTION_ALIGN 4096
#define FILE_ALIGNMENT 512

const uint8_t DOS_STUB[] = {77, 90, 128, 0, 1, 0, 0, 0, 4, 0, 16, 0, 255, 255, 0, 0, 64, 1, 0, 0, 0, 0, 0, 0, 64, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 128, 0, 0, 0, 14, 31, 186, 14, 0, 180, 9, 205, 33, 184, 1, 76, 205, 33, 84, 104, 105, 115, 32, 112, 114, 111, 103, 114, 97, 109, 32, 99, 97, 110, 110, 111, 116, 32, 98, 101, 32, 114, 117, 110, 32, 105, 110, 32, 68, 79, 83, 32, 109, 111, 100, 101, 46, 13, 10, 36, 0, 0, 0, 0, 0, 0, 0, 0};

const uint8_t ZEROES[512] = {0};

typedef struct SectionData {
    const uint8_t* data;
    uint32_t size;
} SectionData;

typedef struct Coff64Image {
    CoffHeader header;
    OptionalHeader64* optional_header;
    SectionTableEntry* section_table;
    SectionData* section_data;
} Coff64Image;


bool Coff64Image_create(Coff64Image* img) {
    OptionalHeader64* header = Mem_alloc(sizeof(OptionalHeader64) + 
                                         16 * sizeof(struct DataDirectory));

    if (header == NULL) {
        return false;
    }
    img->header.machine = 0x8664;
    img->header.number_of_sections = 0;
    // TODO: Set this before writing file
    img->header.timedate_stamp = 0;
    img->header.pointer_to_symbol_table = 0;
    img->header.number_of_symbols = 0;
    img->header.size_of_optional_header = sizeof(OptionalHeader64) +
                                          16 * sizeof(struct DataDirectory);
    // Executable, Large adress aware
    img->header.characteristics = 0x0002 | 0x0020;

    img->optional_header = header;
    img->section_table = NULL;

    header->magic = 0x20b;
    header->major_linker_ver = 0;
    header->minor_linker_ver = 0;
    header->size_of_code = 0;
    header->size_of_initialized_data = 0;
    header->size_of_uninitialized_data = 0;
    header->address_of_entry_point = 0;
    header->base_of_code = 0;

    header->image_base = 0x140000000;
    header->section_alignment = SECTION_ALIGN;
    header->file_alignment = FILE_ALIGNMENT;
    header->major_os_version = 6;
    header->minor_os_version = 0;
    header->major_image_version = 0;
    header->minor_image_version = 0;
    header->major_subsystem_version = 6;
    header->minor_subsystem_version = 0;
    header->win32_version = 0;
    header->win32_version = 0;
    header->size_of_image = SECTION_ALIGN;
    header->size_of_headers = FILE_ALIGNMENT;
    header->checksum = 0;
    header->subsystem = 3; // Console
    header->dll_characteristics = 0;
    header->size_of_stack_reserve = 4096 * 128;
    header->size_of_stack_commit = 4096;
    header->size_of_heap_reserve = 4096 * 256;
    header->size_of_heap_commit = 4096;
    header->loader_flags = 0;
    header->number_of_rvas_and_sizes = 16;

    for (int i = 0; i < 16; ++i) {
        header->data_directories[i].size = 0;
        header->data_directories[i].virtual_address_rva = 0;
    }

    return true;
}

SectionTableEntry* add_section(Coff64Image* img, const char* name, uint32_t file_size,
                               uint32_t virtual_size, const uint8_t* data, uint32_t size) {
    uint64_t name_len = strlen(name);
    if (name_len > 8) {
        return false;
    }

    uint32_t virtual_offset;
    if (img->header.number_of_sections > 0) {
        SectionTableEntry* new_st = Mem_realloc(img->section_table,
                (img->header.number_of_sections + 1) * sizeof(SectionTableEntry));
        if (new_st == NULL) {
            return false;
        }
        img->section_table = new_st;
        SectionData* new_sd = Mem_realloc(img->section_data,
                (img->header.number_of_sections + 1) * sizeof(SectionData));
        if (new_sd == NULL) {
            return false;
        }
        img->section_data = new_sd;
        SectionTableEntry* last_section = &img->section_table[img->header.number_of_sections - 1];
        uint64_t last_end = last_section->virtual_address + 
            ALIGNED_TO(last_section->virtual_size, SECTION_ALIGN);
        virtual_offset = last_end;
    } else {
        img->section_table = Mem_alloc(sizeof(SectionTableEntry));
        if (img->section_table == NULL) {
            return false;
        }
        img->section_data = Mem_alloc(sizeof(SectionData));
        if (img->section_data == NULL) {
            Mem_free(img->section_table);
            return false;
        }
        virtual_offset = SECTION_ALIGN;
    }
    SectionTableEntry* e = img->section_table + img->header.number_of_sections;
    memset(e->name, 0, 8);
    memcpy(e->name, name, name_len);
    e->virtual_address = virtual_offset;
    e->virtual_size = virtual_size;
    e->size_of_raw_data = file_size;
    e->pointer_to_relocations = 0;
    e->pointer_to_line_numbers = 0;
    e->number_of_relocations = 0;
    e->number_of_line_numbers = 0;
    e->characteristics = 0;

    img->header.number_of_sections += 1;
    img->optional_header->size_of_image += ALIGNED_TO(virtual_size, SECTION_ALIGN);

    uint64_t h = sizeof(DOS_STUB) + 4 + sizeof(OptionalHeader64) + 
        img->optional_header->number_of_rvas_and_sizes * sizeof(struct DataDirectory) + 
        img->header.number_of_sections * sizeof(SectionTableEntry);
    img->optional_header->size_of_headers = ALIGNED_TO(h, FILE_ALIGNMENT);

    img->section_table[0].pointer_to_raw_data = img->optional_header->size_of_headers;
    for (uint32_t i = 1; i < img->header.number_of_sections; ++i) {
        img->section_table[i].pointer_to_raw_data = 
            img->section_table[i - 1].pointer_to_raw_data + 
            img->section_table[i - 1].size_of_raw_data;
    }

    img->section_data[img->header.number_of_sections - 1].size = size;
    img->section_data[img->header.number_of_sections - 1].data = data;

    return e;
}

bool Coff64Image_add_code_section(Coff64Image* img, const uint8_t* data,
                                  uint32_t size, const char* name) {

    uint32_t aligned_size = ALIGNED_TO(size, FILE_ALIGNMENT);
    if (!add_section(img, name, aligned_size, size, data, size)) {
        return false;
    }

    SectionTableEntry* e = img->section_table + img->header.number_of_sections - 1;

    if (img->optional_header->base_of_code == 0) {
        img->optional_header->base_of_code = e->virtual_address;
    }
    // Code, Read, Execute
    e->characteristics = IMAGE_SCN_CNT_CODE | IMAGE_SCN_MEM_READ | 
                         IMAGE_SCN_MEM_EXECUTE;
    img->optional_header->size_of_code += aligned_size;

    return true;
}

bool Coff64Image_add_data_section(Coff64Image* img, const uint8_t* data,
                                  uint32_t size, uint32_t virtual_size, 
                                  const char* name) {
    if (virtual_size < size) {
        return false;
    }
    uint32_t aligned_size = ALIGNED_TO(size, FILE_ALIGNMENT);
    if (!add_section(img, name, aligned_size, virtual_size, data, size)) {
        return false;
    }

    SectionTableEntry* e = img->section_table + img->header.number_of_sections - 1;

    // Initialized data, Read, Write
    e->characteristics = IMAGE_SCN_CNT_INITIALIZED_DATA |
                         IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE;
    img->optional_header->size_of_initialized_data += aligned_size;

    return true;
}

bool Coff64Image_add_rdata_section(Coff64Image* img, const uint8_t* data,
                                   uint32_t size, const char* name) {
    uint32_t aligned_size = ALIGNED_TO(size, FILE_ALIGNMENT);
    if (!add_section(img, name, aligned_size, size, data, size)) {
        return false;
    }

    SectionTableEntry* e = img->section_table + img->header.number_of_sections - 1;

    // Initialized data, Read 
    e->characteristics = IMAGE_SCN_CNT_INITIALIZED_DATA |
                         IMAGE_SCN_MEM_READ;
    img->optional_header->size_of_initialized_data += aligned_size;

    return true;
}

void Coff64Image_set_entrypoint(Coff64Image* img, uint32_t entry_rva) {
    img->optional_header->address_of_entry_point = entry_rva;
}

void Coff64Image_write(Coff64Image* img, ByteBuffer* dest) {
    Buffer_extend(dest, DOS_STUB, sizeof(DOS_STUB));
    uint8_t signature[4] = {'P', 'E', 0, 0};
    Buffer_extend(dest, signature, 4);
    Buffer_extend(dest, (uint8_t*)&img->header, sizeof(CoffHeader));
    uint32_t opt_size = sizeof(OptionalHeader64) + 
        img->optional_header->number_of_rvas_and_sizes * sizeof(struct DataDirectory);
    Buffer_extend(dest, (uint8_t*)img->optional_header, opt_size);

    for (uint32_t i = 0; i < img->header.number_of_sections; ++i) {
        Buffer_extend(dest, (uint8_t*)&img->section_table[i], 
                      sizeof(SectionTableEntry));
    }

    uint64_t file_offset = sizeof(DOS_STUB) + 4 + sizeof(CoffHeader) + opt_size + 
        img->header.number_of_sections * sizeof(SectionTableEntry);

    for (uint32_t i = 0; i < img->header.number_of_sections; ++i) {
        while (file_offset < img->section_table[i].pointer_to_raw_data) {
            uint32_t to_write = 512;
            if (img->section_table[i].pointer_to_raw_data - file_offset < 512) {
                to_write = img->section_table[i].pointer_to_raw_data - file_offset;
            }
            Buffer_extend(dest, ZEROES, to_write);
            file_offset += to_write;
        }
        Buffer_extend(dest, img->section_data[i].data, img->section_data[i].size);
        file_offset += img->section_data[i].size;
    }

    uint64_t remaining = ALIGNED_TO(file_offset, FILE_ALIGNMENT) - file_offset;
    while (remaining > 0) {
        uint32_t to_write = 512;
        if (remaining < to_write) {
            to_write = remaining;
        }
        Buffer_extend(dest, ZEROES, to_write);
        remaining -= to_write;
    }
}

void Coff64Image_free(Coff64Image* img) {
    if (img->header.number_of_sections > 0) {
        Mem_free(img->section_data);
        Mem_free(img->section_table);
    }
    Mem_free(img->optional_header);
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

Object* PeObject_read(const uint8_t* buf, uint64_t size) {
    const CoffHeader* header = (const CoffHeader*)buf;
    if (header->machine != 0x8664) {
        LOG_WARNING("Invalid machine type");
        return NULL;
    }
    if (header->size_of_optional_header != 0) {
        LOG_WARNING("Object contains optional header");
        return NULL;
    }
    uint64_t symtab_start = header->pointer_to_symbol_table;
    uint64_t symtab_end = symtab_start + header->number_of_symbols * 18;
    if (symtab_end > size) {
        LOG_WARNING("Object symbol table out of bounds");
        return NULL;
    }
    uint64_t sectab_start = sizeof(CoffHeader);
    uint64_t sectab_end = sectab_start +
        header->number_of_sections * sizeof(SectionTableEntry);
    if (sectab_end > size) {
        LOG_WARNING("Object section table out of bounds");
        return NULL;
    }
    if (symtab_start < sectab_end) {
        LOG_WARNING("Object section table overlaps symbol table");
        return NULL;
    }

    Object* obj = Mem_alloc(sizeof(Object));
    Object_create(obj);
    uint64_t str_table = symtab_end;
    struct SymMap {
        uint32_t ix;
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
        if (sym.section_number == 0xffff || 
            sym.storage_class == STORAGE_CLASS_FILE) {

            map[ix].is_auxilary = true;
            for (uint32_t i = 0; i < sym.number_of_aux_symbols; ++i) {
                ++ix;
                map[ix].is_auxilary = true;
            }
            continue;
        }
        if (sym.storage_class != STORAGE_CLASS_STATIC && 
            sym.storage_class != STORAGE_CLASS_EXTERNAL &&
            sym.storage_class != STORAGE_CLASS_SECTION) {
            LOG_WARNING("Unkown symbol storage class %u", sym.storage_class);
            goto fail;
        }

        if (sym.section_number < 0 || 
            sym.section_number > header->number_of_sections) {
            LOG_WARNING("Unkown symbol section number %u", sym.section_number);
            goto fail;
        }
        bool external = sym.storage_class == STORAGE_CLASS_EXTERNAL;
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
            symbol_ix symbol = Object_add_symbol(obj, section_index, name_ptr, 
                    name_len, external, type, sym.value);
            map[ix].ix = symbol;
            map[ix].is_auxilary = false;
            if (sname_len == name_len && memcmp(name, sname, name_len) == 0 &&
                section->size_of_raw_data == 0 && section->pointer_to_raw_data == 0) {
                // This is a section symbol refering to an empty section
                // Relocations relative this symbol are section relative
                obj->symbols[symbol].section_relative = true;
            }
        } else {
            bool section_relative = false;
            if (sym.storage_class == STORAGE_CLASS_SECTION) {
                sym.value = 0;
                if (section_index == SECTION_IX_NONE) {
                    section_relative = true;
                }
            }
            symbol_ix symbol = Object_add_symbol(obj, section_index, name_ptr, 
                    name_len, external, type, sym.value);
            map[ix].is_auxilary = false;
            map[ix].ix = symbol;
            obj->symbols[symbol].section_relative = section_relative;
        }
        for (uint32_t i = 0; i < sym.number_of_aux_symbols; ++i) {
            ++ix;
            map[ix].is_auxilary = true;
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
        uint32_t align = IMAGE_SCN_ALIGNMENT(section->characteristics);
        if (align == 0 || align > 8192) {
            LOG_WARNING("Invalid section alignment");
            goto fail;
        }

        uint8_t* name_ptr = Mem_alloc(name_len + 1);
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
            if (name_len >= 6 && memcmp(name_ptr, ".idata", 6) == 0 &&
                (name_len == 6 || name_ptr[6] == '$')) {
                type = SECTION_IDATA;
            } else {
                type = SECTION_DATA;
            }
        } else {
            type = SECTION_RDATA;
        }

        section_ix sec = Object_create_named_section(obj, type, name_ptr);
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
        LOG_DEBUG("Parsing section '%s' relocations", name_ptr);
        for (uint32_t j = 0; j < reloc_count; ++j) {
            LOG_DEBUG("Parseing relocation %u", j);
            CoffRelocation r;
            memcpy(&r, buf + reloc_start + j * 10, 10);
            Relocation reloc;
            reloc.offset = r.vritual_address - section->virtual_address;
            if (reloc.offset + 4 > obj->sections[sec].data.size) {
                LOG_WARNING("Relocation offset %u out of bounds for section of size %u",
                             r.vritual_address, obj->sections[sec].data.size);
                goto fail;
            }
            if (r.type == IMAGE_REL_AMD64_ABSOLUTE) {
                continue;
            }
            if (r.symbol_table_index >= header->number_of_symbols ||
                map[r.symbol_table_index].is_auxilary) {
                LOG_WARNING("Invalid relocation symbol %u",
                    r.symbol_table_index);
                goto fail;
            }
            reloc.symbol = map[r.symbol_table_index].ix;
            if (r.type == IMAGE_REL_AMD64_ADDR64) {
                if (reloc.offset + 8 > obj->sections[sec].data.size) {
                    LOG_WARNING("Relocation offset out of bounds");
                    goto fail;
                }
                reloc.type = RELOCATE_ADR64;
            } else if (r.type == IMAGE_REL_AMD64_ADDR32NB) {
                reloc.type = RELOCATE_RVA32;
            } else if (r.type == IMAGE_REL_AMD64_REL32) {
                reloc.type = RELOCATE_REL32;
            } else {
                LOG_WARNING("Unkown relocation type");
                goto fail;
            }
            Object_add_relocation(obj, sec, reloc);
        }
    }
    Mem_free(map);
    return obj;
fail:
    Object_free(obj);
    Mem_free(obj);
    Mem_free(map);
    return NULL;
}

static uint64_t read_ascii_num(const uint8_t *buf, uint32_t len) {
    ochar_t data[33];
    memset(data, 0, 33 * sizeof(ochar_t));
    uint32_t ix = 0;
    for (; ix < len; ++ix) {
        if (buf[ix] >= '0' && buf[ix] <= '9') {
            data[ix] = buf[ix];
        } else {
            break;
        }
    }
    if (ix == 0) {
        return 0;
    }
    uint64_t res;
    parse_uint(data, &res, 10);
    return res;
}

uint8_t* idata_name(char c) {
    uint8_t* ptr = Mem_alloc(9);
    if (ptr == NULL) {
        out_of_memory(NULL);
    }
    memcpy(ptr, ".idata$", 7);
    ptr[7] = c;
    ptr[8] = '\0';
    return ptr;
}

Object* parse_short_import(const ImportHeader* header, const uint8_t* buf,
                           const uint8_t* ext_name, uint32_t ext_len) {
    const uint8_t* sym = buf;
    uint32_t sym_len = 0;

    uint16_t hint = 0;
    uint32_t name_type = ((header->type) >> 2) & 0x7;
    if (name_type != 0) {
        hint = header->ordinal_or_hint;
    }

    while (sym_len < header->size_of_data && sym[sym_len] != '\0') {
        ++sym_len;
    }

    if (sym_len == 0) {
        LOG_WARNING("Empty symbol name in short import");
        return NULL;
    }

    Object* o = Mem_alloc(sizeof(Object));
    if (o == NULL) {
        out_of_memory(NULL);
    }
    // For now assumes import libraries only contain one dll,
    // so dll name is ignored

    Object_create(o);

    section_ix text = Object_create_section(o, SECTION_CODE);
    o->sections[text].align = 4;
    section_ix idata4 = Object_create_named_section(o, SECTION_IDATA, 
            idata_name('4'));
    o->sections[idata4].align = 4;
    section_ix idata5 = Object_create_named_section(o, SECTION_IDATA, 
            idata_name('5'));
    o->sections[idata5].align = 4;
    section_ix idata6 = Object_create_named_section(o, SECTION_IDATA, 
            idata_name('6'));
    o->sections[idata6].align = 2;

    symbol_ix sym_idata6 = Object_add_symbol(o, idata6, idata_name('6'), 8,
                                             false, SYMBOL_STANDARD, 0);

    symbol_ix sym_ix = Object_declare_fn(o, text, sym, sym_len, true);
    symbol_ix imp_sym_ix = Object_declare_import(o, idata5, sym, sym_len);

    Object_declare_var(o, SECTION_IX_NONE, ext_name, ext_len, 0, true);

    Object_append_byte(o, text, 0xff);
    Object_append_byte(o, text, 0x25);
    Object_add_reloc(o, text, imp_sym_ix, RELOCATE_REL32);
    for (uint32_t i = 0; i < 4; ++i) {
        Object_append_byte(o, text, 0x0);
    }
    Object_append_byte(o, text, 0x90);
    Object_append_byte(o, text, 0x90);

    Object_add_reloc(o, idata4, sym_idata6, RELOCATE_RVA32);
    Object_add_reloc(o, idata5, sym_idata6, RELOCATE_RVA32);
    for (uint32_t i = 0; i < 8; ++i) {
        Object_append_byte(o, idata4, 0x0);
        Object_append_byte(o, idata5, 0x0);
    }

    Object_append_data(o, idata6, (const uint8_t*)&hint, sizeof(uint16_t));
    Object_append_data(o, idata6, sym, sym_len);
    Object_append_byte(o, idata6, 0x0);
    if (o->sections[idata6].data.size % 2 != 0) {
        Object_append_byte(o, idata6, 0x0);
    }

    return o;
}

bool PeLibrary_read(ObjectSet* dest, const uint8_t* buf, uint64_t size) {
    if (size < 8 || memcmp(buf, "!<arch>\n", 8) != 0) {
        return false;
    }
    uint64_t offset = 8;
    const uint8_t* ext_name = NULL;
    uint32_t ext_len = 0;

    while (size - offset >= sizeof(ArchiveMemberHeader)) {
        const ArchiveMemberHeader* h = (const ArchiveMemberHeader*)(buf + offset);
        offset += sizeof(ArchiveMemberHeader);
        uint64_t member_size = read_ascii_num(h->size, 10);
        if (offset + member_size > size) {
            LOG_WARNING("Archive member out of bounds");
            return false;
        }
        if (h->end[0] != 0x60 || h->end[1] != 0xA) {
            LOG_WARNING("Invalid Archive member header");
            return false;
        }

        bool special = h->name[0] == '/' &&
            (h->name[1] == ' ' || h->name[1] == '/');
        if (special || member_size < sizeof(CoffHeader)) {
            offset += member_size;
            if (offset % 2 != 0) {
                ++offset;
            }
            continue;
        }

        uint32_t name_len = 0;
        while (name_len < 16 && h->name[name_len] != '/') {
            ++name_len;
        }
        if (name_len == 0) {
            // name is in longname table...
            // This gives ascii index into longname table
            while (name_len < 16 && h->name[name_len] != ' ') {
                ++name_len;
            }
        }

        ImportHeader header;
        // memcpy due to stricter alignment reqirements
        memcpy(&header, buf + offset, sizeof(ImportHeader));
        if (header.sig1 == 0x0 && header.sig2 == 0xffff) {
            if (header.size_of_data + sizeof(ImportHeader) > member_size) {
                LOG_WARNING("Invalid short import member");
                return false;
            }
            LOG_DEBUG("Reading Archive short import '%*.*s'", name_len, name_len,
                     h->name);
            // Short form import lib
            Object* o = parse_short_import(&header, buf + offset + 
                                           sizeof(ImportHeader),
                                           ext_name, ext_len);
            ObjectSet_add(dest, o);
            offset += member_size;
            if (offset % 2 != 0) {
                ++offset;
            }
            continue;
        }
        LOG_DEBUG("Reading Archive member object '%*.*s'", name_len, name_len,
                h->name);
        Object* obj = PeObject_read(buf + offset, member_size);
        if (obj == NULL) {
            LOG_WARNING("Invalid object");
            return false;
        }
        if (ext_name == NULL) {
            for (section_ix i = 0; i < obj->section_count; ++i) {
                if (obj->sections[i].type != SECTION_IDATA) {
                    continue;
                }
                if (obj->sections[i].name[6] != '$' || 
                    obj->sections[i].name[7] != '2') {
                    continue;
                }
                if (obj->sections[i].data.size < 20) {
                    continue;
                }
                // this object has an .idata$2 section containing Import dir
                for (symbol_ix i = 0; i < obj->symbol_count; ++i) {
                    if (obj->symbols[i].external &&
                        obj->symbols[i].section != SECTION_IX_NONE) {
                        ext_name = obj->symbols[i].name;
                        ext_len = obj->symbols[i].name_len;
                        break;
                    }
                }
                break;
            }
        }
        ObjectSet_add(dest, obj);
        offset += member_size;
        if (offset % 2 != 0) {
            ++offset;
        }
    }

    return true;
}

// Puts .idata into .rdata, and computes offset and sizef of directory entries
void rewrite_idata(Object* obj, section_ix section, uint32_t* iat_offset,
                   uint32_t* iat_size, uint32_t* id_offset, uint32_t* id_size,
                   section_ix* import_section) {
    Section* s = obj->sections + section;
    if (s->align < 16) {
        s->align = 16;
    }
    *id_offset = 0;

    section_ix rdata = 0;
    for (; rdata < obj->section_count; ++rdata) {
        if (obj->sections[rdata].type == SECTION_RDATA) {
            break;
        }
    }

    uint32_t importdir_end = 1;
    for (symbol_ix i = 0; i < obj->symbol_count; ++i) {
        Symbol* sym = &obj->symbols[obj->symbol_count - i - 1];
        if (sym->name_len == 8 && memcmp(sym->name, ".idata$3", 8) == 0) {
            importdir_end = sym->offset;
            break;
        }
    }
    if (importdir_end % 20 != 0) {
        LOG_ERROR("Invalid .idata section");
        assert(false);
    }

    *id_size = importdir_end + 20;
    *iat_offset = 0xffffffff;
    uint32_t last_ait = 0;

    for (uint32_t i = 0; i < s->relocation_count; ++i) {
        Relocation* r = s->relocations + i;
        if (r->offset < importdir_end && (r->offset % 20) == 16) {
            if (r->offset < *iat_offset) {
                *iat_offset = r->offset;
            }
            if (r->offset > last_ait) {
                last_ait = r->offset;
            }
        }
        if (r->type == RELOCATE_RVA32) {
            uint32_t val = 0xffffffff;
            memcpy(s->data.data + r->offset, &val, sizeof(uint32_t));
        }
    }

    assert(*iat_offset != 0xffffffff);
    assert(last_ait != 0 && last_ait + 8 <= s->data.size);

    while (1) {
        uint64_t v;
        memcpy(&v, s->data.data + last_ait, sizeof(uint64_t));
        if (last_ait + 8 > s->data.size) {
            break;
        }
        last_ait += 8;
        if (v == 0) {
            break;
        }
    }
    *iat_size = last_ait - *iat_offset;

    if (rdata == obj->section_count) {
        s->type = SECTION_RDATA;
        *import_section = section;
        return;
    }
    *import_section = rdata;

    while (obj->sections[rdata].data.size % s->align != 0) {
        Object_append_byte(obj, rdata, 0);
    }
    if (obj->sections[rdata].align < s->align) {
        obj->sections[rdata].align = s->align;
    }

    uint32_t offset = obj->sections[rdata].data.size;
    Object_append_data(obj, rdata, s->data.data, s->data.size);

    for (uint32_t i = 0; i < s->relocation_count; ++i) {
        Relocation r = s->relocations[i];
        r.offset += offset;
        Object_add_relocation(obj, rdata, r);
    }

    for (uint32_t i = 0; i < obj->symbol_count; ++i) {
        if (obj->symbols[i].section == section) {
            obj->symbols[i].offset += offset;
            obj->symbols[i].section = rdata;
        }
    }
    s->relocation_count = 0;
    s->data.size = 0;

    *iat_offset += offset;
    *id_offset += offset;
}

void rewrite_bss(Object* obj, section_ix* data,
                 uint32_t* uninitialized_size) {
    *data = 0;
    *uninitialized_size = 0;

    if (!Object_has_section(obj, SECTION_BSS)) {
        *data = SECTION_IX_NONE;
        return;
    }
    for (; *data < obj->section_count; ++(*data)) {
        if (obj->sections[*data].type == SECTION_DATA) {
            break;
        }
    }

    if (*data == obj->section_count) {
        *data = Object_create_section(obj, SECTION_DATA);
    }
    Section* s = &obj->sections[*data];

    uint32_t us = 0;
    for (section_ix i = 0; i < obj->section_count; ++i) {
        if (obj->sections[i].type == SECTION_BSS) {
            while (s->data.size + us % obj->sections[i].align != 0) {
                ++us;
            }
            for (symbol_ix sym = 0; sym < obj->symbol_count; ++sym) {
                if (obj->symbols[sym].section == i) {
                    obj->symbols[sym].section = *data;
                    obj->symbols[sym].offset += s->data.size + us;
                }
            }

            us += obj->sections[i].data.size;
            obj->sections[i].data.size = 0;
            assert(obj->sections[i].relocation_count == 0);
        }
    }
    *uninitialized_size = us;
}


void PeExectutable_create(Object* obj, ByteBuffer* dest, symbol_ix entrypoint) {

    uint32_t iat_offset, iat_size, id_offset, id_size;
    section_ix import_section = SECTION_IX_NONE;
    // Write .idata section to end of .rdata secton
    for (section_ix i = 0; i < obj->section_count; ++i) {
        if (obj->sections[i].type == SECTION_IDATA) {
            rewrite_idata(obj, i, &iat_offset, &iat_size, &id_offset, &id_size,
                          &import_section);
            break;
        }
    }

    section_ix data;
    uint32_t uninitialized_size;
    rewrite_bss(obj, &data, &uninitialized_size);

    String s;
    String_create(&s);
    Object_serialize(obj, &s);
    outputUtf8(s.buffer, s.length);
    oprintf("Import address table: %u, %u, Import directories: %u, %u\n",
            iat_offset, iat_size, id_offset, id_size);

    Coff64Image img;
    Coff64Image_create(&img);

    for (section_ix i = 0; i < obj->section_count; ++i) {
        Section* s = obj->sections + i;
        if (i == data) {
            if (s->data.size > 0 || uninitialized_size > 0) {
                Coff64Image_add_data_section(&img, s->data.data, s->data.size,
                                             s->data.size + uninitialized_size, 
                                             (const char*)s->name);
            }
            continue;
        }
        if (s->type == SECTION_CODE) {
            Coff64Image_add_code_section(&img, s->data.data, s->data.size, 
                                         (const char*)s->name);
        } else if (s->type == SECTION_RDATA) {
            Coff64Image_add_rdata_section(&img, s->data.data, s->data.size, 
                                          (const char*)s->name);
        } else if (s->type == SECTION_DATA) {
            if (s->data.size > 0) {
                Coff64Image_add_data_section(&img, s->data.data, s->data.size, 
                                             s->data.size, (const char*)s->name);
            }
        } else {
            assert(s->data.size == 0);
        }
    }

    uint32_t* rvas = Mem_alloc(obj->section_count);
    uint32_t img_ix = 0;
    if (rvas == NULL) {
        out_of_memory(NULL);
    }
    for (section_ix ix = 0; ix < obj->section_count; ++ix) {
        Section* s = obj->sections + ix;
        if (s->type == SECTION_BSS || s->type == SECTION_IDATA) {
            rvas[ix] = 0;
            continue;
        }
        if ((ix != data || uninitialized_size == 0) &&
             s->type == SECTION_DATA && s->data.size == 0) {
            rvas[ix] = 0;
            continue;
        }

        rvas[ix] = img.section_table[img_ix].virtual_address;
        ++img_ix;
    }

    bool found_data = false;
    bool found_code = false;
    for (section_ix i = 0; i < obj->section_count; ++i) {
        Section* s = obj->sections + i;
        if (s->type == SECTION_BSS || s->type == SECTION_IDATA) {
            continue;
        }
        if ((i != data || uninitialized_size == 0) && 
            s->type == SECTION_DATA && s->data.size == 0) {
            continue;
        }
        uint32_t rva = rvas[i];
        if (i == import_section) {
            struct DataDirectory* dir = img.optional_header->data_directories;
            dir[DIRECTORY_IMPORT_ADDRESS_TABLE].virtual_address_rva = 
                rva + iat_offset;
            dir[DIRECTORY_IMPORT_ADDRESS_TABLE].size = iat_size;
            dir[DIRECTORY_IMPORT_TABLE].virtual_address_rva = rva + id_offset;
            dir[DIRECTORY_IMPORT_TABLE].size = id_size;
        }


        for (uint32_t r = 0; r < s->relocation_count; ++r) {
            Relocation* reloc = &s->relocations[r];
            Symbol* sym = &obj->symbols[reloc->symbol];
            uint8_t* data = s->data.data + reloc->offset;

            assert(rvas[sym->section] != 0);

            uint32_t sym_rva = rvas[sym->section] + sym->offset;
            uint32_t reloc_rva = rva + reloc->offset;

            if (reloc->type == RELOCATE_ABS) {
                continue;
            } else if (reloc->type == RELOCATE_RVA32) {
                memcpy(data, &sym_rva, sizeof(uint32_t));
            } else if (reloc->type == RELOCATE_REL32) {
                int64_t src = reloc_rva + 4;
                int64_t dest = sym_rva;
                int64_t delta = dest - src;
                assert(delta >= INT32_MIN && delta <= INT32_MAX);
                int32_t delta32 = delta;
                memcpy(data, &delta32, sizeof(uint32_t));
            } else {
                assert(false);
            }
        }
    }

    Symbol* entry = &obj->symbols[entrypoint];
    uint32_t entry_rva = rvas[entry->section] + entry->offset;
    oprintf("Entry: %u\n", entry_rva);

    Coff64Image_set_entrypoint(&img, entry_rva);

    Coff64Image_write(&img, dest);
    Coff64Image_free(&img);
}
