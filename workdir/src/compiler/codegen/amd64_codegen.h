#ifndef COMPILER_AMD64_ASM_H_00
#define COMPILER_AMD64_ASM_H_00
#include "../code_generation.h"

uint64_t amd64_get_regs();

void amd64_inital_contraints(ConflictGraph* graph, VarList* vars);

void amd64_add_constrains(ConflictGraph* graph, VarSet* live_set, Quad* quad,
                            VarList* vars, FlowNode* node, Arena* arena);

Object* amd64_generate_asm(NameTable* name_table, FunctionTable* func_table,
                             FunctionTable* externs, StringLiteral* literals,
                             Arena* arena, bool serialize);

bool amd64_arg_is_ptr(AllocInfo info);

bool amd64_return_as_ptr(AllocInfo info);
#endif
