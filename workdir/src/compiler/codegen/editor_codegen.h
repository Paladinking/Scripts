#ifndef EDITOR_CODEGEN_H_00
#define EDITOR_CODEGEN_H_00

#include "../code_generation.h"

uint64_t editor_get_regs();

void editor_inital_contraints(ConflictGraph* graph, VarList* vars);

void editor_add_constrains(ConflictGraph* graph, VarSet* live_set, Quad* quad,
                            VarList* vars, FlowNode* node, Arena* arena);

Object* editor_generate_asm(NameTable* name_table, FunctionTable* func_table,
                             FunctionTable* externs, StringLiteral* literals,
                             Arena* arena, bool serialize);

bool editor_arg_is_ptr(AllocInfo info);

bool editor_return_as_ptr(AllocInfo info);

#endif
