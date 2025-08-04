#ifndef COMPILER_LINKER_H_00
#define COMPILER_LINKER_H_00

#include "../utils.h"
#include <ochar.h>


enum SectionType {
    SECTION_CODE, SECTION_DATA, SECTION_RDATA, SECTION_BSS
};

enum RelocationType {
    RELOCATE_REL32, // Symbol adress relative byte following source
    RELOCATE_RVA32, // Symbol RVA
    RELOCATE_ADR64, // Symbol virtual address
    RELOCATE_SECTION_REL32, // Section adress relative byte following source
    RELOCATE_SECTION_RVA32, // Section RVA
    RELOCATE_SECTION_ADR64 // Section virtual address
};

typedef uint32_t symbol_ix;
#define SYMBOL_IX_NONE ((symbol_ix)-1)
typedef uint32_t section_ix;
#define SECTION_IX_NONE ((section_ix)-1)

typedef struct Relocation {
    enum RelocationType type;
    union {
        symbol_ix symbol;
        section_ix section;
    };
    uint32_t offset;
} Relocation;

typedef struct Section {
    enum SectionType type;
    ByteBuffer data;
    const uint8_t* name; //  Can be NULL

    uint32_t align;
    uint32_t relocation_cap;
    uint32_t relocation_count;
    Relocation* relocations;
} Section;

enum SymbolType {
    SYMBOL_STANDARD,
    SYMBOL_FUNCTION
};

typedef struct Symbol {
    const uint8_t* name;
    uint32_t name_len;
    section_ix section; // SECTION_IX_NONE means not defined in current object
    uint32_t offset;

    enum SymbolType type;
    // NOTE: In other places, dynamicaly imported functions are called extern
    // Here, it just means public symbol (IMAGE_SYM_CLASS_EXTERNAL)
    bool external;
} Symbol;

typedef struct Object {
    uint32_t section_count;
    uint32_t section_cap;
    Section* sections;

    uint32_t symbol_count;
    uint32_t symbol_cap;
    Symbol* symbols;
} Object;

void Object_create(Object* object);

void Object_free(Object* object);

section_ix Object_create_section(Object* object, enum SectionType type);

void Object_add_reloc(Object* object, section_ix section, symbol_ix symbol,
                      enum RelocationType type);

symbol_ix Object_reserve_var(Object* object, section_ix section,
                             const uint8_t* name, uint32_t name_len,
                             uint32_t size, uint32_t align, bool external);

symbol_ix Object_declare_var(Object* object, section_ix section,
                             const uint8_t* name, uint32_t name_len,
                             uint32_t align, bool external);

symbol_ix Object_declare_fn(Object* object, const uint8_t* name, uint32_t name_len,
                           section_ix section, bool external);

void Object_append_byte(Object* object, section_ix section, uint8_t byte);

void Object_append_data(Object* object, section_ix section, const uint8_t* data, uint32_t size);

void Linker_run(Object** objects, uint32_t object_count, const ochar_t** argv, uint32_t argc);

#endif
