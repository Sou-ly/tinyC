#include "codegen.h"
#include <stdio.h>
#include <string.h>

static x86_Operand codegen_val(IrVal val) {
    switch (val.kind) {
        case IR_CONSTANT:
            return (x86_Operand){.kind = x86_IMM, .imm = val.int_val};
        case IR_VARIABLE:
            return (x86_Operand){.kind = x86_ID, .identifier = strdup(val.name)};
    }
    fprintf(stderr, "codegen: unsupported IR value kind\n");
    exit(1);
}

static x86_Unop codegen_unop(IrUnaryOpType op) {
    switch (op) {
        case IR_NEG:  return x86_NEG;
        case IR_COMP: return x86_NOT;
    }
    fprintf(stderr, "codegen: unsupported unary op\n");
    exit(1);
}

static void append_instr(x86_Instr** instrs, int* count, x86_Instr instr) {
    *instrs = realloc(*instrs, (*count + 1) * sizeof(x86_Instr));
    (*instrs)[*count] = instr;
    (*count)++;
}

static void codegen_instr(IrInstruction* ir_instr, x86_Instr** instrs, int* count) {
    switch (ir_instr->type) {
        case IR_RETURN: {
            x86_Operand src = codegen_val(ir_instr->ret.val);
            append_instr(instrs, count, (x86_Instr){.kind = x86_MOV, .mov = {
                .dst = (x86_Operand){.kind = x86_REG, .reg = x86_AX},
                .src = src
            }});
            append_instr(instrs, count, (x86_Instr){.kind = x86_RET});
            return;
        }
        case IR_UNARY: {
            x86_Operand src = codegen_val(ir_instr->unary.src);
            x86_Operand dst = codegen_val(ir_instr->unary.dst);
            append_instr(instrs, count, (x86_Instr){.kind = x86_MOV, .mov = {
                .dst = dst,
                .src = src
            }});
            x86_Operand dst2 = codegen_val(ir_instr->unary.dst);
            append_instr(instrs, count, (x86_Instr){.kind = x86_UNOP, .unop = {
                .unop = codegen_unop(ir_instr->unary.op),
                .operand = dst2
            }});
            return;
        }
    }
    fprintf(stderr, "codegen: unsupported IR instruction type\n");
    exit(1);
}

static x86_Function codegen_function(IrFunction* ir_fn) {
    x86_Instr* instrs = NULL;
    int num_instrs = 0;
    for (int i = 0; i < ir_fn->size; i++) {
        codegen_instr(&ir_fn->instructions[i], &instrs, &num_instrs);
    }
    return make_x86_function(ir_fn->name, instrs, num_instrs);
}

x86_Program codegen(IrProgram* program) {
    x86_Function* functions = malloc(sizeof(x86_Function) * program->size);
    for (int i = 0; i < program->size; i++) {
        functions[i] = codegen_function(&program->functions[i]);
    }
    return make_x86_program(functions, program->size);
}
