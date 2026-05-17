#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../parser/parser.h"

void test_create_int_expr() {
    Expr* e = create_int_expr(42);
    assert(e != NULL);
    assert(e->kind == EXPR_INT);
    assert(e->int_lit.value == 42);
    destroy_expr(e);
    printf("  PASS: test_create_int_expr\n");
}

void test_create_unary_expr() {
    Expr* e = create_unary_expr('-', create_int_expr(5));
    assert(e != NULL);
    assert(e->kind == EXPR_UNARY);
    assert(e->unary.op == '-');
    assert(e->unary.operand->int_lit.value == 5);
    destroy_expr(e);
    printf("  PASS: test_create_unary_expr\n");
}

void test_create_binary_expr() {
    Expr* e = create_binary_expr('+', create_int_expr(3), create_int_expr(7));
    assert(e != NULL);
    assert(e->kind == EXPR_BINARY);
    assert(e->binary.op == '+');
    assert(e->binary.lhs->int_lit.value == 3);
    assert(e->binary.rhs->int_lit.value == 7);
    destroy_expr(e);
    printf("  PASS: test_create_binary_expr\n");
}

void test_create_return_stmt() {
    Expr* e = create_int_expr(2);
    Stmt* s = create_return_stmt(e);
    assert(s != NULL);
    assert(s->kind == STMT_RETURN);
    assert(s->ret.expr->int_lit.value == 2);
    destroy_stmt(s);
    printf("  PASS: test_create_return_stmt\n");
}

void test_create_function_decl() {
    Stmt** body = malloc(sizeof(Stmt*));
    body[0] = create_return_stmt(create_int_expr(0));

    Decl* fn = create_function_decl("main", body, 1);
    assert(fn != NULL);
    assert(fn->kind == DECL_FUNCTION);
    assert(strcmp(fn->function.name, "main") == 0);
    assert(fn->function.num_stmts == 1);
    assert(fn->function.body[0]->kind == STMT_RETURN);
    destroy_decl(fn);
    printf("  PASS: test_create_function_decl\n");
}

void test_create_program() {
    Stmt** body = malloc(sizeof(Stmt*));
    body[0] = create_return_stmt(create_int_expr(0));

    Decl** decls = malloc(sizeof(Decl*));
    decls[0] = create_function_decl("main", body, 1);

    Program* prog = create_program(decls, 1);
    assert(prog != NULL);
    assert(prog->num_decls == 1);
    assert(prog->decls[0]->kind == DECL_FUNCTION);
    destroy_program(prog);
    printf("  PASS: test_create_program\n");
}

int main(void) {
    printf("Running parser tests...\n");
    test_create_int_expr();
    test_create_unary_expr();
    test_create_binary_expr();
    test_create_return_stmt();
    test_create_function_decl();
    test_create_program();
    printf("All parser tests passed!\n");
    return 0;
}
