#include "mem.h"
#include "coff.h"
#include "args.h"
#include "glob.h"
#include <stdbool.h>
#include <stdint.h>

#define SECTION_ALIGN 4096
#define FILE_ALIGNMENT 512

#define READ_U32(ptr)                                                          \
    (((ptr)[0]) | ((ptr)[1] << 8) | ((ptr)[2] << 16) | ((ptr)[3] << 24))
#define READ_U16(ptr) (((ptr)[0]) | ((ptr)[1] << 8))
#define READ_U32_BE(ptr)                                                       \
    (((ptr)[3]) | ((ptr)[2] << 8) | ((ptr)[1] << 16) | ((ptr)[0] << 24))


#define ALIGNED_TO(i, size) (((i) % (size) == 0) ? (i) : ((i) + ((size) - ((i) % (size)))))

const uint8_t DOS_STUB[] = {77, 90, 128, 0, 1, 0, 0, 0, 4, 0, 16, 0, 255, 255, 0, 0, 64, 1, 0, 0, 0, 0, 0, 0, 64, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 128, 0, 0, 0, 14, 31, 186, 14, 0, 180, 9, 205, 33, 184, 1, 76, 205, 33, 84, 104, 105, 115, 32, 112, 114, 111, 103, 114, 97, 109, 32, 99, 97, 110, 110, 111, 116, 32, 98, 101, 32, 114, 117, 110, 32, 105, 110, 32, 68, 79, 83, 32, 109, 111, 100, 101, 46, 13, 10, 36, 0, 0, 0, 0, 0, 0, 0, 0};

const uint8_t ZEROES[512] = {0};

bool set_file_offset(HANDLE file, uint64_t offset) {
    LARGE_INTEGER l;
    if (!GetFileSizeEx(file, &l)) {
        return false;
    }
    if (offset > l.QuadPart) {
        return false;
    }
    l.QuadPart = offset;
    return SetFilePointerEx(file, l, &l, FILE_BEGIN) && l.QuadPart == offset;
}

SectionTableEntry *read_section_table(HANDLE file, CoffHeader *coff_header) {
    uint16_t count = coff_header->number_of_sections;
    SectionTableEntry *section_table =
        Mem_alloc(count * sizeof(SectionTableEntry));

    DWORD r;
    if (!ReadFile(file, section_table, count * sizeof(SectionTableEntry), &r,
                  NULL)) {
        Mem_free(section_table);
        return NULL;
    }

    return section_table;
}

bool is_library(HANDLE file) {
    uint8_t buf[8];
    DWORD r;
    if (!ReadFile(file, buf, 8, &r, NULL) || r != 8) {
        return false;
    }
    return memcmp(buf, "!<arch>\n", 8) == 0;
}

bool read_pe_header(HANDLE file, CoffHeader *coff_header, void **opt_header,
                    bool *pe_plus) {
    uint8_t buf[0x3c + 4];
    DWORD r;
    bool image = false;
    if (!set_file_offset(file, 0)) {
        return false;
    }

    if (ReadFile(file, buf, 0x3c + 4, &r, NULL) && r >= 0x3c + 4) {
        uint32_t offset = READ_U32(buf + 0x3c);
        if (set_file_offset(file, offset)) {
            uint32_t signature;
            if (!ReadFile(file, &signature, 4, &r, NULL)) {
                return false;
            }
            if (r == 4 && signature == 17744) {
                image = true;
            }
        }
    }

    if (!image && !set_file_offset(file, 0)) {
        return false;
    }

    if (!ReadFile(file, coff_header, sizeof(CoffHeader), &r, NULL) ||
        r < sizeof(CoffHeader)) {
        return false;
    }
    if (coff_header->size_of_optional_header > 0) {
        uint8_t *h = Mem_alloc(coff_header->size_of_optional_header);
        if (h == NULL) {
            return false;
        }
        if (!ReadFile(file, h, coff_header->size_of_optional_header, &r,
                      NULL) ||
            r < coff_header->size_of_optional_header) {
            Mem_free(h);
            return false;
        }
        uint16_t magic = READ_U16(h);
        if (magic != 0x10B) {
            if (magic != 0x20B) {
                Mem_free(h);
                return false;
            }
            if (coff_header->size_of_optional_header < 112) {
                Mem_free(h);
                return false;
            }
            uint32_t dirs = ((OptionalHeader64*)h)->number_of_rvas_and_sizes;
            if (coff_header->size_of_optional_header < 112 + 8 * dirs) {
                Mem_free(h);
                return false;
            }
            *pe_plus = true;
        } else {
            if (coff_header->size_of_optional_header < 96) {
                Mem_free(h);
                return false;
            }
            uint32_t dirs = ((OptionalHeader32*)h)->number_of_rvas_and_sizes;
            if (coff_header->size_of_optional_header < 96 + 8 * dirs) {
                Mem_free(h);
                return false;
            }
            *pe_plus = false;
        }
        *opt_header = h;
    } else {
        *opt_header = NULL;
    }

    return true;
}

uint64_t rva_to_file_offset(uint64_t rva, CoffHeader *coff_header,
                            SectionTableEntry *section_table) {
    for (uint32_t ix = 0; ix < coff_header->number_of_sections; ++ix) {
        SectionTableEntry *section = section_table + ix;
        if (rva >= section->virtual_address &&
            rva < section->virtual_address + section->virtual_size) {
            return rva - section->virtual_address +
                   section->pointer_to_raw_data;
        }
    }
    return 0;
}

char** read_symbol_table(HANDLE file, CoffHeader *coff_header,
                       SectionTableEntry *section_table, uint32_t* count) {
    *count = 0;
    uint32_t symtab_offset = coff_header->pointer_to_symbol_table;
    uint32_t sym_count = coff_header->number_of_symbols;
    if (symtab_offset == 0 || sym_count == 0) {
        return Mem_alloc(1);
    }
    if (!set_file_offset(file, symtab_offset)) {
        return NULL;
    }
    uint32_t stringtab_offset = symtab_offset + 18 * sym_count;
    uint8_t *symbol_data = Mem_alloc(18 * sym_count);
    if (symbol_data == NULL) {
        return NULL;
    }
    uint32_t read = 0;
    DWORD r;
    while (read < 18 * sym_count) {
        if (!ReadFile(file, symbol_data + read, 18 * sym_count - read, &r, NULL) ||
            r == 0) {
            Mem_free(symbol_data);
            return NULL;
        }
        read += r;
    }
    uint8_t strtab_size_b[4];
    uint32_t strtab_size;
    if (!ReadFile(file, strtab_size_b, 4, &r, NULL) || r < 4) {
        Mem_free(symbol_data);
        return NULL;
    }
    strtab_size = READ_U32(strtab_size_b);
    if (strtab_size < 4) {
        Mem_free(symbol_data);
        return NULL;
    }
    strtab_size -= 4;
    uint8_t *str_data = Mem_alloc(strtab_size);
    if (str_data == NULL) {
        Mem_free(symbol_data);
        return NULL;
    }
    read = 0;
    while (read < strtab_size) {
        if (!ReadFile(file, str_data + read, strtab_size - read, &r, NULL) ||
            r == 0) {
            Mem_free(symbol_data);
            Mem_free(str_data);
            return NULL;
        }
        read += r;
    }

    uint8_t* syms_buf = Mem_alloc(sym_count * sizeof(char*) + 100);
    uint32_t cap = 100;
    uint32_t syms_size = 0;
    if (syms_buf == NULL) {
        Mem_free(symbol_data);
        Mem_free(str_data);
        return NULL;
    }
    char** syms = (char**)syms_buf;
    char* dest = (char*)(syms_buf + sym_count * sizeof(char*));
    uint32_t out_ix = 0;

    for (uint32_t ix = 0; ix < sym_count; ++ix) {
        StandardSymbol symbol;
        memcpy(&symbol, symbol_data + 18 * ix, 18);
        ix += symbol.number_of_aux_symbols;
        if (symbol.storage_class != STORAGE_CLASS_EXTERNAL) {
            continue;
        }
        uint64_t len;
        char* sym;

        if (READ_U32(symbol.name) == 0) {
            uint32_t offset = READ_U32(symbol.name + 4);
            if (offset >= 4 && offset - 4 < strtab_size) {
                offset -= 4;
                sym = (char *)(str_data + offset);
                uint64_t rem_size = strtab_size - offset;
                len = strnlen(sym, rem_size);
                if (len == rem_size) {
                    continue;
                }
            }
        } else {
            char name[9];
            name[8] = '\0';
            memcpy(name, symbol.name, 8);
            sym = name;
            len = strlen(sym);
        }
        while (syms_size + len + 1 > cap) {
            cap *= 2;
            syms_buf = Mem_realloc(syms_buf, sym_count * sizeof(char*) + cap);
            if (syms_buf == NULL) {
                Mem_free(symbol_data);
                Mem_free(str_data);
                Mem_free(syms);
                return NULL;
            }
            syms = (char**)syms_buf;
            dest = (char*)(syms_buf + sym_count * sizeof(char*));
        }
        memcpy(dest + syms_size, sym, (len + 1));
        syms[out_ix++] = (char*)(uintptr_t)syms_size;
        syms_size += len + 1;
    }

    Mem_free(symbol_data);
    Mem_free(str_data);
    *count = out_ix;
    for (uint32_t ix = 0; ix < *count; ++ix) {
        syms[ix] = dest + (uintptr_t)syms[ix];
    }

    return syms;
}

char** read_export_table(HANDLE file, uint64_t file_offset, uint32_t size,
                       CoffHeader *coff_header, SectionTableEntry *section_table, 
                       uint32_t* count) {
    *count = 0;
    if (file_offset == 0 || size == 0) {
        // Needed to tell difference between empty and failing to parse.
        // Mem_alloc(0) can legaly return NULL.
        return Mem_alloc(1);
    }
    if (!set_file_offset(file, file_offset)) {
        return NULL;
    }
    DWORD r;
    ExportDirectoryTable table;
    uint8_t *data = Mem_alloc(size);
    if (data == NULL) {
        return NULL;
    }
    uint32_t read = 0;
    while (read < size) {
        if (!ReadFile(file, data + read, size - read, &r, NULL)) {
            Mem_free(data);
            return NULL;
        }
        read += r;
    }
    memcpy(&table, data, sizeof(table));
    uint64_t name_offset =
        rva_to_file_offset(table.name_rva, coff_header, section_table);

    if (name_offset < file_offset || name_offset >= file_offset + size) {
        // Unsure if this can happen in well formated file
        Mem_free(data);
        return NULL;
    }
    char *name = (char *)(data + (name_offset - file_offset));
    uint32_t sym_count = table.number_of_name_ptrs;
    if (sym_count == 0) {
        Mem_free(data);
        return Mem_alloc(1);
    }
    uint64_t names_offset =
        rva_to_file_offset(table.name_pointer_rva, coff_header, section_table);
    if (names_offset < file_offset || names_offset >= file_offset + size) {
        Mem_free(data);
        return NULL;
    }
    names_offset -= file_offset;

    uint8_t* syms_buf = Mem_alloc(sym_count * sizeof(char*) + 100);
    uint32_t cap = 100;
    uint32_t syms_size = 0;
    if (syms_buf == NULL) {
        Mem_free(data);
        return NULL;
    }
    char** syms = (char**)syms_buf;
    char* dest = (char*)(syms_buf + sym_count * sizeof(char*));
    uint32_t out_ix = 0;

    for (uint32_t ix = 0; ix < sym_count; ++ix) {
        if (names_offset + ix * 4 + 4 > size) {
            break;
        }
        uint32_t rva = READ_U32(data + names_offset + ix * 4);
        uint64_t offset = rva_to_file_offset(rva, coff_header, section_table);
        if (offset < file_offset || offset >= file_offset + size) {
            continue;
        }
        char *symbol = (char *)(data + (offset - file_offset));
        uint64_t rem_size = size - (offset - file_offset);
        uint64_t len = strnlen(symbol, rem_size);
        if (len == rem_size) {
            continue;
        }
        while (syms_size + len + 1 > cap) {
            cap *= 2;
            syms_buf = Mem_realloc(syms_buf, sym_count * sizeof(char*) + cap);
            if (syms_buf == NULL) {
                Mem_free(data);
                Mem_free(syms);
                return NULL;
            }
            syms = (char**)syms_buf;
            dest = (char*)(syms_buf + sym_count * sizeof(char*));
        }
        memcpy(dest + syms_size, symbol, (len + 1));
        syms[out_ix++] = (char*)(uintptr_t)syms_size;
        syms_size += len + 1;
    }
    *count = out_ix;
    for (uint32_t ix = 0; ix < *count; ++ix) {
        syms[ix] = dest + (uintptr_t)(syms[ix]);
    }

    Mem_free(data);
    return syms;
}

// Note: len <= 32
static uint64_t read_ascii_num(uint8_t *buf, uint32_t len) {
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

char** read_library_members(HANDLE file, uint32_t* count) {
    ArchiveMemberHeader header;
    DWORD r;
    *count = 0;
    if (!ReadFile(file, &header, 60, &r, NULL) || r != 60) {
        return NULL;
    }

    if (header.name[0] != '/' || header.name[1] != ' ') {
        // No first linker member
        return NULL;
    }
    uint64_t size = read_ascii_num(header.size, 10);
    uint8_t *buf = Mem_alloc(size);
    if (buf == NULL) {
        return NULL;
    }
    if (!ReadFile(file, buf, size, &r, NULL) || r != size) {
        Mem_free(buf);
        return NULL;
    }

    if (size % 2 != 0) {
        uint8_t b;
        if (!ReadFile(file, &b, 1, &r, NULL) || r != 1) {
            return NULL;
        }
    }
    if (!ReadFile(file, &header, 60, &r, NULL) || r != 60) {
        return NULL;
    }

    if (header.name[0] != '/' || header.name[1] != ' ') {
        // No second linker member
        uint32_t no_symbols = READ_U32_BE(buf);
        uint64_t offset = 4 + no_symbols * 4;
        
        uint8_t* syms_buf = Mem_alloc(no_symbols * sizeof(char*) + 100);
        if (syms_buf == NULL) {
            Mem_free(buf);
            return NULL;
        }
        uint32_t cap = 100;
        uint32_t syms_size = 0;
        char** syms = (char**)syms_buf;
        char* dest = (char*)(syms_buf + no_symbols * sizeof(char*));

        for (uint32_t ix = 0; ix < no_symbols; ++ix) {
            if (offset >= size) {
                break;
            }
            uint64_t rem_size = size - offset;
            char *s = (char *)(buf + offset);
            uint64_t len = strnlen(s, rem_size);
            if (len == rem_size) {
                break;
            }
            while (syms_size + len + 1 > cap) {
                cap *= 2;
                syms_buf = Mem_realloc(syms_buf, no_symbols * sizeof(char*) + cap);
                if (syms_buf == NULL) {
                    Mem_free(syms);
                    Mem_free(buf);
                    return NULL;
                }
                syms = (char**)syms_buf;
                dest = (char*)(syms_buf + no_symbols * sizeof(char*));
            }
            syms[ix] = (char*)(uintptr_t)syms_size;
            memcpy(dest + syms_size, s, (len + 1));
            offset += len + 1;
            syms_size += len + 1;
            ++(*count);
        }
        for (uint32_t ix = 0; ix < *count; ++ix) {
            syms[ix] = dest + (uintptr_t)syms[ix];
        }
        Mem_free(buf);
        return syms;
    }

    size = read_ascii_num(header.size, 10);
    uint8_t *new_buf = Mem_realloc(buf, size);
    if (new_buf == NULL) {
        Mem_free(buf);
        return NULL;
    }
    buf = new_buf;
    if (!ReadFile(file, buf, size, &r, NULL) || r != size) {
        Mem_free(buf);
        return NULL;
    }
    uint32_t no_members = READ_U32(buf);
    uint64_t offset = 4 + no_members * 4;
    if (offset >= size) {
        Mem_free(buf);
        return NULL;
    }
    uint32_t no_symbols = READ_U32(buf + offset);
    offset = offset + 4 + no_symbols * 2;

    uint8_t* syms_buf = Mem_alloc(no_symbols * sizeof(char*) + 100);
    if (syms_buf == NULL) {
        Mem_free(buf);
        return NULL;
    }
    uint32_t cap = 100;
    uint32_t syms_size = 0;
    char** syms = (char**)syms_buf;
    char* dest = (char*)(syms_buf + no_symbols * sizeof(char*));

    for (uint32_t ix = 0; ix < no_symbols; ++ix) {
        if (offset >= size) {
            break;
        }
        uint64_t rem_size = size - offset;
        char *s = (char *)(buf + offset);
        uint64_t len = strnlen(s, rem_size);
        if (len == rem_size) {
            break;
        }
        while (syms_size + len + 1 > cap) {
            cap *= 2;
            syms_buf = Mem_realloc(syms_buf, no_symbols * sizeof(char*) + cap);
            if (syms_buf == NULL) {
                Mem_free(syms);
                Mem_free(buf);
                return NULL;
            }
            syms = (char**)syms_buf;
            dest = (char*)(syms_buf + no_symbols * sizeof(char*));
        }
        syms[ix] = (char*)(uintptr_t)syms_size;
        memcpy(dest + syms_size, s, (len + 1));
        offset += len + 1;
        syms_size += len + 1;
        ++(*count);
    }
    for (uint32_t ix = 0; ix < *count; ++ix) {
        syms[ix] = dest + (uintptr_t)syms[ix];
    }
    Mem_free(buf);
    return syms;
}

char** symbol_dump(const ochar_t* filename, uint32_t* count, enum SymbolFileType* type) {
    HANDLE file = open_file_read(filename);
    if (file == INVALID_HANDLE_VALUE) {
        *type = SYMBOL_DUMP_NOT_FOUND;
        return NULL;
    }
    *count = 0;
    *type = SYMBOL_DUMP_INVALID;
    if (is_library(file)) {
        *type = SYMBOL_DUMP_ARCHIVE;
        char** res = read_library_members(file, count);
        CloseHandle(file);
        return res;
    }

    CoffHeader coff_header;
    bool pe_plus;
    void *header;
    if (!read_pe_header(file, &coff_header, &header, &pe_plus)) {
        CloseHandle(file);
        return NULL;
    }

    SectionTableEntry *section_table = read_section_table(file, &coff_header);
    if (header != NULL && section_table != NULL) {
        *type = SYMBOL_DUMP_IMAGE;
        uint32_t directory_count = 0;
        struct DataDirectory *dirs = NULL;
        if (pe_plus) {
            OptionalHeader64 *opt_header = (OptionalHeader64 *)header;
            directory_count = opt_header->number_of_rvas_and_sizes;
            dirs = opt_header->data_directories;
        } else {
            OptionalHeader32 *opt_header = (OptionalHeader32 *)header;
            directory_count = opt_header->number_of_rvas_and_sizes;
            dirs = opt_header->data_directories;
        }

        if (dirs != NULL && directory_count > DIRECTORY_EXPORT_TABLE) {
            uint64_t rva = dirs[DIRECTORY_EXPORT_TABLE].virtual_address_rva;
            uint64_t addr =
                rva_to_file_offset(rva, &coff_header, section_table);
            uint32_t size = dirs[DIRECTORY_EXPORT_TABLE].size;
            if (addr != 0 && size != 0) { 
                char** syms = read_export_table(file, addr, size, &coff_header,
                                                section_table, count);
                Mem_free(header);
                Mem_free(section_table);
                CloseHandle(file);
                return syms;
            }
        }
    }
    if (section_table != NULL) {
        if (*type == SYMBOL_DUMP_INVALID) {
            *type = SYMBOL_DUMP_OBJECT;
        }
        char** syms = read_symbol_table(file, &coff_header, section_table, count);
        if (header != NULL) {
            Mem_free(header);
        }
        Mem_free(section_table);
        CloseHandle(file);
        return syms;
    }

    if (header != NULL) {
        Mem_free(header);
    }
    if (section_table != NULL) {
        Mem_free(section_table);
    }
    *count = 0;
    CloseHandle(file);
    return Mem_alloc(1);
}

#define IMPORT_HASHTABLE_SIZE 1024

static uint32_t hash(const uint8_t* str, uint32_t len) {
    uint64_t hash = 5381;
    int c;
    for (uint32_t ix = 0; ix < len; ++ix) {
        c = str[ix];
        hash = ((hash << 5) + hash) + c;
    }
    return hash % IMPORT_HASHTABLE_SIZE;
}

bool Import64Structure_create(Import64Structure* s) {
    s->hash_table = Mem_alloc(IMPORT_HASHTABLE_SIZE * sizeof(uint32_t));
    if (s->hash_table == NULL) {
        return false;
    }
    memset(s->hash_table, 0, IMPORT_HASHTABLE_SIZE * sizeof(uint32_t));
    s->node_count = 0;
    s->node_cap = 64;
    s->nodes = Mem_alloc(64 * sizeof(struct ImportNode));
    if (s->nodes == NULL) {
        Mem_free(s->hash_table);
        return false;
    }
    s->str_table_count = 0;
    s->str_table_cap = 64;
    s->str_table = Mem_alloc(64);
    if (s->str_table == NULL) {
        Mem_free(s->hash_table);
        Mem_free(s->nodes);
        return false;
    }

    s->module_count = 0;
    s->entry_count = 0;
    s->first_module = 0;
    s->last_module = 0;

    s->str_length = 0;
    return true;
}

struct ImportNode* allocate_node(Import64Structure* s, const uint8_t* name, uint32_t name_len) {
    if (name_len >= UINT16_MAX) {
        return false;
    }
    if (s->node_count == s->node_cap) {
        struct ImportNode* ptr = Mem_realloc(s->nodes, s->node_cap * 2 * 
                sizeof(struct ImportNode));
        if (ptr == NULL) {
            return NULL;
        }
        s->nodes = ptr;
        s->node_cap *= 2;
    }
    while (s->str_table_count + name_len > s->str_table_cap) {
        uint8_t* ptr = Mem_realloc(s->str_table, s->str_table_cap * 2);
        if (ptr == NULL) {
            return NULL;
        }
        s->str_table = ptr;
        s->str_table_cap *= 2;
    }
    uint8_t* str = s->str_table + s->str_table_count;
    memcpy(str, name, name_len);
    s->str_table_count += name_len;

    struct ImportNode* node = s->nodes + s->node_count;
    ++s->node_count;
    node->name_offset = str - s->str_table;
    node->name_len = name_len;
    node->hash_link = 0;
    node->node_link = 0;
    node->is_dll = false;
    node->hint = 0;
    node->offset = 0;
    node->module_id = 0;

    return node;
}

uint32_t Import64Structure_add_module(Import64Structure* s, const uint8_t* name,
                                      uint32_t name_len) {
    uint32_t h = hash(name, name_len);
    uint32_t ix = s->hash_table[h];
    if (ix != 0) {
        uint32_t n_ix = ix - 1;
        while (1) {
            if (s->nodes[n_ix].is_dll && s->nodes[n_ix].name_len == name_len &&
                memcmp(s->str_table + s->nodes[n_ix].name_offset, name, name_len) == 0) {
                // Module already present
                return n_ix + 1;
            }
            if (s->nodes[n_ix].hash_link == 0) {
                break;
            }
            n_ix = s->nodes[n_ix].hash_link - 1;
        }
    }
    struct ImportNode* node = allocate_node(s, name, name_len);
    if (node == NULL) {
        return 0;
    }
    uint32_t n_ix = node - s->nodes;
    node->hash_link = ix;
    s->hash_table[h] = n_ix + 1;

    node->is_dll = true;
    node->first_sym = 0;
    node->last_sym = 0;
    if (s->first_module == 0) {
        s->first_module = n_ix + 1;
    } else {
        s->nodes[s->last_module - 1].node_link = n_ix + 1;
    }
    s->last_module = n_ix + 1;

    s->module_count += 1;
    s->str_length += name_len + 1;
    if ((name_len + 1) % 2 != 0) {
        s->str_length += 1;
    }
    return n_ix + 1;
}

uint32_t Import64Structure_add_symbol(Import64Structure* s, uint32_t* mod_id,
                                      const uint8_t* sym, uint32_t sym_len, uint16_t hint) {
    if (*mod_id == 0) {
        return 0;
    }
    uint32_t h = hash(sym, sym_len);
    uint32_t ix = s->hash_table[h];
    if (ix != 0) {
        uint32_t n_ix = ix - 1;
        while (1) {
            if (!s->nodes[n_ix].is_dll && s->nodes[n_ix].name_len == sym_len &&
                memcmp(s->str_table + s->nodes[n_ix].name_offset, sym, sym_len) == 0) {
                // Symbol already present
                *mod_id = s->nodes[n_ix].module_id;
                return n_ix + 1;
            }
            if (s->nodes[n_ix].hash_link == 0) {
                break;
            }
            n_ix = s->nodes[n_ix].hash_link - 1;
        }
    }
    struct ImportNode* node = allocate_node(s, sym, sym_len);
    if (node == NULL) {
        return 0;
    }
    uint32_t n_ix = node - s->nodes;
    node->hash_link = ix;
    s->hash_table[h] = n_ix + 1;

    node->is_dll = false;
    node->module_id = *mod_id;
    node->hint = hint;

    if (s->nodes[node->module_id - 1].first_sym == 0) {
        s->nodes[node->module_id - 1].first_sym = n_ix + 1;
    } else {
        uint32_t last = s->nodes[node->module_id - 1].last_sym;
        s->nodes[last].node_link = n_ix + 1;
    }
    s->nodes[node->module_id - 1].last_sym = n_ix + 1;

    s->str_length += sym_len + 3;
    if ((sym_len + 3) % 2 != 0) {
        s->str_length += 1;
    }
    s->entry_count += 1;
    return true;
}

uint32_t Import64Structure_get_size(Import64Structure* s) {
    uint32_t ilt_length = (s->entry_count + s->module_count) * sizeof(uint64_t);
    uint32_t idt_length = (s->module_count + 1) * sizeof(ImportDirectoryEntry);
    idt_length = ALIGNED_TO(idt_length, 8);

    return 2 * ilt_length + idt_length + s->str_length;
}

void Import64Structure_write(Import64Structure* s, uint8_t* dest, uint32_t base_rva,
                             Coff64Image* img) {
    uint32_t ilt_length = (s->entry_count + s->module_count) * sizeof(uint64_t);
    img->optional_header->
        data_directories[DIRECTORY_IMPORT_ADDRESS_TABLE].virtual_address_rva = base_rva;
    img->optional_header->
        data_directories[DIRECTORY_IMPORT_ADDRESS_TABLE].size = ilt_length;

    uint32_t idt_length = (s->module_count + 1) * sizeof(ImportDirectoryEntry);

    img->optional_header->data_directories[DIRECTORY_IMPORT_TABLE].virtual_address_rva = 
        base_rva + ilt_length;
    img->optional_header->data_directories[DIRECTORY_IMPORT_TABLE].size = idt_length;
    idt_length = ALIGNED_TO(idt_length, 8);

    uint32_t table_offset = 0;
    uint32_t str_table = ilt_length * 2 + idt_length;

    uint32_t module_id = s->first_module;
    uint64_t i = 0;
    while (module_id != 0) {
        ImportDirectoryEntry e;
        e.import_lookup_table_rva = base_rva + idt_length + ilt_length + table_offset;
        e.import_address_table_rva = base_rva + table_offset;
        e.timedate_stamp = 0;
        e.forwarder_chain = 0;

        uint32_t node_id = s->nodes[module_id - 1].first_sym;
        uint64_t j = 0;
        while (node_id != 0) {
            uint8_t* name = s->str_table + s->nodes[node_id - 1].name_offset;
            uint32_t name_len = s->nodes[node_id - 1].name_len;
            memcpy(dest + str_table, &s->nodes[node_id - 1].hint, sizeof(uint16_t));
            memcpy(dest + str_table + 2, name, name_len);
            dest[str_table + name_len + 2] = '\0';
            uint64_t lookup_entry = base_rva + str_table;
            memcpy(dest + table_offset, &lookup_entry, sizeof(uint64_t));
            memcpy(dest + table_offset + idt_length + ilt_length, &lookup_entry,
                   sizeof(uint64_t));
            table_offset += sizeof(uint64_t);

            if ((name_len + 3) % 2 != 0) {
                dest[str_table + name_len + 3] = '\0';
                ++str_table;
            }
            str_table += name_len + 3;
            s->nodes[node_id - 1].offset = (i + j) * sizeof(uint64_t);
            node_id = s->nodes[node_id - 1].node_link;
            ++j;
        }
        e.name_rva = base_rva + str_table;
        uint8_t* modname = s->str_table + s->nodes[module_id - 1].name_offset;
        uint32_t modname_len = s->nodes[module_id - 1].name_len;

        memcpy(dest + str_table, modname, modname_len);
        dest[str_table + modname_len] = '\0';

        if ((modname_len + 1) % 2 != 0) {
            dest[str_table + modname_len + 1] = '\0';
            ++str_table;
        }
        str_table += modname_len + 1;
        memset(dest + table_offset, 0, sizeof(uint64_t));
        memset(dest + table_offset + idt_length + ilt_length, 0, sizeof(uint64_t));
        table_offset += sizeof(uint64_t);
        memcpy(dest + ilt_length + i * sizeof(ImportDirectoryEntry), &e,
               sizeof(ImportDirectoryEntry));
        ++i;
        module_id = s->nodes[module_id - 1].node_link;
    }
    memset(dest + ilt_length + s->module_count * sizeof(ImportDirectoryEntry), 0,
            sizeof(ImportDirectoryEntry));
    dest[ilt_length + idt_length - 1] = 0;
    dest[ilt_length + idt_length - 2] = 0;
    dest[ilt_length + idt_length - 3] = 0;
    dest[ilt_length + idt_length - 4] = 0;
}

uint32_t Import64Structure_get_sym_offset(Import64Structure* s, uint32_t sym_id) {
    return s->nodes[sym_id - 1].offset;
}

uint8_t* Import64Structure_find_module(Import64Structure* s, const uint8_t* sym, 
                                       uint32_t sym_len, uint32_t* mod_len,
                                       uint16_t* hint) {

    uint32_t h = hash(sym, sym_len);
    uint32_t ix = s->hash_table[h];
    while (ix) {
        uint32_t n_ix = ix - 1;
        if (s->nodes[n_ix].is_dll && s->nodes[n_ix].name_len == sym_len &&
            memcmp(s->str_table + s->nodes[n_ix].name_offset, sym, sym_len) == 0) {
            uint32_t mod_id = s->nodes[n_ix].module_id;
            *mod_len = s->nodes[mod_id - 1].name_len;
            *hint = s->nodes[n_ix].hint;
            return s->str_table + s->nodes[mod_id - 1].name_offset;
        }
        ix = s->nodes[n_ix].hash_link;
    }
    return NULL;

}

void Import64Structure_free(Import64Structure* s) {
    Mem_free(s->nodes);
    Mem_free(s->hash_table);
    Mem_free(s->str_table);
}


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
    e->characteristics = 0x00000020 | 0x40000000 | 0x20000000;
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
    e->characteristics = 0x00000040 | 0x40000000 | 0x80000000;
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
    e->characteristics = 0x00000040 | 0x40000000;
    img->optional_header->size_of_initialized_data += aligned_size;

    return true;
}

void Coff64Image_set_entrypoint(Coff64Image* img, uint32_t entry_offset) {
    if (img->header.number_of_sections > 0) {
        entry_offset += img->section_table[0].virtual_address;
    }
    img->optional_header->address_of_entry_point = entry_offset;
}

bool Coff64Image_write(Coff64Image* img, const ochar_t* name) {
    HANDLE file = open_file_write(name);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }

    DWORD w;
    if (!WriteFile(file, DOS_STUB, sizeof(DOS_STUB), &w, NULL) || w != sizeof(DOS_STUB)) {
        CloseHandle(file);
        return false;
    }
    uint8_t signature[4] = {'P', 'E', 0, 0};
    if (!WriteFile(file, signature, 4, &w, NULL) || w != 4) {
        CloseHandle(file);
        return false;
    }
    if (!WriteFile(file, &img->header, sizeof(CoffHeader), &w, NULL) || w != sizeof(CoffHeader)) {
        CloseHandle(file);
        return false;
    }
    uint32_t opt_size = sizeof(OptionalHeader64) + 
        img->optional_header->number_of_rvas_and_sizes * sizeof(struct DataDirectory);
    if (!WriteFile(file, img->optional_header, opt_size, &w, NULL) || w != opt_size) {
        CloseHandle(file);
        return false;
    }

    for (uint32_t i = 0; i < img->header.number_of_sections; ++i) {
        if (!WriteFile(file, &img->section_table[i], sizeof(SectionTableEntry), &w, NULL) || 
             w != sizeof(SectionTableEntry)) {
            CloseHandle(file);
            return false;
        }
    }

    uint64_t file_offset = sizeof(DOS_STUB) + 4 + sizeof(CoffHeader) + opt_size + 
        img->header.number_of_sections * sizeof(SectionTableEntry);

    for (uint32_t i = 0; i < img->header.number_of_sections; ++i) {
        while (file_offset < img->section_table[i].pointer_to_raw_data) {
            uint32_t to_write = 512;
            if (img->section_table[i].pointer_to_raw_data - file_offset < 512) {
                to_write = img->section_table[i].pointer_to_raw_data - file_offset;
            }
            if (!WriteFile(file, ZEROES, to_write, &w, NULL)) {
                CloseHandle(file);
                return false;
            }
            file_offset += w;
        }
        uint32_t written = 0;
        while (written < img->section_data[i].size) {
            if (!WriteFile(file, img->section_data[i].data + written,
                  img->section_data[i].size - written, &w, NULL)) {
                CloseHandle(file);
                return false;
            }
            written += w;
        }
        file_offset += written;
    }

    uint64_t remaining = ALIGNED_TO(file_offset, FILE_ALIGNMENT) - file_offset;
    while (remaining > 0) {
        uint32_t to_write = 512;
        if (remaining < to_write) {
            to_write = remaining;
        }
        if (!WriteFile(file, ZEROES, to_write, &w, NULL)) {
            CloseHandle(file);
            return false;
        }
        remaining -= w;
    }

    return true;
}

void Coff64Image_free(Coff64Image* img) {
    if (img->header.number_of_sections > 0) {
        Mem_free(img->section_data);
        Mem_free(img->section_table);
    }
    Mem_free(img->optional_header);
}

enum ImportSymbolStatus {
    IMPORTSYMSTATUS_OK,
    IMPORTSYMSTATUS_OUTOFMEM,
    IMPORTSYMSTATUS_INVALID,
    IMPORTSYMSTATUS_DUPLICATE
};

// Parse short form import archive member.
enum ImportSymbolStatus parse_short_import(Import64Structure* s, 
        ImportHeader* header, uint8_t* buf) {
    uint8_t* sym = buf;
    uint32_t sym_len = 0;

    uint16_t hint = 0;
    uint32_t name_type = ((header->type) >> 2) & 0x7;
    if (name_type != 0) {
        hint = header->ordinal_or_hint;
    }

    while (sym_len < header->size_of_data && sym[sym_len] != '\0') {
        ++sym_len;
    }
    uint8_t* dll = buf + sym_len + 1;
    uint32_t dll_len = 0;
    while (dll_len + sym_len + 1 < header->size_of_data && dll[dll_len] != '\0') {
        ++dll_len;
    }
    if (sym_len == 0 || dll_len == 0) {
        return IMPORTSYMSTATUS_INVALID;
    }

    uint32_t module = Import64Structure_add_module(s, dll, dll_len);
    if (!module) {
        return IMPORTSYMSTATUS_OUTOFMEM;;
    }

    uint32_t mod_id = module;
    if (!Import64Structure_add_symbol(s, &mod_id, sym, sym_len, hint)) {
        return IMPORTSYMSTATUS_OUTOFMEM;;
    }
    if (mod_id != module) {
        return IMPORTSYMSTATUS_DUPLICATE;
    }
    return IMPORTSYMSTATUS_OK;
}

bool Import64Structure_read_lib(Import64Structure* s, const ochar_t* filename) {
    HANDLE file = open_file_read(filename);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }
    if (!is_library(file)) {
        CloseHandle(file);
        return false;
    }

    LARGE_INTEGER filesize;
    if (!GetFileSizeEx(file, &filesize)) {
        CloseHandle(file);
        return false;
    }

    ArchiveMemberHeader header;
    DWORD r;

    uint64_t offset = 8;
    
    uint64_t cap = 2048;
    uint8_t* buf = Mem_alloc(cap);
    if (buf == NULL) {
        CloseHandle(file);
        return false;
    }

    while (1) {
        if (!ReadFile(file, &header, 60, &r, NULL) || r != 60) {
            goto fail;
        }
        offset += sizeof(ArchiveMemberHeader);
        uint64_t size = read_ascii_num(header.size, 10);

        if (size > INT32_MAX || offset + size > filesize.QuadPart) {
            goto fail;
        }
        if (header.end[0] != 0x60 || header.end[1] != 0xA) {
            goto fail;
        }

        bool special = header.name[0] == '/' &&
            (header.name[1] == ' ' || header.name[1] == '/');

        if (special || size < sizeof(ImportHeader)) {
            // Skip
            if (size % 2 != 0) {
                ++size;
            }
            offset += size;
            if (!set_file_offset(file, offset)) {
                goto fail;
            }
            continue;
        }

        if (size > cap) {
            uint8_t* ptr = Mem_alloc(cap);
            if (ptr == NULL) {
                goto fail;
            }
            size = cap;
            buf = ptr;
        }
        uint64_t read = 0;
        while (read < size) {
            if (!ReadFile(file, buf + read, size - read, &r, NULL)) {
                goto fail;
            }
            read += r;
        }

        ImportHeader* h = (ImportHeader*)buf;
        if (h->sig1 == 0x0 && h->sig2 == 0xffff &&
            h->size_of_data <= size - sizeof(ImportHeader)) {
            if (parse_short_import(s, h, buf + sizeof(ImportHeader)) < 0) {
                goto fail;
            }
        }

        offset += size;
        if (offset == filesize.QuadPart || offset + 1 == filesize.QuadPart) {
            CloseHandle(file);
            Mem_free(buf);

            return true;
        }
        if (size % 2 != 0) {
            ++offset;
            if (!ReadFile(file, buf, 1, &r, NULL) || r != 1) {
                goto fail;
            }
        }
    }
fail:
    Mem_free(buf);
    CloseHandle(file);
    return false;
}
