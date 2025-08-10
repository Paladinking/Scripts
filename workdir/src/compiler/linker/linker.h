#ifndef COMPILER_LINKER_H_00
#define COMPILER_LINKER_H_00

#include "../utils.h"
#include <ochar.h>
#include <dynamic_string.h>


enum SectionType {
    SECTION_CODE, SECTION_DATA, SECTION_RDATA, SECTION_BSS,
    SECTION_IDATA,
    SECTION_INVALID
};

enum RelocationType {
    RELOCATE_ABS,   // Ignored
    RELOCATE_REL32, // Symbol adress relative byte following source
    RELOCATE_RVA32, // Symbol RVA
    RELOCATE_ADR64 // Symbol virtual address
};

typedef uint32_t symbol_ix;
#define SYMBOL_IX_NONE ((symbol_ix)-1)
typedef uint32_t section_ix;
#define SECTION_IX_NONE ((section_ix)-1)

typedef struct Relocation {
    enum RelocationType type;
    symbol_ix symbol;
    uint32_t offset;
} Relocation;

typedef struct Section {
    enum SectionType type;
    ByteBuffer data;
    const uint8_t* name;

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
    // True if this symbol always refers to the start of section,
    // even after merging.
    bool section_relative;
} Symbol;

typedef struct Object {
    uint32_t section_count;
    uint32_t section_cap;
    Section* sections;

    uint32_t symbol_count;
    uint32_t symbol_cap;
    Symbol* symbols;

    symbol_ix* map;
    uint32_t map_size;
} Object;

// Realy ObjectList...
typedef struct ObjectSet {
    Object** objects;
    uint32_t object_count;
    uint32_t object_cap;
} ObjectSet;

typedef struct Library {
    Object* dest;
    ObjectSet objects;
} Library;

typedef struct SymbolQueue {
    uint64_t head;
    uint64_t tail;

    uint64_t capacity;
    StrWithLength* symbols;
} SymbolQueue;

void Libary_create(Library* lib);

bool Library_find_symbol(Library* lib, const uint8_t* sym, uint32_t len,
                         SymbolQueue* queue);

void Library_free(Library* lib);

void SymbolQueue_create(SymbolQueue* queue);

void SymbolQueue_free(SymbolQueue* queue);

void SymbolQueue_append(SymbolQueue* queue, const uint8_t* sym, uint32_t len);

uint64_t SymbolQueue_size(SymbolQueue* queue);

StrWithLength SymbolQueue_get(SymbolQueue* queue);

void ObjectSet_create(ObjectSet* set);

void ObjectSet_free(ObjectSet* set);

void ObjectSet_add(ObjectSet* set, Object* obj);

void ObjectSet_remove(ObjectSet* set, uint32_t ix);

void Object_create(Object* object);

void Object_free(Object* object);

section_ix Object_create_section(Object* object, enum SectionType type);

section_ix Object_create_named_section(Object* object, enum SectionType type,
                                           uint8_t* name);

// Add the relocation <reloc> to section <section>.
// <section> must not be SECTION_IX_NONE
void Object_add_relocation(Object* object, section_ix section, 
                           Relocation reloc);

symbol_ix Object_add_symbol(Object* object, section_ix section, const uint8_t* name,
                            uint32_t name_len, bool external, enum SymbolType type,
                            uint32_t offset);

// Add a symbol relocation of type <type> to current offset into <section>
void Object_add_reloc(Object* object, section_ix section, symbol_ix symbol,
                      enum RelocationType type);

symbol_ix Object_reserve_var(Object* object, section_ix section,
                             const uint8_t* name, uint32_t name_len,
                             uint32_t size, uint32_t align, bool external);

symbol_ix Object_declare_var(Object* object, section_ix section,
                             const uint8_t* name, uint32_t name_len,
                             uint32_t align, bool external);

symbol_ix Object_declare_import(Object* object, section_ix section,
                                const uint8_t* name, uint32_t name_len);

symbol_ix Object_declare_fn(Object* object, section_ix section,
                            const uint8_t* name, uint32_t name_len,
                            bool external);

void Object_append_byte(Object* object, section_ix section, uint8_t byte);

void Object_append_data(Object* object, section_ix section, const uint8_t* data, uint32_t size);

bool Object_has_section(Object* obj, enum SectionType type);

void Object_serialize(Object* obj, String* s);

void Linker_run(ObjectSet* objects, const ochar_t** argv, uint32_t argc,
                bool serialize_obj);

#endif
