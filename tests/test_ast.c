#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../src/parser/ast.h"

// --- AstBlock unit tests ---
//
// These exercise the block container that was extracted out of AstFunction:
// a block is a list of block items (statements or declarations), a compound
// statement is just a block, and blocks therefore nest through statements.

// A fresh block starts empty but with the requested capacity reserved.
void test_block_make_empty() {
    AstBlock block = ast_block_make(4);
    assert(block.size == 0);
    assert(block.capacity == 4);
    assert(block.items != NULL);
    ast_block_destroy(&block);
    printf("  PASS: test_block_make_empty\n");
}

// Appended items are stored in order and reachable through .items.
void test_block_append_keeps_order() {
    AstBlock block = ast_block_make(2);
    ast_block_append(&block, (AstBlockItem){
        .type = AST_STATEMENT,
        .as.stmt = make_return_stmt(create_int_exp(1)),
    });
    ast_block_append(&block, (AstBlockItem){
        .type = AST_STATEMENT,
        .as.stmt = make_exp_stmt(create_int_exp(2)),
    });

    assert(block.size == 2);
    assert(block.items[0].type == AST_STATEMENT);
    assert(block.items[0].as.stmt->kind == STMT_RETURN);
    assert(block.items[1].as.stmt->kind == STMT_EXP);
    ast_block_destroy(&block);
    printf("  PASS: test_block_append_keeps_order\n");
}

// Appending past the initial capacity grows the backing storage without
// losing earlier items.
void test_block_append_grows() {
    AstBlock block = ast_block_make(1);
    for (int i = 0; i < 10; i++) {
        ast_block_append(&block, (AstBlockItem){
            .type = AST_STATEMENT,
            .as.stmt = make_return_stmt(create_int_exp(i)),
        });
    }
    assert(block.size == 10);
    assert(block.capacity >= 10);
    for (int i = 0; i < 10; i++) {
        assert(block.items[i].as.stmt->kind == STMT_RETURN);
        assert(block.items[i].as.stmt->as.ret.exp->as.int_lit.value == i);
    }
    ast_block_destroy(&block);
    printf("  PASS: test_block_append_grows\n");
}

// A block can hold declarations too; the nullable initializer is honored.
void test_block_holds_declarations() {
    AstBlock block = ast_block_make(2);
    ast_block_append(&block, (AstBlockItem){
        .type = AST_DECLARATION,
        .as.decl = { .identifier = strdup("x"), .exp = create_int_exp(7) },
    });
    ast_block_append(&block, (AstBlockItem){
        .type = AST_DECLARATION,
        .as.decl = { .identifier = strdup("y"), .exp = NULL },
    });

    assert(block.size == 2);
    assert(block.items[0].type == AST_DECLARATION);
    assert(strcmp(block.items[0].as.decl.identifier, "x") == 0);
    assert(block.items[0].as.decl.exp->as.int_lit.value == 7);
    assert(block.items[1].as.decl.exp == NULL);
    ast_block_destroy(&block);
    printf("  PASS: test_block_holds_declarations\n");
}

// A compound statement is a block: make_compound_stmt wraps one, and the
// items stay reachable through stmt.as.compound.
void test_compound_stmt_wraps_block() {
    AstBlock block = ast_block_make(2);
    ast_block_append(&block, (AstBlockItem){
        .type = AST_STATEMENT,
        .as.stmt = make_return_stmt(create_int_exp(0)),
    });

    AstStatement* stmt = make_compound_stmt(block);
    assert(stmt->kind == STMT_COMPOUND);
    assert(stmt->as.compound.size == 1);
    assert(stmt->as.compound.items[0].as.stmt->kind == STMT_RETURN);
    destroy_stmt(stmt);
    printf("  PASS: test_compound_stmt_wraps_block\n");
}

// Blocks nest through statements: a block item can be a compound statement,
// which itself holds a block. Destroying the outer block must recurse.
void test_nested_compound_block() {
    AstBlock inner = ast_block_make(1);
    ast_block_append(&inner, (AstBlockItem){
        .type = AST_STATEMENT,
        .as.stmt = make_return_stmt(create_int_exp(42)),
    });

    AstBlock outer = ast_block_make(1);
    ast_block_append(&outer, (AstBlockItem){
        .type = AST_STATEMENT,
        .as.stmt = make_compound_stmt(inner),
    });

    assert(outer.items[0].as.stmt->kind == STMT_COMPOUND);
    AstBlock nested = outer.items[0].as.stmt->as.compound;
    assert(nested.size == 1);
    assert(nested.items[0].as.stmt->as.ret.exp->as.int_lit.value == 42);
    ast_block_destroy(&outer);
    printf("  PASS: test_nested_compound_block\n");
}

// AstFunction now delegates its list to an embedded AstBlock named body.
void test_function_holds_block() {
    AstFunction fn = ast_function_make("main", ast_block_make(4));
    ast_function_append(&fn, (AstBlockItem){
        .type = AST_STATEMENT,
        .as.stmt = make_return_stmt(create_int_exp(0)),
    });

    assert(strcmp(fn.identifier, "main") == 0);
    assert(fn.body.size == 1);
    assert(fn.body.items[0].type == AST_STATEMENT);
    assert(fn.body.items[0].as.stmt->kind == STMT_RETURN);
    ast_function_destroy(&fn);
    printf("  PASS: test_function_holds_block\n");
}

int main() {
    printf("Running AST block tests...\n");
    test_block_make_empty();
    test_block_append_keeps_order();
    test_block_append_grows();
    test_block_holds_declarations();
    test_compound_stmt_wraps_block();
    test_nested_compound_block();
    test_function_holds_block();
    printf("All AST block tests passed!\n");
    return 0;
}
