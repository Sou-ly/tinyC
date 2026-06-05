#include "ast.h"
#include <string.h>

// --- AstExpressionessions ---

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

AstStatement* create_return_stmt(AstExpression* expr) {
    AstStatement* s = malloc(sizeof(AstStatement));
    s->kind = STMT_RETURN;
    s->ret.expr = expr;
    return s;
}

AstStatement* create_expr_stmt(AstExpression* expr) {
    AstStatement* s = malloc(sizeof(AstStatement));
    s->kind = STMT_EXPR;
    s->expr_stmt.expr = expr;
    return s;
}

void destroy_stmt(AstStatement* stmt) {
    if (!stmt) return;
    switch (stmt->kind) {
        case STMT_RETURN:
            destroy_expr(stmt->ret.expr);
            break;
        case STMT_EXPR:
            destroy_expr(stmt->expr_stmt.expr);
            break;
    }
    free(stmt);
}

// --- AstDeclarationarations ---

AstDeclaration* create_function_decl(char* name, AstStatement** body, int num_stmts) {
    AstDeclaration* d = malloc(sizeof(AstDeclaration));
    d->kind = DECL_FUNCTION;
    d->function.name = strdup(name);
    d->function.body = body;
    d->function.num_stmts = num_stmts;
    return d;
}

void destroy_decl(AstDeclaration* decl) {
    if (!decl) return;
    switch (decl->kind) {
        case DECL_FUNCTION:
            free(decl->function.name);
            for (int i = 0; i < decl->function.num_stmts; i++) {
                destroy_stmt(decl->function.body[i]);
            }
            free(decl->function.body);
            break;
    }
    free(decl);
}

// --- AstProgram ---

AstProgram* create_program(AstDeclaration** decls, int num_decls) {
    AstProgram* p = malloc(sizeof(AstProgram));
    p->decls = decls;
    p->num_decls = num_decls;
    return p;
}

void destroy_program(AstProgram* program) {
    if (!program) return;
    for (int i = 0; i < program->num_decls; i++) {
        destroy_decl(program->decls[i]);
    }
    free(program->decls);
    free(program);
}
