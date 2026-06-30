#include "ast.h"
#include <string.h>

// --- Expressions ---

AstExp* create_int_exp(int value) {
    AstExp* e = malloc(sizeof(AstExp));
    e->kind = EXP_INT;
    e->as.int_lit.value = value;
    return e;
}

AstExp* create_unary_exp(AstUnopType op_type, AstExp* operand) {
    AstExp* e = malloc(sizeof(AstExp));
    e->kind = EXP_UNOP;
    e->as.unary.op_type = op_type;
    e->as.unary.operand = operand;
    return e;
}

AstExp* create_variable_exp(const char* identifier) {
    AstExp* e = malloc(sizeof(AstExp));
    e->kind = EXP_VAR;
    e->as.variable.identifier = strdup(identifier);
    return e;
}

AstExp* create_assign_exp(AstAssignOp op, AstExp* lhs, AstExp* rhs) {
    AstExp* e = malloc(sizeof(AstExp));
    e->kind = EXP_ASSIGN;
	e->as.assign.op  = op;
    e->as.assign.lhs = lhs;
    e->as.assign.rhs = rhs;
    return e;
}

AstExp* create_binop_exp(AstBinopType op_type, AstExp* lhs, AstExp* rhs) {
    AstExp* e = malloc(sizeof(AstExp));
    e->kind = EXP_BINOP;
    e->as.binop.op_type = op_type;
    e->as.binop.lhs = lhs;
    e->as.binop.rhs = rhs;
    return e;
}

AstExp* create_conditional_exp(AstExp* lhs, AstExp* mid, AstExp* rhs) {
    AstExp* e = malloc(sizeof(AstExp));
    e->kind = EXP_CONDITIONAL;
    e->as.conditional.lhs = lhs;
    e->as.conditional.mid = mid;
    e->as.conditional.rhs = rhs;
    return e;
}

AstBlock ast_block_make(size_t capacity) {
	AstBlock block;
	block.size = 0;
	block.capacity = capacity;
	block.items = malloc(capacity * sizeof(AstBlockItem));
	return block;
}

void ast_block_append(AstBlock* block, AstBlockItem block_item) {
	if (block->size >= block->capacity) {
		block->capacity *= 2;
		block->items = realloc(block->items, block->capacity * sizeof(AstBlockItem));
	}
	block->items[block->size++] = block_item;
	return;
}

void ast_block_destroy(AstBlock* block) {
	for (size_t i = 0; i < block->size; i++) {
		AstBlockItem* block_item = block->items+i;
		switch (block_item->type) {
			case AST_DECLARATION:
				free(block_item->as.decl.identifier);
				destroy_exp(block_item->as.decl.exp);
				break;
			case AST_STATEMENT:
				destroy_stmt(&block_item->as.stmt);
				break;
		}
	}
	free(block->items);
	return;
}

void destroy_exp(AstExp* exp) {
    if (!exp) return;
    switch (exp->kind) {
        case EXP_INT:
            break;
        case EXP_UNOP:
            destroy_exp(exp->as.unary.operand);
            break;
        case EXP_BINOP:
            destroy_exp(exp->as.binop.lhs);
            destroy_exp(exp->as.binop.rhs);
            break;
		case EXP_VAR:
			free(exp->as.variable.identifier);
			break;
		case EXP_ASSIGN:
			destroy_exp(exp->as.assign.lhs);
			destroy_exp(exp->as.assign.rhs);
			break;
		case EXP_CONDITIONAL:
			destroy_exp(exp->as.conditional.lhs);
			destroy_exp(exp->as.conditional.mid);
			destroy_exp(exp->as.conditional.rhs);
    }
    free(exp);
}

// --- Statements ---

AstStatement make_return_stmt(AstExp* exp) {
    return (AstStatement){ .kind = STMT_RETURN, .as.ret = { exp } };
}

AstStatement make_exp_stmt(AstExp* exp) {
    return (AstStatement){ .kind = STMT_EXP, .as.exp_stmt = { exp } };
}

// then_br and else_br are heap-owned; else_br may be NULL (a plain `if`).
AstStatement make_if_stmt(AstExp* cond, AstStatement* then_br, AstStatement* else_br) {
    return (AstStatement){ .kind = STMT_IF, .as.if_cond = { cond, then_br, else_br } };
}

AstStatement make_compound_stmt(AstBlock block) {
	return (AstStatement) {.kind=STMT_COMPOUND, .as.compound=block};
}

void destroy_stmt(AstStatement* stmt) {
    switch (stmt->kind) {
        case STMT_RETURN:
            destroy_exp(stmt->as.ret.exp);
            break;
        case STMT_EXP:
            destroy_exp(stmt->as.exp_stmt.exp);
            break;
        case STMT_IF:
            destroy_exp(stmt->as.if_cond.cond);
            destroy_stmt(stmt->as.if_cond.then_br);
            free(stmt->as.if_cond.then_br);
            if (stmt->as.if_cond.else_br != NULL) {
                destroy_stmt(stmt->as.if_cond.else_br);
                free(stmt->as.if_cond.else_br);
            }
            break;
        case STMT_COMPOUND:
            ast_block_destroy(&stmt->as.compound);
            break;
    }
}

AstFunction ast_function_make(const char* name, AstBlock block) {
	AstFunction function;
	function.identifier = strdup(name);
	function.body = block;
	return function;
}

void ast_function_append(AstFunction* function, AstBlockItem block_item) {
	ast_block_append(&function->body, block_item);
}

void ast_function_destroy(AstFunction* function) {
	free(function->identifier);
	ast_block_destroy(&function->body);
	return;
}

// --- Program ---

AstProgram ast_program_create(AstFunction* functions, int num_functions) {
	return (AstProgram) {.functions = functions, .num_functions = num_functions};	
}

void destroy_program(AstProgram* program) {
    for (int i = 0; i < program->num_functions; i++) {
        ast_function_destroy(&program->functions[i]);
    }
    free(program->functions);
}
