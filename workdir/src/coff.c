#include "mem.h"
#include "coff.h"
#include "args.h"
#include <stdbool.h>
#include <stdint.h>

#define READ_U32(ptr)                                                          \
    (((ptr)[0]) | ((ptr)[1] << 8) | ((ptr)[2] << 16) | ((ptr)[3] << 24))
#define READ_U16(ptr) (((ptr)[0]) | ((ptr)[1] << 8))
#define READ_U32_BE(ptr)                                                       \
    (((ptr)[3]) | ((ptr)[2] << 8) | ((ptr)[1] << 16) | ((ptr)[0] << 24))

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
uint64_t read_ascii_num(uint8_t *buf, uint32_t len) {
    wchar_t data[33];
    memset(data, 0, 33);
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
    parse_uintw(data, &res, 10);
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

char** symbol_dump(const wchar_t* filename, uint32_t* count, enum SymbolFileType* type) {
    HANDLE file = CreateFileW(filename, GENERIC_READ, FILE_SHARE_READ, NULL,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        *type = SYMBOL_DUMP_NOT_FOUND;
        return NULL;
    }
    *count = 0;
    *type = SYMBOL_DUMP_INVALID;
    if (is_library(file)) {
        *type = SYMBOL_DUMP_ARCHIVE;
        CloseHandle(file);
        return read_library_members(file, count);
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
