#include "ast.h"
#include <string.h>

// --- Expressions ---

AstExpression* create_int_expr(int value) {
    AstExpression* e = malloc(sizeof(AstExpression));
    e->kind = EXPR_INT;
    e->int_lit.value = value;
    return e;
}

AstExpression* create_unary_expr(UnaryOpType op_type, AstExpression* operand) {
    AstExpression* e = malloc(sizeof(AstExpression));
    e->kind = EXPR_UNARY;
    e->unary.op_type = op_type;
    e->unary.operand = operand;
    return e;
}

AstExpression* create_binary_expr(char op, AstExpression* lhs, AstExpression* rhs) {
    AstExpression* e = malloc(sizeof(AstExpression));
    e->kind = EXPR_BINARY;
    e->binary.op = op;
    e->binary.lhs = lhs;
    e->binary.rhs = rhs;
    return e;
}

void destroy_expr(AstExpression* expr) {
    if (!expr) return;
    switch (expr->kind) {
        case EXPR_UNARY:
            destroy_expr(expr->unary.operand);
            break;
        case EXPR_BINARY:
            destroy_expr(expr->binary.lhs);
            destroy_expr(expr->binary.rhs);
            break;
        case EXPR_INT:
            break;
    }
    free(expr);
}

// --- Statements ---

AstStatement make_return_stmt(AstExpression* expr) {
    return (AstStatement){ .kind = STMT_RETURN, .ret = { expr } };
}

AstStatement make_expr_stmt(AstExpression* expr) {
    return (AstStatement){ .kind = STMT_EXPR, .expr_stmt = { expr } };
}

void destroy_stmt(AstStatement* stmt) {
    switch (stmt->kind) {
        case STMT_RETURN:
            destroy_expr(stmt->ret.expr);
            break;
        case STMT_EXPR:
            destroy_expr(stmt->expr_stmt.expr);
            break;
    }
}

// --- Declarations ---

AstDeclaration make_function_decl(char* name, AstStatement* body, int num_stmts) {
    return (AstDeclaration){
        .kind = DECL_FUNCTION,
        .function = { .name = strdup(name), .body = body, .num_stmts = num_stmts }
    };
}

void destroy_decl(AstDeclaration* decl) {
    switch (decl->kind) {
        case DECL_FUNCTION:
            free(decl->function.name);
            for (int i = 0; i < decl->function.num_stmts; i++) {
                destroy_stmt(&decl->function.body[i]);
            }
            free(decl->function.body);
            break;
    }
}

// --- Program ---

AstProgram make_program(AstDeclaration* decls, int num_decls) {
    return (AstProgram){ .decls = decls, .num_decls = num_decls };
}

void destroy_program(AstProgram* program) {
    for (int i = 0; i < program->num_decls; i++) {
        destroy_decl(&program->decls[i]);
    }
    free(program->decls);
}
