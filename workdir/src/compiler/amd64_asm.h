#ifndef COMPILER_AMD64_ASM_H_00
#define COMPILER_AMD64_ASM_H_00
#include "code_generation.h"
#include "linker/linker.h"

uint64_t Backend_get_regs();

void Backend_inital_contraints(ConflictGraph* graph, var_id var_count);

void Backend_add_constrains(ConflictGraph* graph, VarSet* live_set, Quad* quad,
                            VarList* vars, FlowNode* node, Arena* arena);

Object* Backend_generate_asm(NameTable* name_table, FunctionTable* func_table,
                             FunctionTable* externs, StringLiteral* literals,
                             Arena* arena, bool serialize);

#endif
