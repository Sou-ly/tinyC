#include "ir.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>


static int VAR_NAME_COUNTER = 0;

static char *generate_temp_name(const char *prefix, char sep) {
	int len = snprintf(NULL, 0, "%s%c%d", prefix, sep, VAR_NAME_COUNTER);
	char *name = (char *)malloc(len + 1);
	if (!name) return NULL;
	snprintf(name, len + 1, "%s%c%d", prefix, sep, VAR_NAME_COUNTER++);
	return name;
}

static char *generate_variable_name(void) {
	return generate_temp_name("var", '.');
}

static char * generate_label_name() {
	return generate_temp_name("label", '_');
}

static void append_ir_instruction(IrFunction* function, IrInstruction instruction) {
	function->size++;
	function->instructions = realloc(function->instructions, function->size * sizeof(IrInstruction));
	function->instructions[function->size - 1] = instruction;
}

static IrUnopType convert_ir_unary(AstUnopType ast_op) {
	switch (ast_op) {
		case UNOP_COMP:		return IR_COMP;
		case UNOP_MINUS:	return IR_NEG;
		case UNOP_NOT:		return IR_NOT;
	}
	fprintf(stderr, "ir: unsupported unary operator\n");
	exit(1);
}

static IrBinopType convert_ir_binop(AstBinopType ast_op) {
	switch (ast_op) {
		case BINOP_ADD:		return IR_ADD;
		case BINOP_SUB:		return IR_SUB;
		case BINOP_MUL:		return IR_MUL;
		case BINOP_DIV:		return IR_DIV;
		case BINOP_MOD:		return IR_MOD;
		case BINOP_AND:		return IR_AND;
		case BINOP_OR:		return IR_OR;
		case BINOP_XOR:		return IR_XOR;
		case BINOP_LSHIFT:	return IR_LSHIFT;
		case BINOP_RSHIFT:	return IR_RSHIFT;
		case BINOP_LAND:	return IR_LAND;
		case BINOP_LOR:		return IR_LOR;
		case BINOP_EQ:		return IR_EQ;
		case BINOP_NEQ:		return IR_NEQ;
		case BINOP_LESS:	return IR_LESS;
		case BINOP_GREATER:	return IR_GREATER;
		case BINOP_LEQ:		return IR_LEQ;
		case BINOP_GEQ:		return IR_GEQ;
	}
	fprintf(stderr, "ir: unsupported binary operator\n");
	exit(1);
}

static IrVal emit_ir_expression(const AstExp* exp, IrFunction* ir_function) {
	switch (exp->kind) {
		case EXP_INT: {
			IrVal constant = { IR_CONSTANT, .int_val = exp->int_lit.value };
			return constant;
		}
		case EXP_UNOP: {
			IrVal src = emit_ir_expression(exp->unary.operand, ir_function);
			IrVal dst = { IR_VARIABLE, .name = generate_variable_name() };
			IrInstruction instruction = {
				IR_UNOP,
				.unary = { convert_ir_unary(exp->unary.op_type), src, dst }
			};
			append_ir_instruction(ir_function, instruction);
			return dst;
		}
		case EXP_BINOP: {
			IrBinopType optype = convert_ir_binop(exp->binop.op_type);
			if (optype == IR_LAND) {
				IrInstruction false_label = { IR_LABEL, .label = { generate_label_name() } };
				IrInstruction end_label = { IR_LABEL, .label = { generate_label_name() } };
				IrVal dst = { IR_VARIABLE, .name = generate_variable_name() };
				// evaluate v1
				IrVal lhs = emit_ir_expression(exp->binop.lhs, ir_function);
				// jumpIfZero(v1, false)
				IrInstruction jump_zero_lhs = { IR_JUMP_ZERO, .jump_zero = { lhs, false_label.label.identifier } };
				append_ir_instruction(ir_function, jump_zero_lhs);
				// evaluate v2
				IrVal rhs = emit_ir_expression(exp->binop.rhs, ir_function);
				// jumpIfZero(v2, false)
				IrInstruction jump_zero_rhs = { IR_JUMP_ZERO, .jump_zero = { rhs, false_label.label.identifier } };
				append_ir_instruction(ir_function, jump_zero_rhs);
				// return = 1
				IrInstruction store_true = { IR_COPY, .copy = { { IR_CONSTANT, .int_val = 1 }, dst } };
				append_ir_instruction(ir_function, store_true);
				// jump(end)
				IrInstruction jump_end = { IR_JUMP, .jump = { end_label.label.identifier } };
				append_ir_instruction(ir_function, jump_end);
				// false
				append_ir_instruction(ir_function, false_label);
				// return = 0
				IrInstruction store_false = { IR_COPY, .copy = { { IR_CONSTANT, .int_val = 0 }, dst } };
				append_ir_instruction(ir_function, store_false);
				// end
				append_ir_instruction(ir_function, end_label);
				return dst;
			}
			if (optype == IR_LOR) {
				IrInstruction end_label = { IR_LABEL, .label = { generate_label_name() } };
				// result = 1
				IrVal dst = { IR_VARIABLE, .name = generate_variable_name() };
				IrInstruction store_true = { IR_COPY, .copy = { { IR_CONSTANT, .int_val = 1 }, dst } };
				// evaluate v1
				append_ir_instruction(ir_function, store_true);
				IrVal lhs = emit_ir_expression(exp->binop.lhs, ir_function);
				// JumpIfNotZero(v1, end)
				IrInstruction jump_not_zero_lhs = { IR_JUMP_NOT_ZERO, .jump_not_zero = { lhs, end_label.label.identifier } };
				append_ir_instruction(ir_function, jump_not_zero_lhs);
				// evaluate v2
				IrVal rhs = emit_ir_expression(exp->binop.rhs, ir_function);
				// JumpIfNotZero(v2, end)
				IrInstruction jump_not_zero_rhs = { IR_JUMP_NOT_ZERO, .jump_not_zero = { rhs, end_label.label.identifier } };
				append_ir_instruction(ir_function, jump_not_zero_rhs);
				// return = 0
				IrInstruction store_false = { IR_COPY, .copy = { { IR_CONSTANT, .int_val = 0 }, dst } };
				append_ir_instruction(ir_function, store_false);
				// end
				append_ir_instruction(ir_function, end_label);
				return dst;
			}
			IrVal lhs = emit_ir_expression(exp->binop.lhs, ir_function);
			IrVal rhs = emit_ir_expression(exp->binop.rhs, ir_function);
			IrVal dst = { IR_VARIABLE, .name = generate_variable_name() };
			IrInstruction instruction = {
				IR_BINOP,
				.binop = { optype, lhs, rhs, dst }
			};
			append_ir_instruction(ir_function, instruction);
			return dst;	
		}
		default:
			break;
	}
	fprintf(stderr, "ir: unsupported expression\n");
	exit(1);
}

static void emit_ir_statement(const AstStatement* stmt, IrFunction* ir_function) {
	switch (stmt->kind) {
		case STMT_EXP:
			emit_ir_expression(stmt->exp_stmt.exp, ir_function);
			return;
		case STMT_RETURN: {
			IrVal val = emit_ir_expression(stmt->ret.exp, ir_function);
			IrInstruction ret_instr = { IR_RETURN, .ret = { val } };
			append_ir_instruction(ir_function, ret_instr);
			return;
		}
	}
	fprintf(stderr, "ir: unsupported statement type\n");
	exit(1);
}

static void append_ir_function(IrProgram* program, IrFunction function) {
	program->size++;
	program->functions = realloc(program->functions, program->size * sizeof(IrFunction));
	program->functions[program->size - 1] = function;
}

static IrFunction emit_ir_function(const AstDeclaration* decl) {
	assert(decl->kind == DECL_FUNCTION);
	IrFunction ir_function = {NULL, NULL, 0};
	ir_function.name = strdup(decl->function.name);
	for (int i = 0; i < decl->function.num_stmts; i++) {
		emit_ir_statement(&decl->function.body[i], &ir_function);
	}
	return ir_function;
}

IrProgram emit_ir(const AstProgram* ast_program) {
	IrProgram ir_program = {NULL, 0};
	for (int i = 0; i < ast_program->num_decls; i++) {
		const AstDeclaration* decl = &ast_program->decls[i];
		switch (decl->kind) {
			case DECL_FUNCTION:
				append_ir_function(&ir_program, emit_ir_function(decl));
				break;
			default:
				fprintf(stderr, "ir: unsupported declaration type\n");
				exit(1);
		}
	}
	return ir_program;
}

static void destroy_val(IrVal val) {
    if (val.kind == IR_VARIABLE) {
        free(val.name);
    }
}

void destroy_ir(IrProgram* program){
    for (int fn = 0; fn < program->size; fn++) {
        // iterate over functions
        IrFunction function = program->functions[fn];
        for (int in = 0; in < function.size; in++) {
            // iterate over instructions    
            IrInstruction instruction = function.instructions[in];
            switch (instruction.type) {
                case IR_RETURN:
                    destroy_val(instruction.ret.val);
                    break;
                case IR_UNOP:
                    destroy_val(instruction.unary.dst); 
                    destroy_val(instruction.unary.src);
                    break;
                default:
                    break;
            }
        }
        free(function.name);
        free(function.instructions);
    }
    free(program->functions);
    program->functions = NULL;
}
