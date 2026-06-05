#include "emit.h"

static const char* reg_name(x86_Reg reg) {
    switch (reg) {
        case x86_AX:  return "eax";
        case x86_R10: return "r10d";
    }
    return "???";
}

static void emit_operand(x86_Operand* op, FILE* out) {
    switch (op->kind) {
        case x86_REG:
            fprintf(out, "%s", reg_name(op->reg));
            break;
        case x86_IMM:
            fprintf(out, "#%d", op->imm);
            break;
    }
}

static void emit_instr(x86_Instr* instr, FILE* out) {
    switch (instr->kind) {
        case x86_MOV:
            fprintf(out, "    mov ");
            emit_operand(&instr->mov.dst, out);
            fprintf(out, ", ");
            emit_operand(&instr->mov.src, out);
            fprintf(out, "\n");
            break;
        case x86_RET:
            fprintf(out, "    ret\n");
            break;
    }
}

void emit_arm64(x86_Program* prog, FILE* out) {
    for (int i = 0; i < prog->num_functions; i++) {
        x86_Function* fn = prog->functions[i];
        fprintf(out, ".global _%s\n", fn->name);
        fprintf(out, "_%s:\n", fn->name);
        for (int j = 0; j < fn->num_instrs; j++) {
            emit_instr(&fn->instrs[j], out);
        }
        fprintf(out, "\n");
    }
}
