#include "ast.h"
#include <string.h>

// --- Expressions ---

Expr* create_int_expr(int value) {
    Expr* e = malloc(sizeof(Expr));
    e->kind = EXPR_INT;
    e->int_lit.value = value;
    return e;
}

Expr* create_unary_expr(UnaryOpType op_type, Expr* operand) {
    Expr* e = malloc(sizeof(Expr));
    e->kind = EXPR_UNARY;
    e->unary.op_type = op_type;
    e->unary.operand = operand;
    return e;
}

Expr* create_binary_expr(char op, Expr* lhs, Expr* rhs) {
    Expr* e = malloc(sizeof(Expr));
    e->kind = EXPR_BINARY;
    e->binary.op = op;
    e->binary.lhs = lhs;
    e->binary.rhs = rhs;
    return e;
}

void destroy_expr(Expr* expr) {
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

Stmt* create_return_stmt(Expr* expr) {
    Stmt* s = malloc(sizeof(Stmt));
    s->kind = STMT_RETURN;
    s->ret.expr = expr;
    return s;
}

Stmt* create_expr_stmt(Expr* expr) {
    Stmt* s = malloc(sizeof(Stmt));
    s->kind = STMT_EXPR;
    s->expr_stmt.expr = expr;
    return s;
}

void destroy_stmt(Stmt* stmt) {
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

// --- Declarations ---

Decl* create_function_decl(char* name, Stmt** body, int num_stmts) {
    Decl* d = malloc(sizeof(Decl));
    d->kind = DECL_FUNCTION;
    d->function.name = strdup(name);
    d->function.body = body;
    d->function.num_stmts = num_stmts;
    return d;
}

void destroy_decl(Decl* decl) {
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

// --- Program ---

Program* create_program(Decl** decls, int num_decls) {
    Program* p = malloc(sizeof(Program));
    p->decls = decls;
    p->num_decls = num_decls;
    return p;
}

void destroy_program(Program* program) {
    if (!program) return;
    for (int i = 0; i < program->num_decls; i++) {
        destroy_decl(program->decls[i]);
    }
    free(program->decls);
    free(program);
}
