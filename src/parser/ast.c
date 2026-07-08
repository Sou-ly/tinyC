#include "ast.h"
#include <string.h>

// --- Expressions ---

AstExp* ast_exp_int(int value) {
    AstExp* e = malloc(sizeof(AstExp));
    e->kind = EXP_INT;
    e->as.int_lit.value = value;
    return e;
}

AstExp* ast_exp_unary(AstUnopType op_type, AstExp* operand) {
    AstExp* e = malloc(sizeof(AstExp));
    e->kind = EXP_UNOP;
    e->as.unary.op_type = op_type;
    e->as.unary.operand = operand;
    return e;
}

AstExp* ast_exp_var(const char* identifier) {
    AstExp* e = malloc(sizeof(AstExp));
    e->kind = EXP_VAR;
    e->as.variable.identifier = strdup(identifier);
    return e;
}

AstExp* ast_exp_assign(AstAssignOp op, AstExp* lhs, AstExp* rhs) {
    AstExp* e = malloc(sizeof(AstExp));
    e->kind = EXP_ASSIGN;
	e->as.assign.op  = op;
    e->as.assign.lhs = lhs;
    e->as.assign.rhs = rhs;
    return e;
}

AstExp* ast_exp_binop(AstBinopType op_type, AstExp* lhs, AstExp* rhs) {
    AstExp* e = malloc(sizeof(AstExp));
    e->kind = EXP_BINOP;
    e->as.binop.op_type = op_type;
    e->as.binop.lhs = lhs;
    e->as.binop.rhs = rhs;
    return e;
}

AstExp* ast_exp_conditional(AstExp* lhs, AstExp* mid, AstExp* rhs) {
    AstExp* e = malloc(sizeof(AstExp));
    e->kind = EXP_CONDITIONAL;
    e->as.conditional.lhs = lhs;
    e->as.conditional.mid = mid;
    e->as.conditional.rhs = rhs;
    return e;
}

void ast_block_append(AstBlock* block, AstBlockItem block_item) {
	list_push(block, block_item);
}

void ast_block_destroy(AstBlock* block) {
	for (size_t i = 0; i < block->count; i++) {
		AstBlockItem* block_item = block->items+i;
		switch (block_item->kind) {
			case AST_DECLARATION:
				free(block_item->as.decl.identifier);
				if (block_item->as.decl.init.present)
					ast_exp_destroy(block_item->as.decl.init.value);
				break;
			case AST_STATEMENT:
				ast_stmt_destroy(block_item->as.stmt);
				break;
		}
	}
	free(block->items);
	return;
}

void ast_exp_destroy(AstExp* exp) {
    if (!exp) return;
    switch (exp->kind) {
        case EXP_INT:
            break;
        case EXP_UNOP:
            ast_exp_destroy(exp->as.unary.operand);
            break;
        case EXP_BINOP:
            ast_exp_destroy(exp->as.binop.lhs);
            ast_exp_destroy(exp->as.binop.rhs);
            break;
		case EXP_VAR:
			free(exp->as.variable.identifier);
			break;
		case EXP_ASSIGN:
			ast_exp_destroy(exp->as.assign.lhs);
			ast_exp_destroy(exp->as.assign.rhs);
			break;
		case EXP_CONDITIONAL:
			ast_exp_destroy(exp->as.conditional.lhs);
			ast_exp_destroy(exp->as.conditional.mid);
			ast_exp_destroy(exp->as.conditional.rhs);
    }
    free(exp);
}

// --- Statements ---

// All ast_stmt_* constructors heap-allocate the node and return an owning
// pointer, mirroring ast_exp_*. Nested statements (if branches, loop
// bodies) are likewise pointers, so the whole statement tree is uniform;
// ast_stmt_destroy frees a node and everything it owns, including itself.
static AstStatement* alloc_stmt(AstStatement value) {
    AstStatement* stmt = malloc(sizeof(AstStatement));
    *stmt = value;
    return stmt;
}

AstStatement* ast_stmt_return(AstExp* exp) {
    return alloc_stmt((AstStatement){ .kind = STMT_RETURN, .as.ret = { exp } });
}

AstStatement* ast_stmt_exp(AstExp* exp) {
    return alloc_stmt((AstStatement){ .kind = STMT_EXP, .as.exp_stmt = { exp } });
}

// then_br and else_br are heap-owned; else_br may be NULL (a plain `if`).
AstStatement* ast_stmt_if(AstExp* cond, AstStatement* then_br, AstStatement* else_br) {
    return alloc_stmt((AstStatement){ .kind = STMT_IF, .as.if_cond = { .cond = cond, .then_br = then_br, .else_br = else_br } });
}

AstStatement* ast_stmt_compound(AstBlock block) {
	return alloc_stmt((AstStatement) {.kind=STMT_COMPOUND, .as.compound=block});
}

AstForInit ast_for_init_decl(AstVarDecl decl) {
	return (AstForInit){ .kind = AST_INIT_DECL, .as.decl = decl };
}

AstForInit ast_for_init_exp(AstExp* exp) {
	return (AstForInit){ .kind = AST_INIT_EXP, .as.exp = exp };
}

// body is heap-owned. label is left NULL here and filled in by the loop-labelling pass.
AstStatement* ast_stmt_for(AstForInit init, OptionalExp cond, OptionalExp post, AstStatement* body) {
	return alloc_stmt((AstStatement){ .kind = STMT_FOR, .as.for_loop = { .label = NULL, .init = init, .cond = cond, .post = post, .body = body } });
}

// body is heap-owned; label filled in later by the loop-labelling pass.
AstStatement* ast_stmt_while(AstExp* cond, AstStatement* body) {
	return alloc_stmt((AstStatement){ .kind = STMT_WHILE, .as.while_loop = { .label = NULL, .cond = cond, .body = body } });
}

// body is heap-owned; label filled in later by the loop-labelling pass.
AstStatement* ast_stmt_do_while(AstExp* cond, AstStatement* body) {
	return alloc_stmt((AstStatement){ .kind = STMT_DO_WHILE, .as.do_while_loop = { .label = NULL, .cond = cond, .body = body } });
}

// label is NULL until the loop-labelling pass fills it in; stored directly
// (ast_stmt_destroy frees it, and free(NULL) is a no-op).
AstStatement* ast_stmt_break(char* label) {
	return alloc_stmt((AstStatement){ .kind = STMT_BREAK, .as.break_stmt = { .label = label } });
}

AstStatement* ast_stmt_continue(char* label) {
	return alloc_stmt((AstStatement){ .kind = STMT_CONTINUE, .as.continue_stmt = { .label = label } });
}

// identifier / target are heap-owned (the parser strdup's them), and freed by
// ast_stmt_destroy. A label names a point in the code; goto jumps to that name.
AstStatement* ast_stmt_label(char* identifier) {
	return alloc_stmt((AstStatement){ .kind = STMT_LABEL, .as.label = { .identifier = identifier } });
}

AstStatement* ast_stmt_goto(char* target) {
	return alloc_stmt((AstStatement){ .kind = STMT_GOTO, .as.goto_stmt = { .target = target } });
}

// cond is heap-owned and the clauses list's storage is adopted (the parser
// builds it). label is left NULL for the labelling pass to fill in (base for
// break/clause labels).
AstStatement* ast_stmt_switch(AstExp* cond, AstClauseList clauses) {
	return alloc_stmt((AstStatement){ .kind = STMT_SWITCH, .as.switch_stmt = {
		.label = NULL, .cond = cond, .clauses = clauses } });
}

// Frees the statement and everything it owns, including the node itself.
// Nested statements are heap-owned pointers, so ast_stmt_destroy recurses into
// them directly (no separate free at the call site). NULL-safe.
void ast_stmt_destroy(AstStatement* stmt) {
    if (!stmt) return;
    switch (stmt->kind) {
        case STMT_RETURN:
            ast_exp_destroy(stmt->as.ret.exp);
            break;
        case STMT_EXP:
            ast_exp_destroy(stmt->as.exp_stmt.exp);
            break;
        case STMT_IF:
            ast_exp_destroy(stmt->as.if_cond.cond);
            ast_stmt_destroy(stmt->as.if_cond.then_br);
            ast_stmt_destroy(stmt->as.if_cond.else_br);
            break;
        case STMT_COMPOUND:
            ast_block_destroy(&stmt->as.compound);
            break;
        case STMT_FOR:
            free(stmt->as.for_loop.label);
            switch (stmt->as.for_loop.init.kind) {
                case AST_INIT_DECL:
                    free(stmt->as.for_loop.init.as.decl.identifier);
                    if (stmt->as.for_loop.init.as.decl.init.present)
                        ast_exp_destroy(stmt->as.for_loop.init.as.decl.init.value);
                    break;
                case AST_INIT_EXP:
                    ast_exp_destroy(stmt->as.for_loop.init.as.exp);
                    break;
            }
            if (stmt->as.for_loop.cond.present)
                ast_exp_destroy(stmt->as.for_loop.cond.value);
            if (stmt->as.for_loop.post.present)
                ast_exp_destroy(stmt->as.for_loop.post.value);
            ast_stmt_destroy(stmt->as.for_loop.body);
            break;
        case STMT_WHILE:
            free(stmt->as.while_loop.label);
            ast_exp_destroy(stmt->as.while_loop.cond);
            ast_stmt_destroy(stmt->as.while_loop.body);
            break;
        case STMT_DO_WHILE:
            free(stmt->as.do_while_loop.label);
            ast_exp_destroy(stmt->as.do_while_loop.cond);
            ast_stmt_destroy(stmt->as.do_while_loop.body);
            break;
        case STMT_BREAK:
            free(stmt->as.break_stmt.label);
            break;
        case STMT_CONTINUE:
            free(stmt->as.continue_stmt.label);
            break;
        case STMT_LABEL:
            free(stmt->as.label.identifier);
            break;
        case STMT_GOTO:
            free(stmt->as.goto_stmt.target);
            break;
        case STMT_SWITCH:
            free(stmt->as.switch_stmt.label);
            ast_exp_destroy(stmt->as.switch_stmt.cond);
            for (size_t i = 0; i < stmt->as.switch_stmt.clauses.count; i++) {
                ast_block_destroy(&stmt->as.switch_stmt.clauses.items[i].body);
            }
            list_free(&stmt->as.switch_stmt.clauses);
            break;
    }
    free(stmt);
}

AstFunction ast_function_create(const char* identifier, AstBlock block) {
	AstFunction function;
	function.identifier = strdup(identifier);
	function.body = block;
	return function;
}

void ast_function_append(AstFunction* function, AstBlockItem block_item) {
	list_push(&function->body, block_item);
}

void ast_function_destroy(AstFunction* function) {
	free(function->identifier);
	ast_block_destroy(&function->body);
	return;
}

// --- Program ---

AstProgram ast_program_create(AstFunction* functions, int num_functions) {
	return (AstProgram){ .items = functions, .count = num_functions, .capacity = num_functions };
}

void ast_program_destroy(AstProgram* program) {
    for (size_t i = 0; i < program->count; i++) {
        ast_function_destroy(&program->items[i]);
    }
    list_free(program);
}
