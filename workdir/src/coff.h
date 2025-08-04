#ifndef COFF_STRUCT_H
#define COFF_STRUCT_H
#include <stdint.h>
#include <stdbool.h>
#include "ochar.h"

struct DataDirectory {
    uint32_t virtual_address_rva;
    uint32_t size;
};

typedef struct CoffHeader {
    uint16_t machine;
    uint16_t number_of_sections;
    uint32_t timedate_stamp;
    uint32_t pointer_to_symbol_table;
    uint32_t number_of_symbols;
    uint16_t size_of_optional_header;
    uint16_t characteristics;
} CoffHeader;

typedef struct CoffRelocation {
    uint32_t vritual_address;
    uint32_t symbol_table_index;
    uint16_t type;
} CoffRelocation;

typedef struct OptionalHeader32 {
    uint16_t magic;
    uint8_t major_linker_ver;
    uint8_t minor_linker_ver;
    uint32_t size_of_code;
    uint32_t size_of_initialized_data;
    uint32_t size_of_uninitialized_data;
    uint32_t address_of_entry_point;
    uint32_t base_of_code;
    uint32_t base_of_data;

    uint32_t image_base;
    uint32_t section_alignment;
    uint32_t file_alignment;
    uint16_t major_os_version;
    uint16_t minor_os_version;
    uint16_t major_image_version;
    uint16_t minor_image_version;
    uint16_t major_subsystem_version;
    uint16_t minor_subsystem_version;
    uint16_t win32_version;
    uint32_t size_of_image;
    uint32_t size_of_headers;
    uint32_t checksum;
    uint16_t subsystem;
    uint16_t dll_characteristics;
    uint32_t size_of_stack_reserve;
    uint32_t size_of_stack_commit;
    uint32_t size_of_heap_reserve;
    uint32_t size_of_heap_commit;
    uint32_t loader_flags;
    uint32_t number_of_rvas_and_sizes;
    struct DataDirectory data_directories[];
} OptionalHeader32;

typedef struct OptionalHeader64 {
    uint16_t magic;
    uint8_t major_linker_ver;
    uint8_t minor_linker_ver;
    uint32_t size_of_code;
    uint32_t size_of_initialized_data;
    uint32_t size_of_uninitialized_data;
    uint32_t address_of_entry_point;
    uint32_t base_of_code;

    uint64_t image_base;
    uint32_t section_alignment;
    uint32_t file_alignment;
    uint16_t major_os_version;
    uint16_t minor_os_version;
    uint16_t major_image_version;
    uint16_t minor_image_version;
    uint16_t major_subsystem_version;
    uint16_t minor_subsystem_version;
    uint16_t win32_version;
    uint32_t size_of_image;
    uint32_t size_of_headers;
    uint32_t checksum;
    uint16_t subsystem;
    uint16_t dll_characteristics;
    uint64_t size_of_stack_reserve;
    uint64_t size_of_stack_commit;
    uint64_t size_of_heap_reserve;
    uint64_t size_of_heap_commit;
    uint32_t loader_flags;
    uint32_t number_of_rvas_and_sizes;
    struct DataDirectory data_directories[];
} OptionalHeader64;

typedef struct SectionTableEntry {
    uint8_t name[8];
    uint32_t virtual_size;
    uint32_t virtual_address;
    uint32_t size_of_raw_data;
    uint32_t pointer_to_raw_data;
    uint32_t pointer_to_relocations;
    uint32_t pointer_to_line_numbers;
    uint16_t number_of_relocations;
    uint16_t number_of_line_numbers;
    uint32_t characteristics;
} SectionTableEntry;

typedef struct DebugEntry {
    uint32_t characteristics;
    uint32_t timedate_stamp;
    uint16_t major_version;
    uint16_t minor_version;
    uint32_t type;
    uint32_t size_of_data;
    uint32_t address_of_raw_data;
    uint32_t pointer_to_raw_data;
} DebugEntry;

typedef struct ExportDirectoryTable {
    uint32_t export_flags;
    uint32_t timedate_stamp;
    uint16_t major_version;
    uint16_t minor_version;
    uint32_t name_rva;
    uint32_t ordinal_base;
    uint32_t address_table_entries;
    uint32_t number_of_name_ptrs;
    uint32_t export_address_table_rva;
    uint32_t name_pointer_rva;
    uint32_t ordinal_table_rva;
} ExportDirectoryTable;

typedef struct StandardSymbol {
    uint8_t name[8];
    uint32_t value;
    uint16_t section_number;
    uint16_t type;
    uint8_t storage_class;
    uint8_t number_of_aux_symbols;
} StandardSymbol;

typedef union AuxillarySymbol {
    struct {
        uint32_t tag_index;
        uint32_t total_size;
        uint32_t pointer_to_line_number;
        uint32_t pointer_to_next_function;
        uint16_t unused;
    } function;
    struct {
        uint32_t tag_index;
        uint32_t characteristics;
        uint8_t unused[10];
    } weak_external;
    struct {
        uint8_t name[18];
    } files;
    struct {
        uint32_t length;
        uint16_t number_of_relocations;
        uint16_t number_of_line_numbers;
        uint32_t checksum;
        uint16_t number;
        uint8_t selection;
        uint8_t unused[3];
    } section_def;
    uint8_t unkown[18];
} AuxillarySymbol;

typedef struct SymbolTableEntry {
    union {
        StandardSymbol symbol;
        AuxillarySymbol aux;
    };
} SymbolTableEntry;

enum StorageClass {
    STORAGE_CLASS_EXTERNAL = 2,
    STORAGE_CLASS_STATIC = 3
};

enum Directory {
    DIRECTORY_EXPORT_TABLE = 0,
    DIRECTORY_IMPORT_TABLE = 1,
    DIRECTORY_RESOURCE_TABLE = 2,
    DIRECTORY_EXCEPTION_TABLE = 3,
    DIRECTORY_CERTIFICATE_TABLE = 4,
    DIRECTORY_BASE_RELOCATION_TABLE = 5,
    DIRECTORY_DEBUG = 6,
    DIRECTORY_ARCHITECTURE = 7,
    DIRECTORY_GLOBAL_PTR = 8,
    DIRECTORY_TLS_TABLE = 9,
    DIRECTORY_LOAD_CONFIG_TABLE = 10,
    DIRECTORY_BOUND_IMPORT_TABLE = 11,
    DIRECTORY_IMPORT_ADDRESS_TABLE = 12,
    DIRECTORY_DELAY_IMPORT_DESCRIPTOR = 13,
    DIRECTORY_CLR_RUNTIME_HEADER = 14,
    DIRECTORY_RESERVED = 15
};

#define IMAGE_REL_AMD64_ABSOLUTE 0x0000
#define IMAGE_REL_AMD64_ADDR64 0x0001
#define IMAGE_REL_AMD64_ADDR32NB 0x0003
#define IMAGE_REL_AMD64_REL32 0x0004


#define IMAGE_SCN_CNT_CODE 0x00000020
#define IMAGE_SCN_CNT_INITIALIZED_DATA 0x00000040
#define IMAGE_SCN_CNT_UNINITIALIZED_DATA 0x00000080
#define IMAGE_SCN_LNK_REMOVE 0x00000800

#define IMAGE_SCN_ALIGN_1BYTES 0x00100000
#define IMAGE_SCN_ALIGN_2BYTES 0x00200000
#define IMAGE_SCN_ALIGN_4BYTES 0x00300000
#define IMAGE_SCN_ALIGN_8BYTES 0x00400000
#define IMAGE_SCN_ALIGN_16BYTES 0x00500000
#define IMAGE_SCN_ALIGN_32BYTES 0x00600000
#define IMAGE_SCN_ALIGN_64BYTES 0x00700000
#define IMAGE_SCN_ALIGN_128BYTES 0x00800000
#define IMAGE_SCN_ALIGN_256BYTES 0x00900000
#define IMAGE_SCN_ALIGN_512BYTES 0x00A00000
#define IMAGE_SCN_ALIGN_1024BYTES 0x00B00000
#define IMAGE_SCN_ALIGN_2048BYTES 0x00C00000
#define IMAGE_SCN_ALIGN_4096BYTES 0x00D00000
#define IMAGE_SCN_ALIGN_8192BYTES 0x00E00000

#define SECTION_ALIGNMENT(flags) (((flags) & 0x00F00000) ? (1 << ((((flags) & 0x00F00000) >> 20) - 1)) : (1))

#define IMAGE_SCN_LNK_NRELOC_OVFL 0x01000000
#define IMAGE_SCN_MEM_DISCARDABLE 0x02000000
#define IMAGE_SCN_MEM_NOT_CACHED 0x04000000
#define IMAGE_SCN_MEM_NOT_PAGED 0x08000000
#define IMAGE_SCN_MEM_SHARED 0x10000000
#define IMAGE_SCN_MEM_EXECUTE 0x20000000
#define IMAGE_SCN_MEM_READ 0x40000000
#define IMAGE_SCN_MEM_WRITE 0x80000000

typedef struct ArchiveMemberHeader {
    uint8_t name[16];
    uint8_t date[12];
    uint8_t user_id[6];
    uint8_t group_id[6];
    uint8_t mode[8];
    uint8_t size[10];
    uint8_t end[2];
} ArchiveMemberHeader;

typedef struct ImportHeader {
    uint16_t sig1;
    uint16_t sig2;
    uint16_t version;
    uint16_t machine;
    uint32_t timedate_stamp;
    uint32_t size_of_data;
    uint16_t ordinal_or_hint;
    uint16_t type;
} ImportHeader;

typedef struct ImportDirectoryEntry {
    uint32_t import_lookup_table_rva;
    uint32_t timedate_stamp;
    uint32_t forwarder_chain;
    uint32_t name_rva;
    uint32_t import_address_table_rva;
} ImportDirectoryEntry;

typedef struct SectionData {
    const uint8_t* data;
    uint32_t size;
} SectionData;

struct ImportNode {
    uint32_t hash_link; // 0 for no value
    uint16_t name_len;
    uint16_t hint;
    uint32_t name_offset;
    uint32_t node_link; // 0 for no value
    bool is_dll;
    union {
        struct { // Used for symbols
            uint32_t module_id;
            uint32_t offset;
        };
        struct { // Used for dlls
            uint32_t first_sym; // 0 for no value
            uint32_t last_sym; // 0 for no value
        };
    };
};

typedef struct Import64Structure {
    uint32_t* hash_table; // 0 for no value
    struct ImportNode* nodes;
    uint32_t node_count;
    uint32_t node_cap;

    uint32_t first_module;
    uint32_t last_module;

    uint8_t* str_table;
    uint32_t str_table_count;
    uint32_t str_table_cap;

    uint32_t module_count;
    uint32_t entry_count;

    uint32_t str_length;
} Import64Structure;

typedef struct Coff64Image {
    CoffHeader header;
    OptionalHeader64* optional_header;
    SectionTableEntry* section_table;
    SectionData* section_data;
} Coff64Image;

bool Import64Structure_read_lib(Import64Structure* s, const ochar_t* filename);

bool Import64Structure_create(Import64Structure* s);

// Returns module id (1-based index), or 0 on error
uint32_t Import64Structure_add_module(Import64Structure* s, const uint8_t* name,
                                      uint32_t name_len);

// Returns symbol id (1-based index), or 0 on error
// mod_id should contain module id, will receve old module id if symbol existed
// with a differnt module
uint32_t Import64Structure_add_symbol(Import64Structure* s, uint32_t* mod_id,
                                     const uint8_t* sym, uint32_t sym_len, uint16_t hint);

uint32_t Import64Structure_get_size(Import64Structure* s);

void Import64Structure_write(Import64Structure* s, uint8_t* dest, uint32_t base_rva,
                             Coff64Image* img);

// Only valid after Import64Structure_write
uint32_t Import64Structure_get_sym_offset(Import64Structure* s, uint32_t sym_id);

uint8_t* Import64Structure_find_module(Import64Structure* s, const uint8_t* sym, uint32_t sym_len,
                                       uint32_t* mod_len, uint16_t* hint);

void Import64Structure_free(Import64Structure* s);

bool Coff64Image_create(Coff64Image* img);

bool Coff64Image_add_code_section(Coff64Image* img, const uint8_t* data,
                                  uint32_t size, const char* name);

bool Coff64Image_add_data_section(Coff64Image* img, const uint8_t* data,
                                  uint32_t size, uint32_t virtual_size,
                                  const char* name);

bool Coff64Image_add_rdata_section(Coff64Image* img, const uint8_t* data,
                                   uint32_t size, const char* name);

void Coff64Image_set_entrypoint(Coff64Image* img, uint32_t entry_offset);

bool Coff64Image_write(Coff64Image* img, const ochar_t* name);

void Coff64Image_free(Coff64Image* img);

enum SymbolFileType {
    SYMBOL_DUMP_NOT_FOUND = 0,
    SYMBOL_DUMP_INVALID = 1,
    SYMBOL_DUMP_ARCHIVE = 2,
    SYMBOL_DUMP_IMAGE = 3,
    SYMBOL_DUMP_OBJECT = 4,
};

char** symbol_dump(const ochar_t* filename, uint32_t* count, enum SymbolFileType* type);

#endif
