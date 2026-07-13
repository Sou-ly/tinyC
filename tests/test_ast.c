#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../src/parser/ast.h"

// --- AstBlock unit tests ---
//
// These exercise the block container that AstFunctionDeclaration holds as its body:
// a block is a list of block items (statements or declarations), a compound
// statement is just a block, and blocks therefore nest through statements.

// A zero-initialized block is the empty list: no storage until first append.
void test_block_make_empty() {
    AstBlock block = (AstBlock){0};
    assert(block.count == 0);
    assert(block.capacity == 0);
    assert(block.items == NULL);
    ast_block_destroy(&block);
    printf("  PASS: test_block_make_empty\n");
}

// Appended items are stored in order and reachable through .items.
void test_block_append_keeps_order() {
    AstBlock block = (AstBlock){0};
    ast_block_append(&block, (AstBlockItem){
        .kind = AST_STATEMENT,
        .as.statement = ast_stmt_return(ast_exp_int(1)),
    });
    ast_block_append(&block, (AstBlockItem){
        .kind = AST_STATEMENT,
        .as.statement = ast_stmt_exp(ast_exp_int(2)),
    });

    assert(block.count == 2);
    assert(block.items[0].kind == AST_STATEMENT);
    assert(block.items[0].as.statement->kind == STMT_RETURN);
    assert(block.items[1].as.statement->kind == STMT_EXP);
    ast_block_destroy(&block);
    printf("  PASS: test_block_append_keeps_order\n");
}

// Appending past the initial capacity grows the backing storage without
// losing earlier items.
void test_block_append_grows() {
    AstBlock block = (AstBlock){0};
    for (int i = 0; i < 10; i++) {
        ast_block_append(&block, (AstBlockItem){
            .kind = AST_STATEMENT,
            .as.statement = ast_stmt_return(ast_exp_int(i)),
        });
    }
    assert(block.count == 10);
    assert(block.capacity >= 10);
    for (int i = 0; i < 10; i++) {
        assert(block.items[i].as.statement->kind == STMT_RETURN);
        assert(block.items[i].as.statement->as.ret.exp->as.int_lit.value == i);
    }
    ast_block_destroy(&block);
    printf("  PASS: test_block_append_grows\n");
}

// A block can hold declarations too; the nullable initializer is honored.
void test_block_holds_declarations() {
    AstBlock block = (AstBlock){0};
    ast_block_append(&block, (AstBlockItem){
        .kind = AST_DECLARATION,
        .as.declaration = ast_declaration_variable(ast_variable_declaration(strdup("x"), ast_exp_int(7))),
    });
    ast_block_append(&block, (AstBlockItem){
        .kind = AST_DECLARATION,
        .as.declaration = ast_declaration_variable(ast_variable_declaration(strdup("y"), NULL)),
    });

    assert(block.count == 2);
    assert(block.items[0].kind == AST_DECLARATION);
    assert(block.items[0].as.declaration.kind == DECL_VAR);
    const AstVariableDeclaration* x = &block.items[0].as.declaration.as.variable;
    assert(strcmp(x->identifier, "x") == 0);
    assert(x->init->as.int_lit.value == 7);
    assert(block.items[1].as.declaration.as.variable.init == NULL);
    ast_block_destroy(&block);
    printf("  PASS: test_block_holds_declarations\n");
}

// A compound statement is a block: ast_stmt_compound wraps one, and the
// items stay reachable through stmt.as.compound.
void test_compound_stmt_wraps_block() {
    AstBlock block = (AstBlock){0};
    ast_block_append(&block, (AstBlockItem){
        .kind = AST_STATEMENT,
        .as.statement = ast_stmt_return(ast_exp_int(0)),
    });

    AstStatement* stmt = ast_stmt_compound(block);
    assert(stmt->kind == STMT_COMPOUND);
    assert(stmt->as.compound.count == 1);
    assert(stmt->as.compound.items[0].as.statement->kind == STMT_RETURN);
    ast_stmt_destroy(stmt);
    printf("  PASS: test_compound_stmt_wraps_block\n");
}

// Blocks nest through statements: a block item can be a compound statement,
// which itself holds a block. Destroying the outer block must recurse.
void test_nested_compound_block() {
    AstBlock inner = (AstBlock){0};
    ast_block_append(&inner, (AstBlockItem){
        .kind = AST_STATEMENT,
        .as.statement = ast_stmt_return(ast_exp_int(42)),
    });

    AstBlock outer = (AstBlock){0};
    ast_block_append(&outer, (AstBlockItem){
        .kind = AST_STATEMENT,
        .as.statement = ast_stmt_compound(inner),
    });

    assert(outer.items[0].as.statement->kind == STMT_COMPOUND);
    AstBlock nested = outer.items[0].as.statement->as.compound;
    assert(nested.count == 1);
    assert(nested.items[0].as.statement->as.ret.exp->as.int_lit.value == 42);
    ast_block_destroy(&outer);
    printf("  PASS: test_nested_compound_block\n");
}

// A function definition carries its block as a present OptionalBlock body.
void test_function_holds_block() {
    AstBlock body = (AstBlock){0};
    ast_block_append(&body, (AstBlockItem){
        .kind = AST_STATEMENT,
        .as.statement = ast_stmt_return(ast_exp_int(0)),
    });
    AstFunctionDeclaration fn = ast_function_declaration(
        strdup("main"), (AstParamList){0}, SOME(OptionalBlock, body));

    assert(strcmp(fn.identifier, "main") == 0);
    assert(fn.body.present);
    assert(fn.body.value.count == 1);
    assert(fn.body.value.items[0].kind == AST_STATEMENT);
    assert(fn.body.value.items[0].as.statement->kind == STMT_RETURN);
    ast_function_declaration_destroy(&fn);
    printf("  PASS: test_function_holds_block\n");
}

// A prototype has no body; destroying it must not touch body.value.
void test_function_prototype_has_no_body() {
    AstFunctionDeclaration fn = ast_function_declaration(
        strdup("puts"), (AstParamList){0}, NONE(OptionalBlock));
    assert(!fn.body.present);
    ast_function_declaration_destroy(&fn);
    printf("  PASS: test_function_prototype_has_no_body\n");
}

// --- Loop statement construction tests ---

// ast_stmt_while creates a STMT_WHILE with label initially NULL.
void test_while_stmt_creation() {
    AstStatement* s = ast_stmt_while(
        ast_exp_int(1),
        ast_stmt_exp(ast_exp_int(2)));
    assert(s->kind == STMT_WHILE);
    assert(s->as.while_loop.label == NULL);
    assert(s->as.while_loop.cond->as.int_lit.value == 1);
    assert(s->as.while_loop.body->kind == STMT_EXP);
    ast_stmt_destroy(s);
    printf("  PASS: test_while_stmt_creation\n");
}

// ast_stmt_do_while creates a STMT_DO_WHILE with label initially NULL.
void test_do_while_stmt_creation() {
    AstStatement* s = ast_stmt_do_while(
        ast_exp_int(1),
        ast_stmt_exp(ast_exp_int(2)));
    assert(s->kind == STMT_DO_WHILE);
    assert(s->as.do_while_loop.label == NULL);
    assert(s->as.do_while_loop.cond->as.int_lit.value == 1);
    assert(s->as.do_while_loop.body->kind == STMT_EXP);
    ast_stmt_destroy(s);
    printf("  PASS: test_do_while_stmt_creation\n");
}

// ast_stmt_for creates a STMT_FOR with label NULL and nullable cond/post.
void test_for_stmt_creation() {
    AstForInit init = ast_for_init_exp(NULL);
    AstStatement* s = ast_stmt_for(
        init, ast_exp_int(1), ast_exp_int(2),
        ast_stmt_exp(ast_exp_int(3)));
    assert(s->kind == STMT_FOR);
    assert(s->as.for_loop.label == NULL);
    assert(s->as.for_loop.cond);
    assert(s->as.for_loop.cond->as.int_lit.value == 1);
    assert(s->as.for_loop.post);
    assert(s->as.for_loop.post->as.int_lit.value == 2);
    assert(s->as.for_loop.body->kind == STMT_EXP);
    ast_stmt_destroy(s);
    printf("  PASS: test_for_stmt_creation\n");
}

// ast_stmt_for with absent cond and post (infinite loop shape).
void test_for_stmt_nullable_fields() {
    AstForInit init = ast_for_init_exp(NULL);
    AstStatement* s = ast_stmt_for(
        init, NULL, NULL,
        ast_stmt_exp(ast_exp_int(0)));
    assert(s->kind == STMT_FOR);
    assert(!s->as.for_loop.cond);
    assert(!s->as.for_loop.post);
    ast_stmt_destroy(s);
    printf("  PASS: test_for_stmt_nullable_fields\n");
}

// ast_stmt_break and ast_stmt_continue store their label (or NULL).
void test_break_continue_creation() {
    AstStatement* brk = ast_stmt_break(NULL);
    assert(brk->kind == STMT_BREAK);
    assert(brk->as.break_stmt.label == NULL);
    ast_stmt_destroy(brk);

    AstStatement* cont = ast_stmt_continue(NULL);
    assert(cont->kind == STMT_CONTINUE);
    assert(cont->as.continue_stmt.label == NULL);
    ast_stmt_destroy(cont);

    AstStatement* brk2 = ast_stmt_break(strdup("loop.0"));
    assert(strcmp(brk2->as.break_stmt.label, "loop.0") == 0);
    ast_stmt_destroy(brk2);

    printf("  PASS: test_break_continue_creation\n");
}

// A while loop containing break and continue as its body (via compound).
void test_loop_with_break_continue() {
    AstBlock body = (AstBlock){0};
    ast_block_append(&body, (AstBlockItem){
        .kind = AST_STATEMENT,
        .as.statement = ast_stmt_break(NULL),
    });
    ast_block_append(&body, (AstBlockItem){
        .kind = AST_STATEMENT,
        .as.statement = ast_stmt_continue(NULL),
    });

    AstStatement* loop = ast_stmt_while(
        ast_exp_int(1),
        ast_stmt_compound(body));

    assert(loop->kind == STMT_WHILE);
    AstBlock* compound = &loop->as.while_loop.body->as.compound;
    assert(compound->count == 2);
    assert(compound->items[0].as.statement->kind == STMT_BREAK);
    assert(compound->items[1].as.statement->kind == STMT_CONTINUE);
    ast_stmt_destroy(loop);
    printf("  PASS: test_loop_with_break_continue\n");
}

// Nested loops: outer while containing an inner for loop.
void test_nested_loops() {
    AstForInit init = ast_for_init_exp(NULL);
    AstStatement* inner = ast_stmt_for(
        init, ast_exp_int(1), NULL,
        ast_stmt_exp(ast_exp_int(0)));

    AstBlock outer_body = (AstBlock){0};
    ast_block_append(&outer_body, (AstBlockItem){
        .kind = AST_STATEMENT,
        .as.statement = inner,
    });

    AstStatement* outer = ast_stmt_while(
        ast_exp_int(1),
        ast_stmt_compound(outer_body));

    assert(outer->kind == STMT_WHILE);
    AstStatement* nested = outer->as.while_loop.body->as.compound.items[0].as.statement;
    assert(nested->kind == STMT_FOR);
    assert(nested->as.for_loop.cond->as.int_lit.value == 1);
    ast_stmt_destroy(outer);
    printf("  PASS: test_nested_loops\n");
}

// --- Function-call and function-declaration construction tests ---

// Push an expression value into an arg list and free the heap shell, matching
// how the parser builds argument lists (args are stored by value, not pointer).
static void arg_push(AstArgList* args, AstExp* exp) {
    list_push(args, *exp);
    free(exp);
}

// A call with no arguments: kind, owned identifier, and an empty arg list.
void test_function_call_no_args() {
    AstExp* call = ast_exp_function_call(strdup("foo"), (AstArgList){0});
    assert(call->kind == EXP_FUNCTION_CALL);
    assert(strcmp(call->as.funcall.identifier, "foo") == 0);
    assert(call->as.funcall.args.count == 0);
    ast_exp_destroy(call);
    printf("  PASS: test_function_call_no_args\n");
}

// A call with two arguments keeps them by value in source order.
void test_function_call_with_args() {
    AstArgList args = (AstArgList){0};
    arg_push(&args, ast_exp_int(1));
    arg_push(&args, ast_exp_int(2));
    AstExp* call = ast_exp_function_call(strdup("add"), args);

    assert(call->as.funcall.args.count == 2);
    assert(call->as.funcall.args.items[0].kind == EXP_INT);
    assert(call->as.funcall.args.items[0].as.int_lit.value == 1);
    assert(call->as.funcall.args.items[1].as.int_lit.value == 2);
    ast_exp_destroy(call);
    printf("  PASS: test_function_call_with_args\n");
}

// A call may appear as another call's argument; destroying the outer call must
// recurse into the by-value inner call and free everything it owns (identifier
// and its own arguments). Exercises the recursive arg cleanup path.
void test_function_call_nested() {
    AstArgList inner_args = (AstArgList){0};
    arg_push(&inner_args, ast_exp_int(7));
    AstExp* inner = ast_exp_function_call(strdup("g"), inner_args);

    AstArgList outer_args = (AstArgList){0};
    arg_push(&outer_args, inner);
    AstExp* outer = ast_exp_function_call(strdup("f"), outer_args);

    assert(outer->as.funcall.args.count == 1);
    AstExp* arg = &outer->as.funcall.args.items[0];
    assert(arg->kind == EXP_FUNCTION_CALL);
    assert(strcmp(arg->as.funcall.identifier, "g") == 0);
    assert(arg->as.funcall.args.items[0].as.int_lit.value == 7);
    ast_exp_destroy(outer);
    printf("  PASS: test_function_call_nested\n");
}

// Arguments that own heap memory (a variable's identifier) are freed on destroy.
void test_function_call_owns_arg_contents() {
    AstArgList args = (AstArgList){0};
    arg_push(&args, ast_exp_var("x"));
    AstExp* call = ast_exp_function_call(strdup("h"), args);

    assert(call->as.funcall.args.items[0].kind == EXP_VAR);
    assert(strcmp(call->as.funcall.args.items[0].as.variable.identifier, "x") == 0);
    ast_exp_destroy(call);  // must free the arg's strdup'd identifier
    printf("  PASS: test_function_call_owns_arg_contents\n");
}

// A prototype carries its parameter names and no body.
void test_function_decl_with_params() {
    AstParamList params = (AstParamList){0};
    list_push(&params, strdup("a"));
    list_push(&params, strdup("b"));
    AstFunctionDeclaration fn =
        ast_function_declaration(strdup("f"), params, NONE(OptionalBlock));

    assert(strcmp(fn.identifier, "f") == 0);
    assert(fn.params.count == 2);
    assert(strcmp(fn.params.items[0], "a") == 0);
    assert(strcmp(fn.params.items[1], "b") == 0);
    assert(!fn.body.present);
    ast_function_declaration_destroy(&fn);
    printf("  PASS: test_function_decl_with_params\n");
}

// A prototype parameter may be unnamed (stored as NULL); destroy tolerates it.
void test_function_decl_unnamed_param() {
    AstParamList params = (AstParamList){0};
    list_push(&params, NULL);   // `int f(int);`
    AstFunctionDeclaration fn =
        ast_function_declaration(strdup("f"), params, NONE(OptionalBlock));

    assert(fn.params.count == 1);
    assert(fn.params.items[0] == NULL);
    ast_function_declaration_destroy(&fn);   // free(NULL) is a no-op
    printf("  PASS: test_function_decl_unnamed_param\n");
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
    test_function_prototype_has_no_body();
    printf("Running function construction tests...\n");
    test_function_call_no_args();
    test_function_call_with_args();
    test_function_call_nested();
    test_function_call_owns_arg_contents();
    test_function_decl_with_params();
    test_function_decl_unnamed_param();
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
