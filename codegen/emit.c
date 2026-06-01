#include "emit.h"

static void emit_operand(AsmOperand* op, FILE* out) {
    switch (op->kind) {
        case OPERAND_REG:
            fprintf(out, "%s", op->reg);
            break;
        case OPERAND_IMM:
            fprintf(out, "#%d", op->imm);
            break;
    }
}

static void emit_instr(AsmInstr* instr, FILE* out) {
    switch (instr->kind) {
        case ASM_MOV:
            fprintf(out, "    mov ");
            emit_operand(&instr->operands[0], out);
            fprintf(out, ", ");
            emit_operand(&instr->operands[1], out);
            fprintf(out, "\n");
            break;
        case ASM_RET:
            fprintf(out, "    ret\n");
            break;
    }
}

void emit_arm64(AsmProgram* prog, FILE* out) {
    for (int i = 0; i < prog->num_functions; i++) {
        AsmFunction* fn = prog->functions[i];
        fprintf(out, ".global _%s\n", fn->name);
        fprintf(out, "_%s:\n", fn->name);
        for (int j = 0; j < fn->num_instrs; j++) {
            emit_instr(&fn->instrs[j], out);
        }
        fprintf(out, "\n");
    }
}
