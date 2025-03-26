#ifndef COFF_STRUCT_H
#define COFF_STRUCT_H
#include <stdint.h>

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

typedef struct ArchiveMemberHeader {
    uint8_t name[16];
    uint8_t date[12];
    uint8_t user_id[6];
    uint8_t group_id[6];
    uint8_t mode[8];
    uint8_t size[10];
    uint8_t end[2];
} ArchiveMemberHeader;


enum SymbolFileType {
    SYMBOL_DUMP_NOT_FOUND = 0,
    SYMBOL_DUMP_INVALID = 1,
    SYMBOL_DUMP_ARCHIVE = 2,
    SYMBOL_DUMP_IMAGE = 3,
    SYMBOL_DUMP_OBJECT = 4,
};

char** symbol_dump(const wchar_t* filename, uint32_t* count, enum SymbolFileType* type);

#endif
