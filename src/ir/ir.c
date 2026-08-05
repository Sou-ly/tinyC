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
	list_push(&function->instructions, instruction);
}

// --- IR constructors ---
// See ir.h for the ownership convention: string fields are stored by pointer.

IrVal ir_val_constant(int value) {
	return (IrVal){ .kind = IR_CONSTANT, .as.int_val = value };
}

IrVal ir_val_variable(char* identifier) {
	return (IrVal){ .kind = IR_VARIABLE, .as.identifier = identifier };
}

IrInstruction ir_instr_return(IrVal val) {
	return (IrInstruction){ IR_RETURN, .as.ret = { val } };
}

IrInstruction ir_instr_unary(IrUnopType op, IrVal src, IrVal dst) {
	return (IrInstruction){ IR_UNOP, .as.unary = { op, src, dst } };
}

IrInstruction ir_instr_binop(IrBinopType op, IrVal lhs, IrVal rhs, IrVal dst) {
	return (IrInstruction){ IR_BINOP, .as.binop = { op, lhs, rhs, dst } };
}

IrInstruction ir_instr_copy(IrVal src, IrVal dst) {
	return (IrInstruction){ IR_COPY, .as.copy = { src, dst } };
}

IrInstruction ir_instr_jump(char* target) {
	return (IrInstruction){ IR_JUMP, .as.jump = { target } };
}

IrInstruction ir_instr_jump_zero(IrVal cond, char* target) {
	return (IrInstruction){ IR_JUMP_ZERO, .as.jump_zero = { cond, target } };
}

IrInstruction ir_instr_jump_not_zero(IrVal cond, char* target) {
	return (IrInstruction){ IR_JUMP_NOT_ZERO, .as.jump_not_zero = { cond, target } };
}

IrInstruction ir_instr_label(char* identifier) {
	return (IrInstruction){ IR_LABEL, .as.label = { identifier } };
}

// Adopts `args` (the list's backing array) along with the identifier.
IrInstruction ir_instr_function_call(char* identifier, IrValList args, IrVal dst) {
	return (IrInstruction){ IR_FUNCALL, .as.funcall = { identifier, args, dst } };
}

// A fresh variable value backed by a newly generated temp name.
static IrVal new_temp(void) {
	return ir_val_variable(generate_variable_name());
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

static IrBinopType compound_assign_op(AstAssignOp op) {
	switch (op) {
		case ASSIGN_ADD:	return IR_ADD;
		case ASSIGN_SUB:	return IR_SUB;
		case ASSIGN_MUL:	return IR_MUL;
		case ASSIGN_DIV:	return IR_DIV;
		case ASSIGN_MOD:	return IR_MOD;
		case ASSIGN_AND:	return IR_AND;
		case ASSIGN_OR:		return IR_OR;
		case ASSIGN_XOR:	return IR_XOR;
		case ASSIGN_RSHIFT:	return IR_RSHIFT;
		case ASSIGN_LSHIFT:	return IR_LSHIFT;
		case ASSIGN_NOP:	break; // plain copy, handled in EXP_ASSIGN
	}
	ICE("ir: unsupported compound assignment operator");
}

static IrVal emit_ir_expression(const AstExp* exp, IrFunction* ir_function);

static IrVal emit_ir_short_circuit(const AstExp* exp, IrFunction* ir_function) {
	bool is_and = exp->as.binop.op_type == BINOP_LAND;
	int short_circuit_value = is_and ? 0 : 1;
	char* short_circuit_target = generate_label_name();
	char* end_target = generate_label_name();
	IrVal dst = new_temp();
	// evaluate v1, short-circuit if it decides the result. jump_zero and
	// jump_not_zero share a layout, so the right constructor is picked by is_and.
	IrVal lhs = emit_ir_expression(exp->as.binop.lhs, ir_function);
	append_ir_instruction(ir_function, is_and ? ir_instr_jump_zero(lhs, short_circuit_target)
											  : ir_instr_jump_not_zero(lhs, short_circuit_target));
	// evaluate v2, short-circuit if it decides the result
	IrVal rhs = emit_ir_expression(exp->as.binop.rhs, ir_function);
	append_ir_instruction(ir_function, is_and ? ir_instr_jump_zero(rhs, short_circuit_target)
											  : ir_instr_jump_not_zero(rhs, short_circuit_target));
	// fall-through: result is the opposite of the short-circuit value
	append_ir_instruction(ir_function, ir_instr_copy(ir_val_constant(!short_circuit_value), dst));
	append_ir_instruction(ir_function, ir_instr_jump(end_target));
	// short-circuit
	append_ir_instruction(ir_function, ir_instr_label(short_circuit_target));
	append_ir_instruction(ir_function, ir_instr_copy(ir_val_constant(short_circuit_value), dst));
	// end
	append_ir_instruction(ir_function, ir_instr_label(end_target));
	return dst;
}

static IrVal emit_ir_expression(const AstExp* exp, IrFunction* ir_function) {
	switch (exp->kind) {
		case EXP_INT:
			return ir_val_constant(exp->as.int_lit.value);
		case EXP_UNOP: {
			if (exp->as.unary.op_type == UNOP_POSTINC || exp->as.unary.op_type == UNOP_POSTDEC) {
				// postfix: save the old value, mutate in place, yield the saved temp.
				// The parser guarantees the operand is an lvalue (a variable).
				assert(exp->as.unary.operand->kind == EXP_VAR);
				IrVal var = emit_ir_expression(exp->as.unary.operand, ir_function);
				IrBinopType step_op = exp->as.unary.op_type == UNOP_POSTINC ? IR_ADD : IR_SUB;
				IrVal old = new_temp();
				append_ir_instruction(ir_function, ir_instr_copy(var, old));
				append_ir_instruction(ir_function, ir_instr_binop(step_op, var, ir_val_constant(1), var));
				return old;
			} else if (exp->as.unary.op_type == UNOP_PREINC || exp->as.unary.op_type == UNOP_PREDEC) {
				// prefix: mutate in place first, yield the variable itself.
				assert(exp->as.unary.operand->kind == EXP_VAR);
				IrVal var = emit_ir_expression(exp->as.unary.operand, ir_function);
				IrBinopType step_op = exp->as.unary.op_type == UNOP_PREINC ? IR_ADD : IR_SUB;
				append_ir_instruction(ir_function, ir_instr_binop(step_op, var, ir_val_constant(1), var));
				return var;
			} else {
				IrVal src = emit_ir_expression(exp->as.unary.operand, ir_function);
				IrVal dst = new_temp();
				append_ir_instruction(ir_function, ir_instr_unary(convert_ir_unary(exp->as.unary.op_type), src, dst));
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
			IrVal dst = new_temp();
			append_ir_instruction(ir_function, ir_instr_binop(optype, lhs, rhs, dst));
			return dst;
		}
		case EXP_VAR:
			return ir_val_variable(strdup(exp->as.variable.identifier));
		case EXP_ASSIGN: {
			assert(exp->as.assign.lhs->kind == EXP_VAR);
			IrVal var = emit_ir_expression(exp->as.assign.lhs, ir_function);
			IrVal result = emit_ir_expression(exp->as.assign.rhs, ir_function);
			// `x op= e` reads x, combines with e, and writes back into x; plain
			// `x = e` is just a copy. compound_assign_op maps the rest to IR ops.
			if (exp->as.assign.op == ASSIGN_NOP) {
				append_ir_instruction(ir_function, ir_instr_copy(result, var));
			} else {
				append_ir_instruction(ir_function, ir_instr_binop(compound_assign_op(exp->as.assign.op), var, result, var));
			}
			return result;
		}
		case EXP_CONDITIONAL: {
			IrVal result = new_temp();
			IrVal cond = emit_ir_expression(exp->as.conditional.lhs, ir_function);
			char* false_target = generate_label_name();
			char* end_target = generate_label_name();
			append_ir_instruction(ir_function, ir_instr_jump_zero(cond, false_target));
			IrVal true_res = emit_ir_expression(exp->as.conditional.mid, ir_function);
			append_ir_instruction(ir_function, ir_instr_copy(true_res, result));
			append_ir_instruction(ir_function, ir_instr_jump(end_target));
			append_ir_instruction(ir_function, ir_instr_label(false_target));
			IrVal false_res = emit_ir_expression(exp->as.conditional.rhs, ir_function);
			append_ir_instruction(ir_function, ir_instr_copy(false_res, result));
			append_ir_instruction(ir_function, ir_instr_label(end_target));
			return result;
		}
		case EXP_FUNCTION_CALL: {
			const AstExpFunctionCall* call = &exp->as.funcall;
			IrValList args = {0};
			for (size_t i = 0; i < call->args.count; i++) {
				list_push(&args, emit_ir_expression(&call->args.items[i], ir_function));
			}
			IrVal dst = new_temp();
			append_ir_instruction(ir_function, ir_instr_function_call(strdup(call->identifier), args, dst));
			return dst;
		}
		default:
			break;
	}
	ICE("ir: unsupported expression");
}

static void emit_ir_block(IrFunction* ir_function, const AstBlock block);

static char* loop_label(const char* base, const char* suffix) {
	int len = snprintf(NULL, 0, "%s_%s", base, suffix);
	char* name = malloc(len + 1);
	snprintf(name, len + 1, "%s_%s", base, suffix);
	return name;
}

static char* switch_clause_label(const char* base, size_t index) {
	int len = snprintf(NULL, 0, "%s_clause_%zu", base, index);
	char* name = malloc(len + 1);
	snprintf(name, len + 1, "%s_clause_%zu", base, index);
	return name;
}

static void emit_ir_declaration(IrFunction* ir_function, const AstVariableDeclaration* decl) {
	if (!decl->init) return;
	IrVal result = emit_ir_expression(decl->init, ir_function);
	append_ir_instruction(ir_function, ir_instr_copy(result, ir_val_variable(strdup(decl->identifier))));
}

static void emit_ir_statement(IrFunction* ir_function, const AstStatement* stmt) {
	switch (stmt->kind) {
		case STMT_EXP:
			emit_ir_expression(stmt->as.exp_stmt.exp, ir_function);
			return;
		case STMT_RETURN: {
			IrVal val = emit_ir_expression(stmt->as.ret.exp, ir_function);
			append_ir_instruction(ir_function, ir_instr_return(val));
			return;
		}
		case STMT_IF: {
			IrVal cond = emit_ir_expression(stmt->as.if_cond.cond, ir_function);
			char* end_target = generate_label_name();
			if (stmt->as.if_cond.else_br != NULL) {
				char* else_target = generate_label_name();
				append_ir_instruction(ir_function, ir_instr_jump_zero(cond, else_target));
				emit_ir_statement(ir_function, stmt->as.if_cond.then_br);
				append_ir_instruction(ir_function, ir_instr_jump(end_target));
				append_ir_instruction(ir_function, ir_instr_label(else_target));
				emit_ir_statement(ir_function, stmt->as.if_cond.else_br);
			} else {
				append_ir_instruction(ir_function, ir_instr_jump_zero(cond, end_target));
				emit_ir_statement(ir_function, stmt->as.if_cond.then_br);
			}
			append_ir_instruction(ir_function, ir_instr_label(end_target));
			return;
		}
		case STMT_COMPOUND:
			emit_ir_block(ir_function, stmt->as.compound);
			return;
		case STMT_FOR: {
			// init; start: if(!cond) goto break; body; continue: post; goto start; break:
			const AstStmtFor* loop = &stmt->as.for_loop;
			char* start = loop_label(loop->label, "start");
			char* cont = loop_label(loop->label, "continue");
			char* brk = loop_label(loop->label, "break");
			switch (loop->init.kind) {
				case AST_INIT_DECL:	emit_ir_declaration(ir_function, &loop->init.as.decl); break;
				case AST_INIT_EXP:	if (loop->init.as.exp) emit_ir_expression(loop->init.as.exp, ir_function); break;
			}
			append_ir_instruction(ir_function, ir_instr_label(start));
			if (loop->cond) {
				IrVal cond = emit_ir_expression(loop->cond, ir_function);
				append_ir_instruction(ir_function, ir_instr_jump_zero(cond, brk));
			}
			emit_ir_statement(ir_function, loop->body);
			append_ir_instruction(ir_function, ir_instr_label(cont));
			if (loop->post) emit_ir_expression(loop->post, ir_function);
			append_ir_instruction(ir_function, ir_instr_jump(start));
			append_ir_instruction(ir_function, ir_instr_label(brk));
			return;
		}
		case STMT_WHILE: {
			// continue: if(!cond) goto break; body; goto continue; break:
			const AstStmtWhile* loop = &stmt->as.while_loop;
			char* cont = loop_label(loop->label, "continue");
			char* brk = loop_label(loop->label, "break");
			append_ir_instruction(ir_function, ir_instr_label(cont));
			IrVal cond = emit_ir_expression(loop->cond, ir_function);
			append_ir_instruction(ir_function, ir_instr_jump_zero(cond, brk));
			emit_ir_statement(ir_function, loop->body);
			append_ir_instruction(ir_function, ir_instr_jump(cont));
			append_ir_instruction(ir_function, ir_instr_label(brk));
			return;
		}
		case STMT_DO_WHILE: {
			// start: body; continue: if(cond) goto start; break:
			const AstStmtDoWhile* loop = &stmt->as.do_while_loop;
			char* start = loop_label(loop->label, "start");
			char* cont = loop_label(loop->label, "continue");
			char* brk = loop_label(loop->label, "break");
			append_ir_instruction(ir_function, ir_instr_label(start));
			emit_ir_statement(ir_function, loop->body);
			append_ir_instruction(ir_function, ir_instr_label(cont));
			IrVal cond = emit_ir_expression(loop->cond, ir_function);
			append_ir_instruction(ir_function, ir_instr_jump_not_zero(cond, start));
			append_ir_instruction(ir_function, ir_instr_label(brk));
			return;
		}
		case STMT_BREAK:
			append_ir_instruction(ir_function, ir_instr_jump(loop_label(stmt->as.break_stmt.label, "break")));
			return;
		case STMT_CONTINUE:
			append_ir_instruction(ir_function, ir_instr_jump(loop_label(stmt->as.continue_stmt.label, "continue")));
			return;
		case STMT_SWITCH: {
			const AstStmtSwitch* sw = &stmt->as.switch_stmt;
			IrVal cond = emit_ir_expression(sw->cond, ir_function);
			size_t default_index = sw->clauses.count; // sentinel: no default
			for (size_t i = 0; i < sw->clauses.count; i++) {
				if (sw->clauses.items[i].is_default) { default_index = i; continue; }
				IrVal matched = new_temp();
				append_ir_instruction(ir_function,
					ir_instr_binop(IR_EQ, cond, ir_val_constant(sw->clauses.items[i].value), matched));
				append_ir_instruction(ir_function,
					ir_instr_jump_not_zero(matched, switch_clause_label(sw->label, i)));
			}
			if (default_index != sw->clauses.count)
				append_ir_instruction(ir_function, ir_instr_jump(switch_clause_label(sw->label, default_index)));
			else
				append_ir_instruction(ir_function, ir_instr_jump(loop_label(sw->label, "break")));
			for (size_t i = 0; i < sw->clauses.count; i++) {
				append_ir_instruction(ir_function, ir_instr_label(switch_clause_label(sw->label, i)));
				emit_ir_block(ir_function, sw->clauses.items[i].body);
			}
			append_ir_instruction(ir_function, ir_instr_label(loop_label(sw->label, "break")));
			return;
		}
		case STMT_LABEL:
			append_ir_instruction(ir_function, ir_instr_label(strdup(stmt->as.label.identifier)));
			return;
		case STMT_GOTO:
			append_ir_instruction(ir_function, ir_instr_jump(strdup(stmt->as.goto_stmt.target)));
			return;
	}
}

static void emit_ir_block_item(IrFunction* ir_function, const AstBlockItem block_item) {
	if (block_item.kind == AST_STATEMENT) {
		emit_ir_statement(ir_function, block_item.as.statement);
	} else if (block_item.kind == AST_DECLARATION) {
		// a block-scope function declaration is a prototype: nothing to lower.
		if (block_item.as.declaration.kind == DECL_VAR)
			emit_ir_declaration(ir_function, &block_item.as.declaration.as.variable);
	}
}

static void emit_ir_block(IrFunction* ir_function, const AstBlock block) {
	for (size_t i = 0; i < block.count; i++) {
		emit_ir_block_item(ir_function, block.items[i]);
	}
}

static void append_ir_function(IrProgram* program, IrFunction function) {
	list_push(program, function);
}

static IrFunction emit_ir_function(const AstFunctionDeclaration* ast_function) {
	IrFunction ir_function = { .identifier = NULL, .params = {0}, .instructions = {0} };
	ir_function.identifier = strdup(ast_function->identifier);
	for (size_t i = 0; i < ast_function->params.count; i++) {
		list_push(&ir_function.params, strdup(ast_function->params.items[i]));
	}
	emit_ir_block(&ir_function, ast_function->body.value);
	return ir_function;
}

IrProgram emit_ir(const AstProgram* ast_program) {
	IrProgram ir_program = {0};
	for (size_t i = 0; i < ast_program->count; i++) {
		const AstDeclaration* decl = &ast_program->items[i];
		// TODO: file-scope variables need static storage (.data/.bss) emission;
		// until then only functions are lowered.
		if (decl->kind != DECL_FUNC) continue;
		const AstFunctionDeclaration* function = &decl->as.function;
		if (!function->body.present) continue;	// prototype: nothing to lower
		append_ir_function(&ir_program, emit_ir_function(function));
	}
	return ir_program;
}

static void destroy_val(IrVal val) {
    if (val.kind == IR_VARIABLE) {
        free(val.as.identifier);
    }
}

void ir_program_destroy(IrProgram* program){
    for (size_t fn = 0; fn < program->count; fn++) {
        // iterate over functions
        IrFunction function = program->items[fn];
        for (size_t in = 0; in < function.instructions.count; in++) {
            // iterate over instructions
            IrInstruction instruction = function.instructions.items[in];
            switch (instruction.kind) {
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
        free(function.identifier);
        list_free(&function.instructions);
    }
    list_free(program);
}
