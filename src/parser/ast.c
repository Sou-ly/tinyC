#include "ast.h"
#include <string.h>

// --- Expressions ---

AstExp* create_int_exp(int value) {
    AstExp* e = malloc(sizeof(AstExp));
    e->kind = EXPR_INT;
    e->int_lit.value = value;
    return e;
}

AstExp* create_unary_exp(UnaryOpType op_type, AstExp* operand) {
    AstExp* e = malloc(sizeof(AstExp));
    e->kind = EXPR_UNARY;
    e->unary.op_type = op_type;
    e->unary.operand = operand;
    return e;
}

AstExp* create_binary_exp(char op, AstExp* lhs, AstExp* rhs) {
    AstExp* e = malloc(sizeof(AstExp));
    e->kind = EXPR_BINARY;
    e->binary.op = op;
    e->binary.lhs = lhs;
    e->binary.rhs = rhs;
    return e;
}

void destroy_exp(AstExp* exp) {
    if (!exp) return;
    switch (exp->kind) {
        case EXPR_UNARY:
            destroy_exp(exp->unary.operand);
            break;
        case EXPR_BINARY:
            destroy_exp(exp->binary.lhs);
            destroy_exp(exp->binary.rhs);
            break;
        case EXPR_INT:
            break;
    }
    free(exp);
}

// --- Statements ---

AstStatement make_return_stmt(AstExp* exp) {
    return (AstStatement){ .kind = STMT_RETURN, .ret = { exp } };
}

AstStatement make_exp_stmt(AstExp* exp) {
    return (AstStatement){ .kind = STMT_EXPR, .exp_stmt = { exp } };
}

void destroy_stmt(AstStatement* stmt) {
    switch (stmt->kind) {
        case STMT_RETURN:
            destroy_exp(stmt->ret.exp);
            break;
        case STMT_EXPR:
            destroy_exp(stmt->exp_stmt.exp);
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
