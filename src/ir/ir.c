#include "ir.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>


static int VAR_NAME_COUNTER = 0;

static char *generate_variable_name(const char *prefix) {
	int len = snprintf(NULL, 0, "%s.%d", prefix, VAR_NAME_COUNTER);
	char *name = (char *)malloc(len + 1);
	if (!name) return NULL;
	snprintf(name, len + 1, "%s.%d", prefix, VAR_NAME_COUNTER++);
	return name;
}

static char *generate_temp_name(void) {
	return generate_variable_name("var");
}

static void append_ir_instruction(IrFunction* function, IrInstruction instruction) {
	function->size++;
	function->instructions = realloc(function->instructions, function->size * sizeof(IrInstruction));
	function->instructions[function->size - 1] = instruction;
}

static IrUnopType convert_ir_unary(AstUnopType ast_op) {
	switch (ast_op) {
		case UNOP_NOT:		return IR_COMP;
		case UNOP_MINUS:	return IR_NEG;
	}
	fprintf(stderr, "ir: unsupported unary operator\n");
	exit(1);
}

static IrVal emit_ir_expession(const AstExp* exp, IrFunction* ir_function) {
	switch (exp->kind) {
		case EXP_INT: {
			IrVal constant = { IR_CONSTANT, .int_val = exp->int_lit.value };
			return constant;
		}
		case EXP_UNOP: {
			IrVal src = emit_ir_expession(exp->unary.operand, ir_function);
			IrVal dst = { IR_VARIABLE, .name = generate_temp_name() };
			IrInstruction instruction = {
				IR_UNOP,
				.unary = { convert_ir_unary(exp->unary.op_type), src, dst }
			};
			append_ir_instruction(ir_function, instruction);
			return dst;
		}
		default:
			break;
	}
	fprintf(stderr, "ir: unsupported expession\n");
	exit(1);
}

static void emit_ir_statement(const AstStatement* stmt, IrFunction* ir_function) {
	switch (stmt->kind) {
		case STMT_EXP:
			emit_ir_expession(stmt->exp_stmt.exp, ir_function);
			return;
		case STMT_RETURN: {
			IrVal val = emit_ir_expession(stmt->ret.exp, ir_function);
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
