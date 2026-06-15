#include "emit.h"
#include <stdbool.h>
#include <string.h>

static char* emit_cond_code(x86_ConditionCode cond) {
	switch (cond) {
		case x86_E:		return "e";
    	case x86_NE:	return "ne";
    	case x86_L:		return "l";
    	case x86_LE:	return "le";
    	case x86_G:		return "g";
    	case x86_GE:	return "ge";
	}
	return "???";
}

static char* reg_name(x86_Reg reg, bool one_byte) {
    switch (reg) {
		case x86_AX:  return one_byte? "al" : "eax";
        case x86_DX:  return one_byte? "dl" : "edx";
        case x86_R10: return one_byte? "r10l" : "r10d";
        case x86_R11: return one_byte? "r11l" : "r11d";
    }
	return "???";
}

static void emit_operand(x86_Operand* op, FILE* out) {
    switch (op->kind) {
        case x86_REG:
            fprintf(out, "%%%s", reg_name(op->reg, false));
            break;
        case x86_IMM:
            fprintf(out, "$%d", op->imm);
            break;
        case x86_STACK:
            fprintf(out, "%d(%%rbp)", op->stack);
            break;
        case x86_ID:
            fprintf(out, "<pseudo:%s>", op->identifier);
            break;
    }
}

static const char* unop_name(x86_Unop op) {
    switch (op) {
        case x86_NEG: return "negl";
        case x86_COMP: return "notl";
    }
	fprintf(stderr, "unrecognized unop\n");
	exit(1);
}

static const char* binop_name(x86_Binop op) {
    switch (op) {
		// note: AT&T synthax
        case x86_ADD:		return "addl";
        case x86_SUB: 		return "subl";
        case x86_MUL: 		return "imull";
		case x86_AND:		return "andl";
		case x86_OR:		return "orl";
		case x86_XOR:		return "xorl";
		case x86_LSHIFT:	return "shll";
		case x86_RSHIFT:	return "shrl";
		default:			break;
    }
	fprintf(stderr, "unrecognized binop\n");
	exit(1);
}

static void emit_instr(x86_Instr* instr, FILE* out) {
    switch (instr->kind) {
        case x86_MOV:
            fprintf(out, "    movl ");
            emit_operand(&instr->mov.src, out);
            fprintf(out, ", ");
            emit_operand(&instr->mov.dst, out);
            fprintf(out, "\n");
            break;
        case x86_UNOP:
            fprintf(out, "    %s ", unop_name(instr->unop.unop));
            emit_operand(&instr->unop.operand, out);
            fprintf(out, "\n");
            break;
        case x86_BINOP:
            fprintf(out, "    %s ", binop_name(instr->binop.optype));
            emit_operand(&instr->binop.rhs, out);
            fprintf(out, ", ");
            emit_operand(&instr->binop.dst, out);
            fprintf(out, "\n");
            break;
        case x86_IDIV:
            fprintf(out, "    idivl ");
            emit_operand(&instr->idiv.operand, out);
            fprintf(out, "\n");
            break;
        case x86_CDQ:
            fprintf(out, "    cdq\n");
            break;
        case x86_ALLOC:
            fprintf(out, "    subq $%d, %%rsp\n", instr->alloc_stack.size);
            break;
        case x86_RET:
            fprintf(out, "    movq %%rbp, %%rsp\n");
            fprintf(out, "    popq %%rbp\n");
            fprintf(out, "    ret\n");
            break;
        case x86_CMP:
            fprintf(out, "    cmpl ");
            emit_operand(&instr->cmp.lhs, out);
            fprintf(out, ", ");
            emit_operand(&instr->cmp.rhs, out);
            fprintf(out, "\n");
        case x86_JMP:
            fprintf(out, "   jmp  .L%s\n", instr->jmp.identifier);
        case x86_JMPCC:
            fprintf(out, "   j%s  .L%s\n", emit_cond_code(instr->jmpcc.cond), instr->jmpcc.identifier);
            fprintf(out, "\n");
        case x86_SETCC:
            fprintf(out, "   set%s ", emit_cond_code(instr->setcc.cond));
			fprintf(out, "%%%s", reg_name(instr->setcc.op.reg, true));
            fprintf(out, "\n");
        case x86_LABEL:
            fprintf(out, ".L%s:", instr->label.identifier);
    }
}

void emit_asm(x86_Program* prog, FILE* out) {
    for (int i = 0; i < prog->num_functions; i++) {
        x86_Function* fn = &prog->functions[i];
        if (strcmp(fn->name, "main") == 0) {
            fprintf(out, ".global _%s\n", fn->name);
        }
        fprintf(out, "_%s:\n", fn->name);
        fprintf(out, "    pushq %%rbp\n");
        fprintf(out, "    movq %%rsp, %%rbp\n");
        for (x86_Instr* instr = fn->instrs.head; instr; instr = instr->next) {
            emit_instr(instr, out);
        }
        fprintf(out, "\n");
    }
}
