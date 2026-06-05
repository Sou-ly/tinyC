#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../parser/ast.h"
#include "../parser/parser.h"
#include "../list.h"

// --- AST unit tests ---

void test_create_int_expr() {
    AstExpression* e = create_int_expr(42);
    assert(e != NULL);
    assert(e->kind == EXPR_INT);
    assert(e->int_lit.value == 42);
    destroy_expr(e);
    printf("  PASS: test_create_int_expr\n");
}

void test_create_unary_expr() {
    AstExpression* e = create_unary_expr('-', create_int_expr(5));
    assert(e != NULL);
    assert(e->kind == EXPR_UNARY);
    assert(e->unary.op == '-');
    assert(e->unary.operand->int_lit.value == 5);
    destroy_expr(e);
    printf("  PASS: test_create_unary_expr\n");
}

void test_create_binary_expr() {
    AstExpression* e = create_binary_expr('+', create_int_expr(3), create_int_expr(7));
    assert(e != NULL);
    assert(e->kind == EXPR_BINARY);
    assert(e->binary.op == '+');
    assert(e->binary.lhs->int_lit.value == 3);
    assert(e->binary.rhs->int_lit.value == 7);
    destroy_expr(e);
    printf("  PASS: test_create_binary_expr\n");
}

void test_create_return_stmt() {
    AstStatement* s = create_return_stmt(create_int_expr(2));
    assert(s != NULL);
    assert(s->kind == STMT_RETURN);
    assert(s->ret.expr->int_lit.value == 2);
    destroy_stmt(s);
    printf("  PASS: test_create_return_stmt\n");
}

void test_create_function_decl() {
    AstStatement** body = malloc(sizeof(AstStatement*));
    body[0] = create_return_stmt(create_int_expr(0));

    AstDeclaration* fn = create_function_decl("main", body, 1);
    assert(fn != NULL);
    assert(fn->kind == DECL_FUNCTION);
    assert(strcmp(fn->function.name, "main") == 0);
    assert(fn->function.num_stmts == 1);
    assert(fn->function.body[0]->kind == STMT_RETURN);
    destroy_decl(fn);
    printf("  PASS: test_create_function_decl\n");
}

// --- Parser integration test ---
// Parses: int main() { return 2; }

void test_parse_return_2() {
    token_list tokens = token_list_create(8);

    token_list_push(&tokens, (token){TOK_KEYWORD,   {.kw = KW_INT},          1, 1});
    token_list_push(&tokens, (token){TOK_IDENTIFIER, {.ident = strdup("main")}, 1, 5});
    token_list_push(&tokens, (token){TOK_SEPARATOR, {.sep = SEP_LPAR},       1, 9});
    token_list_push(&tokens, (token){TOK_SEPARATOR, {.sep = SEP_RPAR},       1, 10});
    token_list_push(&tokens, (token){TOK_SEPARATOR, {.sep = SEP_LBRACE},     1, 12});
    token_list_push(&tokens, (token){TOK_KEYWORD,   {.kw = KW_RETURN},       2, 5});
    token_list_push(&tokens, (token){TOK_INT_LITERAL, {.int_val = 2},        2, 12});
    token_list_push(&tokens, (token){TOK_SEPARATOR, {.sep = SEP_SEMICOLON},  2, 13});
    token_list_push(&tokens, (token){TOK_SEPARATOR, {.sep = SEP_RBRACE},     3, 1});

    Parser parser = parser_create(&tokens);
    AstProgram* prog = parse_program(&parser);

    assert(prog != NULL);
    assert(prog->num_decls == 1);
    assert(prog->decls[0]->kind == DECL_FUNCTION);
    assert(strcmp(prog->decls[0]->function.name, "main") == 0);
    assert(prog->decls[0]->function.num_stmts == 1);
    assert(prog->decls[0]->function.body[0]->kind == STMT_RETURN);
    assert(prog->decls[0]->function.body[0]->ret.expr->kind == EXPR_INT);
    assert(prog->decls[0]->function.body[0]->ret.expr->int_lit.value == 2);

    destroy_program(prog);
    free(tokens.items);
    printf("  PASS: test_parse_return_2\n");
}

int main(void) {
    printf("Running parser tests...\n");
    test_create_int_expr();
    test_create_unary_expr();
    test_create_binary_expr();
    test_create_return_stmt();
    test_create_function_decl();
    test_parse_return_2();
    printf("All parser tests passed!\n");
    return 0;
}
