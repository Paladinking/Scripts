#include "tables.h"
#include "utils.h"
#include "log.h"
#include <mem.h>


// Allocate new type id. Does not initialize
type_id create_type_id(TypeTable* type_table) {
    if (type_table->capacity == type_table->size) {
        TypeData* d = Mem_realloc(type_table->data,
                type_table->capacity * 2 * sizeof(TypeData));
        if (d == NULL) {
            out_of_memory(NULL);
        }
        type_table->data = d;
        type_table->capacity *= 2;
    }
    type_table->size += 1;
    return type_table->size - 1;
}

type_id type_function_create(TypeTable *type_table, FunctionDef *def, Arena* arena) {
    type_id t = create_type_id(type_table);
    TypeData* d = &type_table->data[t];
    d->parent = TYPE_ID_INVALID;
    d->kind = TYPE_NORMAL;
    d->ptr_type = TYPE_ID_INVALID;
    d->array_size = 0;
    d->type_def = Arena_alloc_type(arena, TypeDef);
    d->type_def->kind = TYPEDEF_FUNCTION;
    d->type_def->func = def;

    return t;
}

void name_scope_begin(NameTable *name_table) {
    if (name_table->scope_count == name_table->scope_capacity) {
        uint64_t c = name_table->scope_capacity * 2;
        name_id* n = Mem_realloc(name_table->scope_stack, c * sizeof(name_id));
        if (n == NULL) {
            out_of_memory(name_table);
            return;
        }
        name_table->scope_stack = n;
        name_table->scope_capacity = c;
    }
    name_table->scope_stack[name_table->scope_count] = name_table->size - 1;
    name_table->scope_count += 1;
}

void name_scope_end(NameTable *name_table) {
    name_table->scope_count -= 1;
    name_id id = name_table->size - 1;
    while (id > name_table->scope_stack[name_table->scope_count]) {
        hash_id link = name_table->data[id].hash_link;
        name_id parent = name_table->data[id].parent;
        name_table->hash_map[link] = parent;
        --id;
    }
}

static hash_id hash(const uint8_t* str, uint32_t len) {
    uint64_t hash = 5381;
    int c;
    for (uint32_t ix = 0; ix < len; ++ix) {
        c = str[ix];
        hash = ((hash << 5) + hash) + c;
    }
    return hash % NAME_TABLE_HASH_ENTRIES;
}

name_id name_type_insert(NameTable* name_table, StrWithLength name,
                         type_id type, TypeDef* def, Arena* arena) {
    name_id id = name_insert(name_table, name, type, NAME_TYPE, arena);
    if (id != NAME_ID_INVALID) {
        name_table->data[id].type_def = def;
    }
    return id;
}

name_id name_function_insert(NameTable *name_table, StrWithLength name,
                             TypeTable *type_table, FunctionDef *def,
                             Arena *arena) {
    name_id id = name_insert(name_table, name, TYPE_ID_NONE,
                             NAME_FUNCTION, arena);
    if (id != NAME_ID_INVALID) {
        type_id type = type_function_create(type_table, def, arena);
        name_table->data[id].type = type;
        name_table->data[id].func_def = def;
    }
    return id;
}

name_id name_variable_insert(NameTable* name_table, StrWithLength name, type_id type,
                             func_id func, Arena* arena) {
    name_id id = name_insert(name_table, name, type, NAME_VARIABLE, arena);
    if (id != NAME_ID_INVALID) {
        name_table->data[id].function = func;
    }
    return id;
}

name_id name_insert(NameTable *names, StrWithLength name,
                    type_id type, enum NameKind kind, Arena* arena) {

    hash_id h = hash(name.str, name.len);

    name_id id = names->hash_map[h];
    name_id n_id = id;

    while (n_id != NAME_ID_INVALID) {
        if (names->data[n_id].name_len == name.len &&
            memcmp(names->data[n_id].name, name.str, name.len) == 0) {
            if (n_id > names->scope_stack[names->scope_count - 1] ||
                n_id <= names->scope_stack[0]) {
                // Name already taken in this scope, or in builtin scope
                LOG_DEBUG("insert_name: Name '%.*s' already taken", name.len, name.str);
                return NAME_ID_INVALID;
            }
        }
        n_id = names->data[n_id].parent;
    }
    n_id = names->size;
    LOG_DEBUG("insert_name: Inserted name '%.*s', id %llu", name.len, name.str, n_id);
    uint8_t* name_ptr = Arena_alloc(arena, name.len, 1);
    memcpy(name_ptr, name.str, name.len);
    if (names->size == names->capacity) {
        size_t new_cap = names->capacity * 2;
        NameData* new_nd = Mem_realloc(names->data,
                                       new_cap * sizeof(NameData));
        if (new_nd == NULL) {
            out_of_memory(NULL);
            return NAME_ID_INVALID;
        }
        names->data = new_nd;
        names->capacity = new_cap;
    }
    names->size += 1;
    names->data[n_id].kind = kind;
    names->data[n_id].has_var = false;
    names->data[n_id].parent = id;
    names->data[n_id].hash_link = h;
    names->data[n_id].name = name_ptr;
    names->data[n_id].name_len = name.len;
    names->data[n_id].type = type;

    names->hash_map[h] = n_id;
    return n_id;
}

name_id name_find(NameTable *name_table, StrWithLength name) {
    hash_id h = hash(name.str, name.len);
    name_id id = name_table->hash_map[h];
    while (id != NAME_ID_INVALID) {
        if (name_table->data[id].name_len == name.len &&
            memcmp(name_table->data[id].name, name.str, name.len) == 0) {
            return id;
        }
        id = name_table->data[id].parent;
    }
    return id;
}

type_id type_of(NameTable* name_table, name_id name) {
    if (name == NAME_ID_INVALID) {
        return TYPE_ID_INVALID;
    }
    return name_table->data[name].type;
}

type_id type_ptr_of(TypeTable *type_table, type_id id) {
    if (type_table->data[id].ptr_type == TYPE_ID_INVALID) {
        type_id t = create_type_id(type_table);
        type_table->data[id].ptr_type = t;
        type_table->data[t].kind = TYPE_PTR;
        type_table->data[t].type_def = type_table->data[id].type_def;
        type_table->data[t].parent = id;
        type_table->data[t].ptr_type = TYPE_ID_INVALID;
        type_table->data[t].array_size = 0;
    }
    return type_table->data[id].ptr_type;
}

type_id type_array_of(TypeTable* type_table, type_id id, uint64_t size) {
    type_id t = create_type_id(type_table);
    type_table->data[t].kind = TYPE_ARRAY;
    type_table->data[t].type_def = type_table->data[id].type_def;
    type_table->data[t].parent = id;
    type_table->data[t].ptr_type = TYPE_ID_INVALID;
    type_table->data[t].array_size = size;

    return t;
}

static AllocInfo allocation_of_type(TypeDef* def) {
    if (def->kind == TYPEDEF_BUILTIN) {
        return (AllocInfo){def->builtin.byte_size, def->builtin.byte_alignment};
    }
    if (def->kind == TYPEDEF_FUNCTION) {
        return (AllocInfo){0, 1};
    }
    // TODO: struct
    LOG_ERROR("Unkown typedef %d", def->kind);
    assert(false);
    return (AllocInfo){0, 0};
}

AllocInfo type_allocation(TypeTable* type_table, type_id id) {
    assert(id != TYPE_ID_INVALID);
    TypeDef* def = type_table->data[id].type_def;
    if (type_table->data[id].kind == TYPE_NORMAL) {
        return allocation_of_type(def);
    } else if (type_table->data[id].kind == TYPE_ARRAY) {
        AllocInfo root = allocation_of_type(def);
        uint64_t scale = type_table->data[id].array_size;
        // For now...
        assert(type_table->data[type_table->data[id].parent].kind == TYPE_NORMAL);

        root.size *= scale;
        return root;
    } else if (type_table->data[id].kind == TYPE_PTR) {
        return (AllocInfo){PTR_SIZE, PTR_SIZE};
    } else {
        assert(false);
    }
}
