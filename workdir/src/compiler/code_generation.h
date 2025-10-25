#ifndef CODE_GENERATION_H_00
#define CODE_GENERATION_H_00
#include "tables.h"
#include "quads.h"
#include "linker/linker.h"

typedef struct ConflictGraph {
    uint64_t reg_count;
    uint64_t var_count;
    uint64_t width;
    uint64_t shifts;
    VarSet edges;
    VarSet members;
} ConflictGraph;

void ConflictGraph_add_edge(ConflictGraph* graph, uint64_t row, uint64_t col);

bool ConflictGraph_has_edge(ConflictGraph* graph, uint64_t a, uint64_t b);

void ConflictGraph_create(ConflictGraph* graph, uint64_t reg_count, uint64_t var_count);

void ConflictGraph_clear(ConflictGraph* graph);

void ConflictGraph_add_var(ConflictGraph* graph, var_id var);

void ConflictGraph_update_for_live(ConflictGraph* graph, VarSet* live);

typedef struct FlowNode {
    // First quad in node
    Quad* start;
    // Last quad in node (inclusive)
    Quad* end;

    // Variables defined in node
    VarSet def;
    // Variables used in node before defined
    VarSet use;
    // Varibles live before first instruction of node
    VarSet live_in;
    // Varibles live after last instruction of node
    VarSet live_out;

    uint64_t successors[2];
} FlowNode;

var_id create_temp_var(ConflictGraph* graph, VarList* vars, FlowNode* node,
                       VarSet* live, var_id base);

var_id create_typed_temp_var(ConflictGraph* graph, VarList* vars, FlowNode* node,
                             VarSet* live, uint32_t byte_size, uint32_t align,
                             enum VarDatatype datatype);

Object* Generate_code(Quads* quads, FunctionTable* functions, FunctionTable* externs,
                      NameTable* name_table, StringLiteral* literals, Arena* arena,
                      bool serialze_asm);

bool Backend_arg_is_ptr(AllocInfo info);

bool Backend_return_as_ptr(AllocInfo info);

#endif
