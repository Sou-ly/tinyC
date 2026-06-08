#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../src/parser/ast.h"
#include "../src/parser/parser.h"
#include "../src/list.h"

// --- AST unit tests ---

void test_create_int_exp() {
    AstExp* e = create_int_exp(42);
    assert(e != NULL);
    assert(e->kind == EXP_INT);
    assert(e->int_lit.value == 42);
    destroy_exp(e);
    printf("  PASS: test_create_int_exp\n");
}

void test_create_unary_exp() {
    AstExp* e = create_unary_exp(UNOP_MINUS, create_int_exp(5));
    assert(e != NULL);
    assert(e->kind == EXP_UNOP);
    assert(e->unary.op_type == UNOP_MINUS);
    assert(e->unary.operand->int_lit.value == 5);
    destroy_exp(e);
    printf("  PASS: test_create_unary_exp\n");
}

void test_create_binop_exp() {
    AstExp* e = create_binop_exp(BINOP_ADD, create_int_exp(3), create_int_exp(7));
    assert(e != NULL);
    assert(e->kind == EXP_BINOP);
    assert(e->binop.op_type == BINOP_ADD);
    assert(e->binop.lhs->int_lit.value == 3);
    assert(e->binop.rhs->int_lit.value == 7);
    destroy_exp(e);
    printf("  PASS: test_create_binop_exp\n");
}

void test_create_return_stmt() {
    AstStatement s = make_return_stmt(create_int_exp(2));
    assert(s.kind == STMT_RETURN);
    assert(s.ret.exp->int_lit.value == 2);
    destroy_stmt(&s);
    printf("  PASS: test_create_return_stmt\n");
}

void test_create_function_decl() {
    AstStatement* body = malloc(sizeof(AstStatement));
    body[0] = make_return_stmt(create_int_exp(0));

    AstDeclaration fn = make_function_decl("main", body, 1);
    assert(fn.kind == DECL_FUNCTION);
    assert(strcmp(fn.function.name, "main") == 0);
    assert(fn.function.num_stmts == 1);
    assert(fn.function.body[0].kind == STMT_RETURN);
    destroy_decl(&fn);
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
    AstProgram prog = parse_program(&parser);

    assert(prog.num_decls == 1);
    assert(prog.decls[0].kind == DECL_FUNCTION);
    assert(strcmp(prog.decls[0].function.name, "main") == 0);
    assert(prog.decls[0].function.num_stmts == 1);
    assert(prog.decls[0].function.body[0].kind == STMT_RETURN);
    assert(prog.decls[0].function.body[0].ret.exp->kind == EXP_INT);
    assert(prog.decls[0].function.body[0].ret.exp->int_lit.value == 2);

    destroy_program(&prog);
    free(tokens.items);
    printf("  PASS: test_parse_return_2\n");
}

// --- Parser precedence test ---
// Parses: int main() { return 1 + 2 * 3; }
// Expects the tree: (1 + (2 * 3)), i.e. '*' binds tighter than '+'.

void test_parse_precedence() {
    token_list tokens = token_list_create(16);

    token_list_push(&tokens, (token){TOK_KEYWORD,    {.kw = KW_INT},             1, 1});
    token_list_push(&tokens, (token){TOK_IDENTIFIER, {.ident = strdup("main")},  1, 5});
    token_list_push(&tokens, (token){TOK_SEPARATOR,  {.sep = SEP_LPAR},          1, 9});
    token_list_push(&tokens, (token){TOK_SEPARATOR,  {.sep = SEP_RPAR},          1, 10});
    token_list_push(&tokens, (token){TOK_SEPARATOR,  {.sep = SEP_LBRACE},        1, 12});
    token_list_push(&tokens, (token){TOK_KEYWORD,    {.kw = KW_RETURN},          2, 5});
    token_list_push(&tokens, (token){TOK_INT_LITERAL,{.int_val = 1},             2, 12});
    token_list_push(&tokens, (token){TOK_OPERATOR,   {.op = OP_PLUS},            2, 14});
    token_list_push(&tokens, (token){TOK_INT_LITERAL,{.int_val = 2},             2, 16});
    token_list_push(&tokens, (token){TOK_OPERATOR,   {.op = OP_STAR},            2, 18});
    token_list_push(&tokens, (token){TOK_INT_LITERAL,{.int_val = 3},             2, 20});
    token_list_push(&tokens, (token){TOK_SEPARATOR,  {.sep = SEP_SEMICOLON},     2, 21});
    token_list_push(&tokens, (token){TOK_SEPARATOR,  {.sep = SEP_RBRACE},        3, 1});

    Parser parser = parser_create(&tokens);
    AstProgram prog = parse_program(&parser);

    AstExp* root = prog.decls[0].function.body[0].ret.exp;
    // root = 1 + (2 * 3)
    assert(root->kind == EXP_BINOP);
    assert(root->binop.op_type == BINOP_ADD);
    assert(root->binop.lhs->kind == EXP_INT);
    assert(root->binop.lhs->int_lit.value == 1);
    // rhs = 2 * 3
    AstExp* rhs = root->binop.rhs;
    assert(rhs->kind == EXP_BINOP);
    assert(rhs->binop.op_type == BINOP_MUL);
    assert(rhs->binop.lhs->int_lit.value == 2);
    assert(rhs->binop.rhs->int_lit.value == 3);

    destroy_program(&prog);
    free(tokens.items);
    printf("  PASS: test_parse_precedence\n");
}

int main(void) {
    printf("Running parser tests...\n");
    test_create_int_exp();
    test_create_unary_exp();
    test_create_binop_exp();
    test_create_return_stmt();
    test_create_function_decl();
    test_parse_return_2();
    test_parse_precedence();
    printf("All parser tests passed!\n");
    return 0;
}
