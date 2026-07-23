#include "editor_asm.h"

static EditorOp* create_op(EditorAsmCtx* ctx, enum EditorOpcode opcode) {
    EditorOp* op = Arena_alloc_type(&ctx->arena, EditorOp);
    op->next = NULL;
    op->opcode = opcode;
    return op;
}

void editor_asm_ctx_create(EditorAsmCtx* ctx, Object* object, section_ix code_section) {
    const enum LogCatagory LOG_CATAGORY = LOG_CATAGORY_ASM_GENERATION;
    if (!Arena_create(&ctx->arena, 0xffffffff, out_of_memory, NULL)) {
        LOG_CRITICAL("Failed creating arena");
        fatal_error(NULL, PARSE_ERROR_NONE, LINE_INFO_NONE);
        return;
    }
    ctx->start = create_op(ctx, OPCODE_START);
    ctx->end = create_op(ctx, OPCODE_END);

    ctx->start->next = ctx->end;

    ctx->label_count = 0;
    ctx->label_cap = 32;
    ctx->labels = Mem_alloc(32 * sizeof(LabelData));
    if (ctx->labels == NULL) {
        out_of_memory(NULL);
    }

    ctx->object = object;
    ctx->code_section = code_section;
}
void editor_asm_add_n(EditorAsmCtx* ctx, enum EditorOpcode op);
void editor_asm_add_rr(EditorAsmCtx* ctx, enum EditorOpcode op, uint8_t reg1, uint8_t reg2);
void editor_asm_add_r(EditorAsmCtx* ctx, enum EditorOpcode op, uint8_t reg);
void editor_asm_add_sr(EditorAsmCtx* ctx, enum EditorOpcode op, uint8_t sreg, uint8_t reg);
void editor_asm_add_ri(EditorAsmCtx* ctx, enum EditorOpcode op, uint8_t reg1, int64_t imm);
void editor_asm_add_rri(EditorAsmCtx* ctx, enum EditorOpcode op, uint8_t reg1, uint8_t reg2, int64_t imm);
void editor_asm_add_label(EditorAsmCtx* ctx, uint64_t label);
void editor_asm_add_i(EditorAsmCtx* ctx, enum EditorOpcode op, int64_t imm);
void editor_asm_add_lra(EditorAsmCtx* ctx, uint8_t reg, uint64_t label);
