#ifndef CODEGEN_H_00
#define CODEGEN_H_00

#include "../code_generation.h"


void require_reg(ConflictGraph* graph, uint64_t reg, var_id var);

void require_mem(ConflictGraph* graph, var_id var);

bool fully_local(VarList* list, var_id v);

#endif
