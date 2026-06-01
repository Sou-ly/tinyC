#include "asm_ast.h"
#include <string.h>

// --- Operands ---

AsmOperand asm_reg(const char* reg) {
    return (AsmOperand){.kind = OPERAND_REG, .reg = strdup(reg)};
}

AsmOperand asm_imm(int value) {
    return (AsmOperand){.kind = OPERAND_IMM, .imm = value};
}

void destroy_operand(AsmOperand* op) {
    if (op->kind == OPERAND_REG)
        free(op->reg);
}

// --- Instructions ---

AsmInstr asm_mov(AsmOperand dst, AsmOperand src) {
    AsmOperand* ops = malloc(sizeof(AsmOperand) * 2);
    ops[0] = dst;
    ops[1] = src;
    return (AsmInstr){.kind = ASM_MOV, .operands = ops, .num_operands = 2};
}

AsmInstr asm_ret(void) {
    return (AsmInstr){.kind = ASM_RET, .operands = NULL, .num_operands = 0};
}

void destroy_instr(AsmInstr* instr) {
    for (int i = 0; i < instr->num_operands; i++) {
        destroy_operand(&instr->operands[i]);
    }
    free(instr->operands);
}

// --- Functions ---

AsmFunction* create_asm_function(char* name, AsmInstr* instrs, int num_instrs) {
    AsmFunction* fn = malloc(sizeof(AsmFunction));
    fn->name = strdup(name);
    fn->instrs = instrs;
    fn->num_instrs = num_instrs;
    return fn;
}

void destroy_asm_function(AsmFunction* fn) {
    if (!fn) return;
    free(fn->name);
    for (int i = 0; i < fn->num_instrs; i++) {
        destroy_instr(&fn->instrs[i]);
    }
    free(fn->instrs);
    free(fn);
}

// --- Program ---

AsmProgram* create_asm_program(AsmFunction** functions, int num_functions) {
    AsmProgram* prog = malloc(sizeof(AsmProgram));
    prog->functions = functions;
    prog->num_functions = num_functions;
    return prog;
}

void destroy_asm_program(AsmProgram* prog) {
    if (!prog) return;
    for (int i = 0; i < prog->num_functions; i++) {
        destroy_asm_function(prog->functions[i]);
    }
    free(prog->functions);
    free(prog);
}
