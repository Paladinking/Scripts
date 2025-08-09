#include "code_generation.h"
#include "mem.h"
#include "format.h"
#include "amd64_asm.h"
#include <printf.h>
#ifndef NO_TZCOUNT
#include <intrin.h>
#endif

#ifdef _MSC_VER
#pragma intrinsic(memset)
#endif

// max: highest variable value + 1
VarSet VarSet_create(var_id max) {
    uint64_t len = max / 64;
    if (max & 0b111111) {
        ++len;
    }

    VarSet s;
    uint64_t* bitmap = Mem_alloc(len * sizeof(uint64_t));
    if (bitmap == NULL) {
        out_of_memory(NULL);
    }
    s.max = len * 64;
    s.bitmap = bitmap;
    memset(s.bitmap, 0, len * sizeof(uint64_t));

    return s;
}

void VarSet_clearall(VarSet* s) {
    uint64_t len = s->max / 64;
    memset(s->bitmap, 0, len * sizeof(uint64_t));
}

void VarSet_free(VarSet* s) {
    Mem_free(s->bitmap);
    s->bitmap = NULL;
    s->max = 0;
}


var_id VarSet_getnext(VarSet* s, var_id min) {
    uint64_t len = s->max / 64;
    uint64_t start = min / 64;
    uint64_t val = start * 64;
    for (uint64_t ix = start; ix < len; ++ix) {
        uint64_t v = s->bitmap[ix];
        var_id p = val;
        while (v) {
#ifndef NO_TZCOUNT
            var_id zeroes = _tzcnt_u64(v);
            p += zeroes;
            if (p > min) {
                return p;
            }
            ++p;
            v >>= (zeroes);
            v >>= 1;
#else
            v >>= 1;
            ++p;
            if ((v & 1) && p > min) {
                return p;
            }
#endif
        }
        val += 64;
    }
    return VAR_ID_INVALID;
}

var_id VarSet_get(VarSet* s) {
    uint64_t len = s->max / 64;
    uint64_t val = 0;
    for (uint64_t ix = 0; ix < len; ++ix) {
        if (s->bitmap[ix]) {
#ifndef NO_TZCOUNT
            return val + _tzcnt_u64(s->bitmap[ix]);
#else
            uint64_t v = s->bitmap[ix];
            while (!(v & 1)) {
                v >>= 1;
                ++val;
            }
            return val;
#endif
        }
        val += 64;
    }
    return VAR_ID_INVALID;
}

void VarSet_grow(VarSet* s, var_id new_max) {
    if (new_max <= s->max) {
        return;
    }

    uint64_t new_len = new_max / 64;
    if (new_max & 0b111111) {
        ++new_len;
    }
    uint64_t old_len = s->max / 64;
    s->max = new_len * 64;

    uint64_t* bitmap = Mem_realloc(s->bitmap, new_len * sizeof(uint64_t));
    if (bitmap == NULL) {
        out_of_memory(NULL);
    }
    s->bitmap = bitmap;
    memset(s->bitmap + old_len, 0, (new_len - old_len) * sizeof(uint64_t));
}

// Compute dest = dest U src
void VarSet_union(VarSet* dest, const VarSet* src) {
    uint64_t len = dest->max / 64;
    for (uint64_t ix = 0; ix < len; ++ix) {
        dest->bitmap[ix] |= src->bitmap[ix];
    }
}

// Copy src into dest
void VarSet_copy(VarSet* dest, const VarSet* src) {
    if (dest->max < src->max) {
        VarSet_grow(dest, src->max);
    }
    uint64_t len = src->max / 64;
    for (uint64_t ix = 0; ix < len; ++ix) {
        dest->bitmap[ix] = src->bitmap[ix];
    }
    if (src->max < dest->max) {
        uint64_t dlen = dest->max / 64;
        memset(dest->bitmap + len, 0, (dlen - len) * sizeof(uint64_t));
    }
}

#ifdef _MSC_VER
#pragma function(memset)
#endif

// Compute dest = dest - src
void VarSet_diff(VarSet* dest, const VarSet* src) {
    uint64_t len = dest->max / 64;
    for (uint64_t ix = 0; ix < len; ++ix) {
        dest->bitmap[ix] &= ~src->bitmap[ix];
    }
}

bool VarSet_equal(const VarSet* a, const VarSet* b) {
    uint64_t len = a->max / 64;
    return memcmp(a->bitmap, b->bitmap, len * sizeof(uint64_t)) == 0;
}


#define INVALID_NODE ((uint64_t)-1)

void ConflictGraph_clear(ConflictGraph* graph) {
    VarSet_clearall(&graph->edges);
    VarSet_clearall(&graph->members);
    for (uint64_t ix = 0; ix < graph->var_count + graph->reg_count; ++ix) {
        VarSet_set(&graph->members, ix);
    }
}

void ConflictGraph_add_var(ConflictGraph* graph, var_id var) {
    assert(var == graph->var_count);
    uint64_t total = graph->reg_count + graph->var_count + 1;

    LOG_INFO("ConflictGraph add var: %llu, %llu", var, total);
    if (total <= graph->width) {
        VarSet_grow(&graph->members, total);
        VarSet_set(&graph->members, var + graph->reg_count);
        graph->var_count += 1;
        return;
    }
    // Reallocate and copy
    ConflictGraph tmp;
    ConflictGraph_create(&tmp, graph->reg_count, graph->var_count + 1);

    for (uint64_t i = 0; i < graph->var_count + graph->reg_count; ++i) {
        for (uint64_t j = 0; j < graph->var_count + graph->reg_count; ++j) {
            if (i == j) {
                continue;
            }
            if (ConflictGraph_has_edge(graph, i, j)) {
                ConflictGraph_add_edge(&tmp, i, j);
            }
        }
    }
    VarSet_grow(&graph->members, tmp.members.max);
    VarSet_set(&graph->members, var + graph->reg_count);

    VarSet_free(&tmp.members);
    VarSet_free(&graph->edges);
    graph->edges = tmp.edges;
    graph->shifts = tmp.shifts;
    graph->width = tmp.width;
    graph->var_count = tmp.var_count;
}

void ConflictGraph_add_edge(ConflictGraph* graph, uint64_t row, uint64_t col);

void ConflictGraph_create(ConflictGraph* graph, uint64_t reg_count, uint64_t var_count) {
    graph->reg_count = reg_count;
    graph->var_count = var_count;
    uint64_t total = reg_count + var_count;
    if (total < 64) {
        total = 64;
    }
    uint64_t b = 1;
    uint64_t shifts = 0;
    while (b < total) {
        b = b << 1;
        ++shifts;
    }
    graph->width = b;
    graph->shifts = shifts;
    graph->edges = VarSet_create(b * b);
    graph->members = VarSet_create(b);
    for (uint64_t ix = 0; ix < reg_count + var_count; ++ix) {
        VarSet_set(&graph->members, ix);
    }

    LOG_INFO("ConflictGraph create: %llu", graph->edges.max);
}

void ConflictGraph_free(ConflictGraph* graph) {
    VarSet_free(&graph->edges);
}

bool ConflictGraph_has_edge(ConflictGraph* graph, uint64_t a, uint64_t b) {
    uint64_t ix = (a << graph->shifts) + b;
    return VarSet_contains(&graph->edges, ix);
}

void ConflictGraph_add_edge(ConflictGraph* graph, uint64_t a, uint64_t b) {
    assert(a != b);
    uint64_t ix = (a << graph->shifts) + b;
    VarSet_set(&graph->edges, ix);
    ix = (b << graph->shifts) + a;
    VarSet_set(&graph->edges, ix);
}

// Count number of edges for node
uint64_t ConflictGraph_count(ConflictGraph* graph, uint64_t node) {
    uint64_t row = (node << graph->shifts) >> 6;
    uint64_t len = graph->width >> 6;
    uint64_t count = 0;
    for (uint64_t ix = 0; ix < len; ++ix) {
        uint64_t mask = graph->edges.bitmap[row + ix] & graph->members.bitmap[ix];
        count += __popcnt64(mask);
    }
    return count;
}

uint64_t ConflictGraph_pick_register(ConflictGraph* graph, var_id var) {
    uint64_t reg = VAR_ID_INVALID;
    for (uint64_t ix = 0; ix < graph->reg_count; ++ix) {
        if (!ConflictGraph_has_edge(graph, ix, var + graph->reg_count)) {
            reg = ix;
            break;
        }
    }
    if (reg == VAR_ID_INVALID) {
        return VAR_ID_INVALID;
    }

    for (var_id ix = 0; ix < graph->var_count; ++ix) {
        if (ix == var) {
            continue;
        }
        if (ConflictGraph_has_edge(graph, var + graph->reg_count, 
                                   ix + graph->reg_count)) {
            ConflictGraph_add_edge(graph, ix + graph->reg_count, reg);
        }
    }
    return reg;
}

void ConflictGraph_update_for_live(ConflictGraph* graph, VarSet* live) {
    var_id a = VarSet_get(live);
    while (a != VAR_ID_INVALID) {
        var_id next = VarSet_getnext(live, a);
        var_id b = next;
        while (b != VAR_ID_INVALID) {
            assert(b < graph->var_count);
            ConflictGraph_add_edge(graph, a + graph->reg_count, b + graph->reg_count);
            b = VarSet_getnext(live, b);
        }
        a = next;
    }

}

FlowNode FlowNode_Create(Quad* start, Quad* end, var_id var_end) {
    FlowNode n = {
        start, end, 
        VarSet_create(var_end),
        VarSet_create(var_end),
        VarSet_create(var_end),
        VarSet_create(var_end),
        {INVALID_NODE, INVALID_NODE}
    };

    return n;
}

var_id create_temp_var(ConflictGraph* graph, VarList* vars, FlowNode* node,
                       VarSet* live, var_id base) {
    RESERVE(vars->data, vars->size + 1, vars->cap);
    var_id id = vars->size;
    vars->size += 1;
    vars->data[id].datatype = vars->data[base].datatype;
    vars->data[id].alloc_type = ALLOC_NONE;
    vars->data[id].name = NAME_ID_INVALID;
    vars->data[id].byte_size = vars->data[base].byte_size;
    vars->data[id].alignment = vars->data[base].alignment;
    vars->data[id].reads = 1;
    vars->data[id].writes = 1;
    vars->data[id].kind = VAR_TEMP;

    VarSet_grow(live, vars->size);
    ConflictGraph_add_var(graph, id);
    return id;
}

void create_live_in_out(FlowNode* nodes, uint64_t node_count, VarSet* work) {
    bool change;
    do {
        change = false;
        for (uint64_t ix = 0; ix < node_count; ++ix) {
            FlowNode* node = &nodes[node_count - ix - 1];
            if (node->successors[0] == INVALID_NODE) {
                VarSet_clearall(work);
            } else if (node->successors[1] == INVALID_NODE) {
                VarSet_copy(work, &nodes[node->successors[0]].live_in);
            } else {
                VarSet_copy(work, &nodes[node->successors[0]].live_in);
                VarSet_union(work, &nodes[node->successors[1]].live_in);
            }
            VarSet_diff(work, &node->def);
            VarSet_union(work, &node->use);
            if (!VarSet_equal(&node->live_in, work)) {
                change = true;
            }
            VarSet tmp = *work;
            *work = node->live_in;
            node->live_in = tmp;
        }
    } while (change);

    for (uint64_t ix = 0; ix < node_count; ++ix) {
        FlowNode* node = &nodes[node_count - ix - 1];
        if (node->successors[0] != INVALID_NODE) {
            VarSet_copy(&node->live_out, &nodes[node->successors[0]].live_in);
            if (node->successors[1] != INVALID_NODE) {
                VarSet_union(&node->live_out, &nodes[node->successors[1]].live_in);
            }
        }
    }
}

void create_conflict_graph(ConflictGraph* graph, FlowNode* nodes, uint64_t node_count,
                           Quads* quads, VarList* vars, VarSet* live, Arena* arena) {
    Backend_inital_contraints(graph, vars->size);

    VarSet_clearall(live);

    for (uint64_t ix = 0; ix < node_count; ++ix) {
        VarSet_grow(&nodes[ix].live_out, vars->size);
        VarSet_grow(&nodes[ix].live_in, vars->size);
        VarSet_copy(live, &nodes[ix].live_out);
        ConflictGraph_update_for_live(graph, live);
        Quad* q = nodes[ix].end;
        while (1) {
            Backend_add_constrains(graph, live, q, vars, &nodes[ix], arena);
            Quad_update_live(q, live);
            ConflictGraph_update_for_live(graph, live);
            if (q == nodes[ix].start) {
                break;
            }
            q = q->last_quad;
        }
    }
}

bool can_allocate(ConflictGraph* graph, uint64_t to_alloc, var_id* stack) {
    VarSet old_members = VarSet_create(graph->members.max);
    VarSet old_edges = VarSet_create(graph->edges.max);
    VarSet_copy(&old_edges, &graph->edges);
    VarSet_copy(&old_members, &graph->members);

    var_id v = graph->reg_count - 1;
    while (1) {
        v = VarSet_getnext(&graph->members, v);
        if (v == VAR_ID_INVALID) {
            break;
        }
        uint64_t reg = ConflictGraph_pick_register(graph, v - graph->reg_count);
        if (reg == VAR_ID_INVALID) {
            break;
        }
        LOG_DEBUG("Tried picking register %llu for var %llu", reg, v - graph->reg_count);
        stack[to_alloc - 1] = v - graph->reg_count;
        --to_alloc;
        VarSet_clear(&graph->members, v);
        v = graph->reg_count - 1;
    }
    VarSet_copy(&graph->edges, &old_edges);
    VarSet_copy(&graph->members, &old_members);
    VarSet_free(&old_edges);
    VarSet_free(&old_members);

    return to_alloc == 0;
}


void Generate_function(Quads* quads, Quad* start, Quad* end,
                       uint64_t* label_map, VarList* vars, Arena* arena) {
    FlowNode* nodes = Mem_alloc(8 * sizeof(FlowNode));
    if (nodes == NULL) {
        out_of_memory(NULL);
    }
    uint64_t node_count = 1;
    uint64_t node_cap = 8;
    nodes[0] = FlowNode_Create(start, start, vars->size);

    Quad* q = start;
    for (Quad* q = start; q != end->next_quad; q = q->next_quad) {
        enum QuadType type = q->type;
        if (type == QUAD_JMP_FALSE || type == QUAD_JMP_TRUE || type == QUAD_JMP ||
            type == QUAD_RETURN) {
            RESERVE(nodes, node_count + 1, node_cap);
            nodes[node_count - 1].end = q;
            Quad_add_usages(q, &nodes[node_count - 1].use, 
                            &nodes[node_count - 1].def, vars);

            nodes[node_count] = FlowNode_Create(q->next_quad, q->next_quad, vars->size);
            ++node_count;
            continue;
        } else if (type == QUAD_LABEL) {
            if (nodes[node_count - 1].start == q) {
                label_map[q->op1.label] = node_count - 1;
                nodes[node_count - 1].end = q;
            } else {
                RESERVE(nodes, node_count + 1, node_cap);
                label_map[q->op1.label] = node_count;
                nodes[node_count] = FlowNode_Create(q, q, vars->size);
                ++node_count;
            }
            // NOTE: QUAD_LABEL has no usages, so no need to call Quad_add_usages
        } else {
            nodes[node_count - 1].end = q;
            Quad_add_usages(q, &nodes[node_count - 1].use, 
                            &nodes[node_count - 1].def, vars);
        }
    }
    if (nodes[node_count - 1].start == end->next_quad) {
        --node_count;
    }

    for (uint64_t ix = 0; ix < node_count; ++ix) {
        assert(nodes[ix].start != NULL);
        Quad* last = nodes[ix].end;
        enum QuadType type = last->type;
        if (type == QUAD_JMP_FALSE || type == QUAD_JMP_TRUE) {
            nodes[ix].successors[0] = label_map[last->op1.label];
            if (ix + 1 < node_count) {
                nodes[ix].successors[1] = ix + 1;
            }
        } else if (type == QUAD_JMP) {
            nodes[ix].successors[0] = label_map[last->op1.label];
        } else if (type == QUAD_RETURN) {
            nodes[ix].successors[0] = INVALID_NODE;
        } else if (ix + 1 < node_count) {
            nodes[ix].successors[0] = ix + 1;
        }
    }
    VarSet work = VarSet_create(vars->size);
    create_live_in_out(nodes, node_count, &work);

    VarSet_clearall(&work);

    ConflictGraph graph;
    ConflictGraph_create(&graph, Backend_get_regs(), vars->size);

    var_id* stack = Mem_alloc(vars->size * sizeof(var_id));
    if (stack == NULL) {
        out_of_memory(NULL);
    }
    uint64_t stack_size = 0;

    for (var_id id = 0; id < vars->size; ++id) {
        if (vars->data[id].kind == VAR_FUNCTION ||
            vars->data[id].kind == VAR_ARRAY ||
            vars->data[id].kind == VAR_GLOBAL ||
            vars->data[id].datatype == VARTYPE_STRUCT ||
            vars->data[id].kind == VAR_ARRAY_GLOBAL) {
            vars->data[id].alloc_type = ALLOC_MEM;
        }
    }

    // Save old variable count to make sure stack does not overflow
    uint64_t var_cap = vars->size;
    while (1) {
        create_conflict_graph(&graph, nodes, node_count, quads, vars, &work, arena);
        if (vars->size > var_cap) {
            stack = Mem_realloc(stack, (vars->size + 10) * sizeof(var_id));
            if (stack == NULL) {
                out_of_memory(NULL);
            }
            var_cap = vars->size + 10;
        }

        uint64_t to_alloc = 0;
        for (var_id id = 0; id < vars->size; ++id) {
            if (vars->data[id].alloc_type != ALLOC_NONE &&
                vars->data[id].alloc_type != ALLOC_REG) {
                VarSet_clear(&graph.members, id + graph.reg_count);
            } else {
                ++to_alloc;
                assert(VarSet_contains(&graph.members, id + graph.reg_count));
            }
        }

        var_id v = graph.reg_count - 1;
        stack_size = 0;
        VarSet_copy(&work, &graph.members);
        while (1) {
            v = VarSet_getnext(&graph.members, v);
            if (v == VAR_ID_INVALID) {
                break;
            }
            uint64_t count = ConflictGraph_count(&graph, v);
            if (count < graph.reg_count) {
                assert(stack_size < var_cap);
                stack[stack_size] = v - graph.reg_count;
                ++stack_size;
                VarSet_clear(&graph.members, v);
                v = graph.reg_count - 1;
            }
        }
        if (stack_size < to_alloc) {
            VarSet_copy(&graph.members, &work);

            if (can_allocate(&graph, to_alloc, stack)) {
                stack_size = to_alloc;
                LOG_DEBUG("Successfull paint");
                break;
            }

            v = graph.reg_count - 1;
            v = VarSet_getnext(&graph.members, v);
            assert(v != VAR_ID_INVALID);
            // Find variable with most conflicts
            uint64_t max_conflicts = ConflictGraph_count(&graph, v);
            var_id max_var = v - graph.reg_count;
            if (vars->data[max_var].alloc_type == ALLOC_REG) {
                max_conflicts = 0;
            }
            do {
                if (vars->data[v  - graph.reg_count].alloc_type == ALLOC_REG) {
                    // This variable has to be in a register...
                    continue;
                }
                uint64_t c = ConflictGraph_count(&graph, v);
                if (c > max_conflicts) {
                    max_conflicts = c;
                    max_var = v - graph.reg_count;
                }
            } while ((v = VarSet_getnext(&graph.members, v)) != VAR_ID_INVALID);
            // max_var is now variable with most conflicts
            // spill it
            assert(vars->data[max_var].alloc_type == ALLOC_NONE);
            vars->data[max_var].alloc_type = ALLOC_MEM;
            LOG_DEBUG("Spill %llu", max_var);
            continue;
        }
        break;
    }
    while (stack_size > 0) {
        var_id v = stack[stack_size - 1];
        --stack_size;
        uint64_t reg = ConflictGraph_pick_register(&graph, v);
        assert(reg != VAR_ID_INVALID);
        LOG_DEBUG("Picked reg %llu for var %llu", reg, v);
        vars->data[v].alloc_type = ALLOC_REG;
        vars->data[v].reg = reg;
    }

    Mem_free(stack);
}

void Generate_code(Quads* quads, FunctionTable* functions, FunctionTable* externs,
                   NameTable* name_table, StringLiteral* literals, Arena* arena) {
    uint64_t* label_map = Mem_alloc(quads->label_count * sizeof(uint64_t));
    if (label_map == NULL) {
        out_of_memory(NULL);
    }

    for (uint64_t ix = 0; ix < functions->size; ++ix) {
        Quad* start = functions->data[ix]->quad_start;
        Quad* end = functions->data[ix]->quad_end;
        // Break up quads
        start->last_quad = NULL;
        end->next_quad = NULL;
        var_id var_start = 0;
        var_id var_end = functions->data[ix]->vars.size;
        
        Generate_function(quads, start, end, label_map,
                          &functions->data[ix]->vars, arena);
    }
    Backend_generate_asm(name_table, functions, externs, literals, arena);
    Mem_free(label_map);
}
