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
        .as.decl = { .identifier = strdup("x"), .init = some_exp(create_int_exp(7)) },
    });
    ast_block_append(&block, (AstBlockItem){
        .type = AST_DECLARATION,
        .as.decl = { .identifier = strdup("y"), .init = no_exp() },
    });

    assert(block.size == 2);
    assert(block.items[0].type == AST_DECLARATION);
    assert(strcmp(block.items[0].as.decl.identifier, "x") == 0);
    assert(block.items[0].as.decl.init.present);
    assert(block.items[0].as.decl.init.exp->as.int_lit.value == 7);
    assert(!block.items[1].as.decl.init.present);
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

// --- Loop statement construction tests ---

// make_while_stmt creates a STMT_WHILE with label initially NULL.
void test_while_stmt_creation() {
    AstStatement* s = make_while_stmt(
        create_int_exp(1),
        make_exp_stmt(create_int_exp(2)));
    assert(s->kind == STMT_WHILE);
    assert(s->as.while_loop.label == NULL);
    assert(s->as.while_loop.cond->as.int_lit.value == 1);
    assert(s->as.while_loop.body->kind == STMT_EXP);
    destroy_stmt(s);
    printf("  PASS: test_while_stmt_creation\n");
}

// make_do_while_stmt creates a STMT_DO_WHILE with label initially NULL.
void test_do_while_stmt_creation() {
    AstStatement* s = make_do_while_stmt(
        create_int_exp(1),
        make_exp_stmt(create_int_exp(2)));
    assert(s->kind == STMT_DO_WHILE);
    assert(s->as.do_while_loop.label == NULL);
    assert(s->as.do_while_loop.cond->as.int_lit.value == 1);
    assert(s->as.do_while_loop.body->kind == STMT_EXP);
    destroy_stmt(s);
    printf("  PASS: test_do_while_stmt_creation\n");
}

// make_for_stmt creates a STMT_FOR with label NULL and nullable cond/post.
void test_for_stmt_creation() {
    AstForInit init = make_for_init_exp(NULL);
    AstStatement* s = make_for_stmt(
        init, some_exp(create_int_exp(1)), some_exp(create_int_exp(2)),
        make_exp_stmt(create_int_exp(3)));
    assert(s->kind == STMT_FOR);
    assert(s->as.for_loop.label == NULL);
    assert(s->as.for_loop.cond.present);
    assert(s->as.for_loop.cond.exp->as.int_lit.value == 1);
    assert(s->as.for_loop.post.present);
    assert(s->as.for_loop.post.exp->as.int_lit.value == 2);
    assert(s->as.for_loop.body->kind == STMT_EXP);
    destroy_stmt(s);
    printf("  PASS: test_for_stmt_creation\n");
}

// make_for_stmt with absent cond and post (infinite loop shape).
void test_for_stmt_nullable_fields() {
    AstForInit init = make_for_init_exp(NULL);
    AstStatement* s = make_for_stmt(
        init, no_exp(), no_exp(),
        make_exp_stmt(create_int_exp(0)));
    assert(s->kind == STMT_FOR);
    assert(!s->as.for_loop.cond.present);
    assert(!s->as.for_loop.post.present);
    destroy_stmt(s);
    printf("  PASS: test_for_stmt_nullable_fields\n");
}

// make_break_stmt and make_continue_stmt store their label (or NULL).
void test_break_continue_creation() {
    AstStatement* brk = make_break_stmt(NULL);
    assert(brk->kind == STMT_BREAK);
    assert(brk->as.break_stmt.label == NULL);
    destroy_stmt(brk);

    AstStatement* cont = make_continue_stmt(NULL);
    assert(cont->kind == STMT_CONTINUE);
    assert(cont->as.continue_stmt.label == NULL);
    destroy_stmt(cont);

    AstStatement* brk2 = make_break_stmt(strdup("loop.0"));
    assert(strcmp(brk2->as.break_stmt.label, "loop.0") == 0);
    destroy_stmt(brk2);

    printf("  PASS: test_break_continue_creation\n");
}

// A while loop containing break and continue as its body (via compound).
void test_loop_with_break_continue() {
    AstBlock body = ast_block_make(2);
    ast_block_append(&body, (AstBlockItem){
        .type = AST_STATEMENT,
        .as.stmt = make_break_stmt(NULL),
    });
    ast_block_append(&body, (AstBlockItem){
        .type = AST_STATEMENT,
        .as.stmt = make_continue_stmt(NULL),
    });

    AstStatement* loop = make_while_stmt(
        create_int_exp(1),
        make_compound_stmt(body));

    assert(loop->kind == STMT_WHILE);
    AstBlock* compound = &loop->as.while_loop.body->as.compound;
    assert(compound->size == 2);
    assert(compound->items[0].as.stmt->kind == STMT_BREAK);
    assert(compound->items[1].as.stmt->kind == STMT_CONTINUE);
    destroy_stmt(loop);
    printf("  PASS: test_loop_with_break_continue\n");
}

// Nested loops: outer while containing an inner for loop.
void test_nested_loops() {
    AstForInit init = make_for_init_exp(NULL);
    AstStatement* inner = make_for_stmt(
        init, some_exp(create_int_exp(1)), no_exp(),
        make_exp_stmt(create_int_exp(0)));

    AstBlock outer_body = ast_block_make(1);
    ast_block_append(&outer_body, (AstBlockItem){
        .type = AST_STATEMENT,
        .as.stmt = inner,
    });

    AstStatement* outer = make_while_stmt(
        create_int_exp(1),
        make_compound_stmt(outer_body));

    assert(outer->kind == STMT_WHILE);
    AstStatement* nested = outer->as.while_loop.body->as.compound.items[0].as.stmt;
    assert(nested->kind == STMT_FOR);
    assert(nested->as.for_loop.cond.exp->as.int_lit.value == 1);
    destroy_stmt(outer);
    printf("  PASS: test_nested_loops\n");
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
    printf("Running loop statement tests...\n");
    test_while_stmt_creation();
    test_do_while_stmt_creation();
    test_for_stmt_creation();
    test_for_stmt_nullable_fields();
    test_break_continue_creation();
    test_loop_with_break_continue();
    test_nested_loops();
    printf("All AST tests passed!\n");
    return 0;
}
