#include "emit.h"
#include "../common/ice.h"
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

// x86-64 register names are irregular across widths: the legacy accumulators
// drop their prefix letter at one byte (%rax/%eax/%al), the index registers
// append one instead (%rdi/%edi/%dil), and the numbered registers use suffixes
// with none at eight bytes (%r8/%r8d/%r8b). Three rules, so this is a table
// rather than something computed.
static const char* const reg_names[][x86_SZ_COUNT] = {
	//              1-byte  4-byte  8-byte
	[x86_AX]  = {	"al",	"eax",	"rax"	},
	[x86_DI]  = {	"dil",	"edi",	"rdi"	},
	[x86_SI]  = {	"sil",	"esi",	"rsi"	},
	[x86_DX]  = {	"dl",	"edx",	"rdx"	},
	[x86_CX]  = {	"cl",	"ecx",	"rcx"	},
	[x86_R8]  = {	"r8b",	"r8d",	"r8"	},
	[x86_R9]  = {	"r9b",	"r9d",	"r9"	},
	[x86_R10] = {	"r10b",	"r10d",	"r10"	},
	[x86_R11] = {	"r11b",	"r11d",	"r11"	},
};

// Adding a register to x86_Reg without giving it names is a build failure here,
// rather than invalid assembly discovered by the assembler later.
_Static_assert(sizeof reg_names / sizeof reg_names[0] == x86_REG_COUNT,
               "reg_names is missing an entry for a register in x86_Reg");

static const char* reg_name(x86_Reg reg, x86_Size size) {
    if (reg < 0 || reg >= x86_REG_COUNT) ICE("emit: register out of range: %d", reg);
    if (size < 0 || size >= x86_SZ_COUNT) ICE("emit: operand size out of range: %d", size);
    return reg_names[reg][size];
}

static void emit_operand(x86_Operand* op, x86_Size size, FILE* out) {
    switch (op->kind) {
        case x86_REG:
            fprintf(out, "%%%s", reg_name(op->as.reg, size));
            break;
        case x86_IMM:
            fprintf(out, "$%d", op->as.imm);
            break;
        case x86_STACK:
            fprintf(out, "%d(%%rbp)", op->as.stack);
            break;
        case x86_ID:
            fprintf(out, "<pseudo:%s>", op->as.identifier);
            break;
    }
}

static const char* unop_name(x86_Unop op) {
    switch (op) {
        case x86_NEG: return "negl";
        case x86_COMP: return "notl";
    }
	ICE("unrecognized unop");
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
	ICE("unrecognized binop");
}

static void emit_instr(x86_Instr* instr, FILE* out) {
    switch (instr->kind) {
        case x86_MOV:
            fprintf(out, "    movl ");
            emit_operand(&instr->as.mov.src, x86_SZ_4, out);
            fprintf(out, ", ");
            emit_operand(&instr->as.mov.dst, x86_SZ_4, out);
            fprintf(out, "\n");
            break;
        case x86_UNOP:
            fprintf(out, "    %s ", unop_name(instr->as.unop.unop));
            emit_operand(&instr->as.unop.operand, x86_SZ_4, out);
            fprintf(out, "\n");
            break;
        case x86_BINOP:
            fprintf(out, "    %s ", binop_name(instr->as.binop.optype));
            emit_operand(&instr->as.binop.rhs, x86_SZ_4, out);
            fprintf(out, ", ");
            emit_operand(&instr->as.binop.dst, x86_SZ_4, out);
            fprintf(out, "\n");
            break;
        case x86_IDIV:
            fprintf(out, "    idivl ");
            emit_operand(&instr->as.idiv.operand, x86_SZ_4, out);
            fprintf(out, "\n");
            break;
        case x86_CDQ:
            fprintf(out, "    cdq\n");
            break;
        case x86_ALLOC:
            fprintf(out, "    subq $%d, %%rsp\n", instr->as.alloc_stack.size);
            break;
        case x86_RET:
            fprintf(out, "    movq %%rbp, %%rsp\n");
            fprintf(out, "    popq %%rbp\n");
            fprintf(out, "    ret\n");
            break;
        case x86_CMP:
            fprintf(out, "    cmpl ");
            emit_operand(&instr->as.cmp.lhs, x86_SZ_4, out);
            fprintf(out, ", ");
            emit_operand(&instr->as.cmp.rhs, x86_SZ_4, out);
            fprintf(out, "\n");
            break;
        case x86_JMP:
            fprintf(out, "    jmp .L%s\n", instr->as.jmp.identifier);
            break;
        case x86_JMPCC:
            fprintf(out, "    j%s .L%s\n", emit_cond_code(instr->as.jmpcc.cond), instr->as.jmpcc.identifier);
            break;
        case x86_SETCC:
            // setcc writes a single byte, so a register operand needs its
            // 1-byte name; emit_operand handles that now, and memory and
            // immediate operands are formatted the same at any width.
            fprintf(out, "    set%s ", emit_cond_code(instr->as.setcc.cond));
            emit_operand(&instr->as.setcc.op, x86_SZ_1, out);
            fprintf(out, "\n");
            break;
        case x86_LABEL:
            fprintf(out, ".L%s:\n", instr->as.label.identifier);
            break;
		case x86_DEALLOCATE:
			fprintf(out, "    addq $%d, %%rsp\n", instr->as.deallocate.val);
			break;
		case x86_PUSH:
			fprintf(out, "    pushq ");
			emit_operand(&instr->as.push.operand, x86_SZ_8, out);
			fprintf(out, "\n");
			break;
		case x86_CALL:
			fprintf(out, "    call %s\n", instr->as.call.identifier);
			break;
    }
}

// Mach-O prefixes C symbols with an underscore; ELF (Linux) does not. Emit the
// prefix only on Apple so the linker finds `main` on either platform.
#ifdef __APPLE__
#define SYMBOL_PREFIX "_"
#else
#define SYMBOL_PREFIX ""
#endif

void emit_asm(x86_Program* prog, FILE* out) {
    for (int i = 0; i < prog->num_functions; i++) {
        x86_Function* fn = &prog->functions[i];
        if (strcmp(fn->identifier, "main") == 0) {
            fprintf(out, ".global " SYMBOL_PREFIX "%s\n", fn->identifier);
        }
        fprintf(out, SYMBOL_PREFIX "%s:\n", fn->identifier);
        fprintf(out, "    pushq %%rbp\n");
        fprintf(out, "    movq %%rsp, %%rbp\n");
        for (x86_Instr* instr = fn->instrs.head; instr; instr = instr->next) {
            emit_instr(instr, out);
        }
        fprintf(out, "\n");
    }
}
