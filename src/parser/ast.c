#include "ast.h"
#include <string.h>

// --- Expressions ---

AstExp* create_int_exp(int value) {
    AstExp* e = malloc(sizeof(AstExp));
    e->kind = EXP_INT;
    e->int_lit.value = value;
    return e;
}

AstExp* create_unary_exp(AstUnopType op_type, AstExp* operand) {
    AstExp* e = malloc(sizeof(AstExp));
    e->kind = EXP_UNOP;
    e->unary.op_type = op_type;
    e->unary.operand = operand;
    return e;
}

AstExp* create_variable_exp(const char* identifier) {
    AstExp* e = malloc(sizeof(AstExp));
    e->kind = EXP_VAR;
    e->variable.identifier = strdup(identifier);
    return e;
}

AstExp* create_assign_exp(AstAssignOp op, AstExp* lhs, AstExp* rhs) {
    AstExp* e = malloc(sizeof(AstExp));
    e->kind = EXP_ASSIGN;
	e->assign.op  = op;
    e->assign.lhs = lhs;
    e->assign.rhs = rhs;
    return e;
}

AstExp* create_binop_exp(AstBinopType op_type, AstExp* lhs, AstExp* rhs) {
    AstExp* e = malloc(sizeof(AstExp));
    e->kind = EXP_BINOP;
    e->binop.op_type = op_type;
    e->binop.lhs = lhs;
    e->binop.rhs = rhs;
    return e;
}

void destroy_exp(AstExp* exp) {
    if (!exp) return;
    switch (exp->kind) {
        case EXP_INT:
            break;
        case EXP_UNOP:
            destroy_exp(exp->unary.operand);
            break;
        case EXP_BINOP:
            destroy_exp(exp->binop.lhs);
            destroy_exp(exp->binop.rhs);
            break;
		case EXP_VAR:
			free(exp->variable.identifier);
			break;
		case EXP_ASSIGN:
			destroy_exp(exp->assign.lhs);
			destroy_exp(exp->assign.rhs);
			break;
    }
    free(exp);
}

// --- Statements ---

AstStatement make_return_stmt(AstExp* exp) {
    return (AstStatement){ .kind = STMT_RETURN, .ret = { exp } };
}

AstStatement make_exp_stmt(AstExp* exp) {
    return (AstStatement){ .kind = STMT_EXP, .exp_stmt = { exp } };
}

void destroy_stmt(AstStatement* stmt) {
    switch (stmt->kind) {
        case STMT_RETURN:
            destroy_exp(stmt->ret.exp);
            break;
        case STMT_EXP:
            destroy_exp(stmt->exp_stmt.exp);
            break;
    }
}

AstFunction ast_function_make(const char* name, size_t capacity) {
	AstFunction function;
	function.identifier = strdup(name);
	function.size = 0;
	function.capacity = capacity;
	function.body = malloc(capacity * sizeof(AstBlockItem));
	return function;
}

void ast_function_append(AstFunction* function, AstBlockItem block_item) {
	if (function->size >= function->capacity) {
		function->capacity *= 2;
		function->body = realloc(function->body, function->capacity * sizeof(AstBlockItem));
	}
	function->body[function->size++] = block_item;
	return;
}

void ast_function_destroy(AstFunction* function) {
	free(function->identifier);
	for (size_t i = 0; i < function->size; i++) {
		AstBlockItem* block_item = function->body+i;
		switch (block_item->type) {
			case AST_DECLARATION:
				free(block_item->decl.identifier);
				destroy_exp(block_item->decl.exp);
				break;
			case AST_STATEMENT:
				destroy_stmt(&block_item->stmt);
				break;
		}
	}
	free(function->body);
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
