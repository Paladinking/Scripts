#include "codegen.h"


// Require <var> uses <reg>
void require_reg(ConflictGraph* graph, uint64_t reg, var_id var) {
    for (uint64_t ix = 0; ix < graph->reg_count; ++ix) {
        if (reg != ix) {
            ConflictGraph_add_edge(graph, var + graph->reg_count, ix);
        }
    }
}


// Require <var> uses memory
void require_mem(ConflictGraph* graph, var_id var) {
    for (uint64_t ix = 0; ix < graph->reg_count; ++ix) {
        ConflictGraph_add_edge(graph, var + graph->reg_count, ix);
    }
}

bool fully_local(VarList *list, var_id v) {
    return list->data[v].reads == 1 && list->data[v].writes == 1 &&
           list->data[v].kind == VAR_TEMP;
}
