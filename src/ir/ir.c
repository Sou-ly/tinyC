#include "ir.h"
#include "../common/ice.h"

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
		// inc/dec are lowered in emit_ir_expression and never reach here.
		case UNOP_PREINC:
		case UNOP_PREDEC:
		case UNOP_POSTINC:
		case UNOP_POSTDEC:
			break;
	}
	ICE("ir: unsupported unary operator");
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
		case BINOP_ASSIGN:	break; // handled separately in EXP_ASSIGN
		case BINOP_CONDITION:	break; // never a binop node; see EXP_CONDITIONAL
	}
	ICE("ir: unsupported binary operator");
}

static IrVal emit_ir_expression(const AstExp* exp, IrFunction* ir_function);

// emits && and ||: both evaluate operands left to right and jump to a
// short-circuit label as soon as one operand decides the result.
// && short-circuits to 0 when an operand is zero, || to 1 when non-zero.
// jumps are built through .as.jump_zero regardless of jump_type, which is
// fine since jump_zero and jump_not_zero have the same layout.
static IrVal emit_ir_short_circuit(const AstExp* exp, IrFunction* ir_function) {
	bool is_and = exp->as.binop.op_type == BINOP_LAND;
	IrInstructionType jump_type = is_and ? IR_JUMP_ZERO : IR_JUMP_NOT_ZERO;
	int short_circuit_value = is_and ? 0 : 1;
	IrInstruction short_circuit_label = { IR_LABEL, .as.label = { generate_label_name() } };
	IrInstruction end_label = { IR_LABEL, .as.label = { generate_label_name() } };
	IrVal dst = { IR_VARIABLE, .as.name = generate_variable_name() };
	// evaluate v1, short-circuit if it decides the result
	IrVal lhs = emit_ir_expression(exp->as.binop.lhs, ir_function);
	IrInstruction jump_lhs = { jump_type, .as.jump_zero = { lhs, short_circuit_label.as.label.identifier } };
	append_ir_instruction(ir_function, jump_lhs);
	// evaluate v2, short-circuit if it decides the result
	IrVal rhs = emit_ir_expression(exp->as.binop.rhs, ir_function);
	IrInstruction jump_rhs = { jump_type, .as.jump_zero = { rhs, short_circuit_label.as.label.identifier } };
	append_ir_instruction(ir_function, jump_rhs);
	// fall-through: result is the opposite of the short-circuit value
	IrInstruction store_fallthrough = { IR_COPY, .as.copy = { { IR_CONSTANT, .as.int_val = !short_circuit_value }, dst } };
	append_ir_instruction(ir_function, store_fallthrough);
	IrInstruction jump_end = { IR_JUMP, .as.jump = { end_label.as.label.identifier } };
	append_ir_instruction(ir_function, jump_end);
	// short-circuit
	append_ir_instruction(ir_function, short_circuit_label);
	IrInstruction store_short_circuit = { IR_COPY, .as.copy = { { IR_CONSTANT, .as.int_val = short_circuit_value }, dst } };
	append_ir_instruction(ir_function, store_short_circuit);
	// end
	append_ir_instruction(ir_function, end_label);
	return dst;
}

static IrVal emit_ir_expression(const AstExp* exp, IrFunction* ir_function) {
	switch (exp->kind) {
		case EXP_INT: {
			IrVal constant = { IR_CONSTANT, .as.int_val = exp->as.int_lit.value };
			return constant;
		}
		case EXP_UNOP: {
			if (exp->as.unary.op_type == UNOP_POSTINC || exp->as.unary.op_type == UNOP_POSTDEC) {
				// postfix: save the old value, mutate in place, yield the saved temp.
				// The parser guarantees the operand is an lvalue (a variable).
				assert(exp->as.unary.operand->kind == EXP_VAR);
				IrVal var = emit_ir_expression(exp->as.unary.operand, ir_function);
				IrBinopType step_op = exp->as.unary.op_type == UNOP_POSTINC ? IR_ADD : IR_SUB;
				IrVal one = { IR_CONSTANT, .as.int_val = 1 };
				IrVal old = { IR_VARIABLE, .as.name = generate_variable_name() };
				IrInstruction save = { IR_COPY, .as.copy = { .src = var, .dst = old } };
				append_ir_instruction(ir_function, save);
				IrInstruction step = { IR_BINOP, .as.binop = { .op = step_op, .lhs = var, .rhs = one, .dst = var } };
				append_ir_instruction(ir_function, step);
				return old;
			} else if (exp->as.unary.op_type == UNOP_PREINC || exp->as.unary.op_type == UNOP_PREDEC) {
				// prefix: mutate in place first, yield the variable itself.
				assert(exp->as.unary.operand->kind == EXP_VAR);
				IrVal var = emit_ir_expression(exp->as.unary.operand, ir_function);
				IrBinopType step_op = exp->as.unary.op_type == UNOP_PREINC ? IR_ADD : IR_SUB;
				IrVal one = { IR_CONSTANT, .as.int_val = 1 };
				IrInstruction step = { IR_BINOP, .as.binop = { .op = step_op, .lhs = var, .rhs = one, .dst = var } };
				append_ir_instruction(ir_function, step);
				return var;
			} else {
				IrVal src = emit_ir_expression(exp->as.unary.operand, ir_function);
				IrVal dst = { IR_VARIABLE, .as.name = generate_variable_name() };
				IrInstruction instruction = {
					IR_UNOP,
					.as.unary = { convert_ir_unary(exp->as.unary.op_type), src, dst }
				};
				append_ir_instruction(ir_function, instruction);
				return dst;
			}
		}
		case EXP_BINOP: {
			IrBinopType optype = convert_ir_binop(exp->as.binop.op_type);
			if (optype == IR_LAND || optype == IR_LOR) {
				return emit_ir_short_circuit(exp, ir_function);
			}
			IrVal lhs = emit_ir_expression(exp->as.binop.lhs, ir_function);
			IrVal rhs = emit_ir_expression(exp->as.binop.rhs, ir_function);
			IrVal dst = { IR_VARIABLE, .as.name = generate_variable_name() };
			IrInstruction instruction = { IR_BINOP, .as.binop = { optype, lhs, rhs, dst } };
			append_ir_instruction(ir_function, instruction);
			return dst;	
		}
		case EXP_VAR: {
			IrVal var = { .kind = IR_VARIABLE, .as.name = strdup(exp->as.variable.identifier) };
			return var;
		}
		case EXP_ASSIGN: {
			assert(exp->as.assign.lhs->kind == EXP_VAR);
			IrVal var = emit_ir_expression(exp->as.assign.lhs, ir_function);
			IrVal result = emit_ir_expression(exp->as.assign.rhs, ir_function);
			IrInstruction instruction;
			switch(exp->as.assign.op) {
				case ASSIGN_NOP: 
					instruction = (IrInstruction) { IR_COPY, .as.copy = { .src = result, .dst = var}}; 
					break;
				case ASSIGN_ADD: 
					instruction = (IrInstruction) { IR_BINOP, .as.binop = { .op = IR_ADD, .lhs = var, .rhs = result, .dst = var }  }; 
					break;
				case ASSIGN_SUB: 
					instruction = (IrInstruction) { IR_BINOP, .as.binop = { .op = IR_SUB, .lhs = var, .rhs = result, .dst = var }  }; 
					break;
				case ASSIGN_MUL: 
					instruction = (IrInstruction) { IR_BINOP, .as.binop = { .op = IR_MUL, .lhs = var, .rhs = result, .dst = var }  }; 
					break;
				case ASSIGN_DIV: 
					instruction = (IrInstruction) { IR_BINOP, .as.binop = { .op = IR_DIV, .lhs = var, .rhs = result, .dst = var }  }; 
					break;
				case ASSIGN_MOD: 
					instruction = (IrInstruction) { IR_BINOP, .as.binop = { .op = IR_MOD, .lhs = var, .rhs = result, .dst = var }  }; 
					break;
				case ASSIGN_AND: 
					instruction = (IrInstruction) { IR_BINOP, .as.binop = { .op = IR_AND, .lhs = var, .rhs = result, .dst = var }  }; 
					break;
				case ASSIGN_OR: 
					instruction = (IrInstruction) { IR_BINOP, .as.binop = { .op = IR_OR, .lhs = var, .rhs = result, .dst = var }  }; 
					break;
				case ASSIGN_XOR: 
					instruction = (IrInstruction) { IR_BINOP, .as.binop = { .op = IR_XOR, .lhs = var, .rhs = result, .dst = var }  }; 
					break;
				case ASSIGN_RSHIFT: 
					instruction = (IrInstruction) { IR_BINOP, .as.binop = { .op = IR_RSHIFT, .lhs = var, .rhs = result, .dst = var }  }; 
					break;
				case ASSIGN_LSHIFT: 
					instruction = (IrInstruction) { IR_BINOP, .as.binop = { .op = IR_LSHIFT, .lhs = var, .rhs = result, .dst = var }  }; 
					break;
			}
			append_ir_instruction(ir_function, instruction);
			return result;
		}
		case EXP_CONDITIONAL: {
			IrVal result = { IR_VARIABLE, .as.name = generate_variable_name() };
			IrVal cond = emit_ir_expression(exp->as.conditional.lhs, ir_function);
			char* false_target = generate_label_name();
			char* end_target = generate_label_name();
			append_ir_instruction(ir_function, (IrInstruction) { IR_JUMP_ZERO, .as.jump_zero = { .cond = cond, .target = false_target } });
			IrVal true_res = emit_ir_expression(exp->as.conditional.mid, ir_function);
			append_ir_instruction(ir_function, (IrInstruction) { IR_COPY, .as.copy = { .src = true_res, .dst = result } });
			append_ir_instruction(ir_function, (IrInstruction) { IR_JUMP, .as.jump = { .target = end_target } });
			append_ir_instruction(ir_function, (IrInstruction) { IR_LABEL, .as.label = { .identifier = false_target } });
			IrVal false_res = emit_ir_expression(exp->as.conditional.rhs, ir_function);
			append_ir_instruction(ir_function, (IrInstruction) { IR_COPY, .as.copy = { .src = false_res, .dst = result } });
			append_ir_instruction(ir_function, (IrInstruction) { IR_LABEL, .as.label = { .identifier = end_target } });
			return result;
		}
		default:
			break;
	}
	ICE("ir: unsupported expression");
}

static void emit_ir_block(IrFunction* ir_function, const AstBlock block);

static void emit_ir_statement(IrFunction* ir_function, const AstStatement* stmt) {
	switch (stmt->kind) {
		case STMT_EXP:
			emit_ir_expression(stmt->as.exp_stmt.exp, ir_function);
			return;
		case STMT_RETURN: {
			IrVal val = emit_ir_expression(stmt->as.ret.exp, ir_function);
			IrInstruction ret_instr = { IR_RETURN, .as.ret = { val } };
			append_ir_instruction(ir_function, ret_instr);
			return;
		}
		case STMT_IF: {
			IrVal cond = emit_ir_expression(stmt->as.if_cond.cond, ir_function);
			char* end_target = generate_label_name();
			if (stmt->as.if_cond.else_br != NULL) {
				char* else_target = generate_label_name();
				append_ir_instruction(ir_function, (IrInstruction) { IR_JUMP_ZERO, .as.jump_zero = { .cond = cond, .target = else_target } });
				emit_ir_statement(ir_function, stmt->as.if_cond.then_br);
				append_ir_instruction(ir_function, (IrInstruction) { IR_JUMP, .as.jump = { .target = end_target } });
				append_ir_instruction(ir_function, (IrInstruction) { IR_LABEL, .as.label = { .identifier = else_target } });
				emit_ir_statement(ir_function, stmt->as.if_cond.else_br);
			} else {
				append_ir_instruction(ir_function, (IrInstruction) { IR_JUMP_ZERO, .as.jump_zero = { .cond = cond, .target = end_target } });
				emit_ir_statement(ir_function, stmt->as.if_cond.then_br);
			}
			append_ir_instruction(ir_function, (IrInstruction) { IR_LABEL, .as.label = { .identifier = end_target } });
			return;
		}
		case STMT_COMPOUND:
			emit_ir_block(ir_function, stmt->as.compound);
			return;
	}
}

static void emit_ir_block_item(IrFunction* ir_function, const AstBlockItem block_item) {
	if (block_item.type == AST_STATEMENT) {
		emit_ir_statement(ir_function, block_item.as.stmt);
	} else if (block_item.type == AST_DECLARATION && block_item.as.decl.exp != NULL) {
		IrVal result = emit_ir_expression(block_item.as.decl.exp, ir_function);
		IrVal var = { IR_VARIABLE, .as.name = strdup(block_item.as.decl.identifier) };
		IrInstruction copy = { IR_COPY, .as.copy = { .src = result, .dst = var } };
		append_ir_instruction(ir_function, copy);
		return;
	}
	return;
}

static void emit_ir_block(IrFunction* ir_function, const AstBlock block) {
	for (size_t i = 0; i < block.size; i++) {
		emit_ir_block_item(ir_function, block.items[i]);
	}
}

static void append_ir_function(IrProgram* program, IrFunction function) {
	program->size++;
	program->functions = realloc(program->functions, program->size * sizeof(IrFunction));
	program->functions[program->size - 1] = function;
}

static IrFunction emit_ir_function(const AstFunction* ast_function) {
	IrFunction ir_function = {NULL, NULL, 0};
	ir_function.name = strdup(ast_function->identifier);
	emit_ir_block(&ir_function, ast_function->body);
	return ir_function;
}

IrProgram emit_ir(const AstProgram* ast_program) {
	IrProgram ir_program = {NULL, 0};
	for (int i = 0; i < ast_program->num_functions; i++) {
		const AstFunction* function = &ast_program->functions[i];
		append_ir_function(&ir_program, emit_ir_function(function));
	}
	return ir_program;
}

static void destroy_val(IrVal val) {
    if (val.kind == IR_VARIABLE) {
        free(val.as.name);
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
                    destroy_val(instruction.as.ret.val);
                    break;
                case IR_UNOP:
                    destroy_val(instruction.as.unary.dst); 
                    destroy_val(instruction.as.unary.src);
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
