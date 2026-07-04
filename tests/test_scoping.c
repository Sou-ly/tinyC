#define _GNU_SOURCE
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include "../src/parser/ast.h"
#include "../src/parser/parser.h"

// Tests for the variable-resolution / scoping pass (resolve_variables) and
// the VarMap that backs it.
//
// The pass renames every declaration to a unique name (`name.N`, N counting
// up from 0 each call) and rewrites references to match. It rejects duplicate
// declarations *in the same scope* and references to undeclared variables by
// printing to stderr and calling exit(1) — those cases are checked by forking
// a child and inspecting its exit status.

// ---------------------------------------------------------------------------
// AST builders
// ---------------------------------------------------------------------------

static AstBlockItem decl_item(const char* name, AstExp* initializer) {
    OptionalExp opt = initializer ? some_exp(initializer) : no_exp();
    return (AstBlockItem){
        .type = AST_DECLARATION,
        .as.decl = { .identifier = strdup(name), .init = opt },
    };
}

static AstBlockItem stmt_item(AstStatement* stmt) {
    return (AstBlockItem){ .type = AST_STATEMENT, .as.stmt = stmt };
}

static AstBlockItem return_var(const char* name) {
    return stmt_item(make_return_stmt(create_variable_exp(name)));
}

// Wrap a finished body block into a single-function program. destroy_program
// frees the functions array, so it must be heap-allocated.
static AstProgram program_with_body(AstBlock body) {
    AstFunction* functions = malloc(sizeof(AstFunction));
    functions[0] = ast_function_make("main", body);
    return ast_program_create(functions, 1);
}

// Convenience accessors into `main`'s top-level body.
static AstBlockItem* item(AstProgram* prog, size_t index) {
    return &prog->functions[0].body.items[index];
}

static const char* decl_name(AstProgram* prog, size_t index) {
    return item(prog, index)->as.decl.identifier;
}

// ---------------------------------------------------------------------------
// VarMap unit tests
// ---------------------------------------------------------------------------

static VarMapEntry owned_entry(const char* key, const char* val, bool cur) {
    return (VarMapEntry){ .key = strdup(key), .val = strdup(val), .is_cur_scope = cur };
}

// A lookup on an empty map returns NULL rather than a sentinel struct.
void test_varmap_get_miss_empty() {
    VarMap map = varmap_create(4);
    assert(varmap_get(&map, "x") == NULL);
    varmap_destroy(&map);
    printf("  PASS: test_varmap_get_miss_empty\n");
}

// put then get returns the stored entry, with its value and scope flag intact.
void test_varmap_put_get_hit() {
    VarMap map = varmap_create(4);
    varmap_put(&map, owned_entry("a", "a.0", true));

    VarMapEntry* found = varmap_get(&map, "a");
    assert(found != NULL);
    assert(strcmp(found->val, "a.0") == 0);
    assert(found->is_cur_scope == true);

    assert(varmap_get(&map, "b") == NULL);  // miss still NULL with entries present
    varmap_destroy(&map);
    printf("  PASS: test_varmap_put_get_hit\n");
}

// get searches backwards, so a later entry shadows an earlier one with the
// same key — this is what lets an inner-scope binding win over an outer one.
void test_varmap_get_returns_latest() {
    VarMap map = varmap_create(4);
    varmap_put(&map, owned_entry("a", "a.0", false));
    varmap_put(&map, owned_entry("a", "a.1", true));

    VarMapEntry* found = varmap_get(&map, "a");
    assert(found != NULL);
    assert(strcmp(found->val, "a.1") == 0);
    varmap_destroy(&map);
    printf("  PASS: test_varmap_get_returns_latest\n");
}

// Growth past the initial capacity preserves every entry.
void test_varmap_grows_past_capacity() {
    VarMap map = varmap_create(2);
    char key[16], val[16];
    for (int i = 0; i < 50; i++) {
        snprintf(key, sizeof(key), "v%d", i);
        snprintf(val, sizeof(val), "v%d.x", i);
        varmap_put(&map, owned_entry(key, val, true));
    }
    assert(map.size == 50);
    for (int i = 0; i < 50; i++) {
        snprintf(key, sizeof(key), "v%d", i);
        snprintf(val, sizeof(val), "v%d.x", i);
        VarMapEntry* found = varmap_get(&map, key);
        assert(found != NULL && strcmp(found->val, val) == 0);
    }
    varmap_destroy(&map);
    printf("  PASS: test_varmap_grows_past_capacity\n");
}

// A copy is a deep copy: independent string storage, equal contents, and every
// entry demoted to is_cur_scope=false (it now belongs to an enclosing scope).
void test_varmap_copy_is_deep_and_demotes_scope() {
    VarMap map = varmap_create(4);
    varmap_put(&map, owned_entry("a", "a.0", true));
    varmap_put(&map, owned_entry("b", "b.1", true));

    VarMap copy = varmap_copy(map);
    assert(copy.size == 2);
    for (int i = 0; i < copy.size; i++) {
        // same contents...
        assert(strcmp(copy.entries[i].key, map.entries[i].key) == 0);
        assert(strcmp(copy.entries[i].val, map.entries[i].val) == 0);
        // ...but distinct storage...
        assert(copy.entries[i].key != map.entries[i].key);
        assert(copy.entries[i].val != map.entries[i].val);
        // ...and demoted to outer scope.
        assert(copy.entries[i].is_cur_scope == false);
    }
    // mutating the original's entries does not disturb the copy
    varmap_put(&map, owned_entry("c", "c.2", true));
    assert(copy.size == 2);
    assert(varmap_get(&copy, "c") == NULL);

    varmap_destroy(&map);
    varmap_destroy(&copy);
    printf("  PASS: test_varmap_copy_is_deep_and_demotes_scope\n");
}

void test_varmap_copy_empty() {
    VarMap map = varmap_create(4);
    VarMap copy = varmap_copy(map);
    assert(copy.size == 0);
    assert(varmap_get(&copy, "a") == NULL);
    varmap_destroy(&map);
    varmap_destroy(&copy);
    printf("  PASS: test_varmap_copy_empty\n");
}

// ---------------------------------------------------------------------------
// resolve_variables — success cases
// ---------------------------------------------------------------------------

// A lone declaration is renamed and its reference rewritten to match.
void test_resolve_single_declaration() {
    AstBlock body = ast_block_make(4);
    ast_block_append(&body, decl_item("a", NULL));
    ast_block_append(&body, return_var("a"));
    AstProgram prog = program_with_body(body);

    resolve_variables(&prog);

    assert(strcmp(decl_name(&prog, 0), "a.0") == 0);
    AstExp* ret = item(&prog, 1)->as.stmt->as.ret.exp;
    assert(ret->kind == EXP_VAR);
    assert(strcmp(ret->as.variable.identifier, "a.0") == 0);

    destroy_program(&prog);
    printf("  PASS: test_resolve_single_declaration\n");
}

// Distinct declarations get distinct unique names, numbered in source order.
void test_resolve_distinct_names() {
    AstBlock body = ast_block_make(4);
    ast_block_append(&body, decl_item("a", NULL));
    ast_block_append(&body, decl_item("b", NULL));
    AstProgram prog = program_with_body(body);

    resolve_variables(&prog);

    assert(strcmp(decl_name(&prog, 0), "a.0") == 0);
    assert(strcmp(decl_name(&prog, 1), "b.1") == 0);

    destroy_program(&prog);
    printf("  PASS: test_resolve_distinct_names\n");
}

// A reference inside an initializer resolves against earlier declarations.
void test_resolve_initializer_reference() {
    AstBlock body = ast_block_make(4);
    ast_block_append(&body, decl_item("a", create_int_exp(5)));
    ast_block_append(&body, decl_item("b", create_variable_exp("a")));
    AstProgram prog = program_with_body(body);

    resolve_variables(&prog);

    assert(strcmp(decl_name(&prog, 0), "a.0") == 0);
    assert(strcmp(decl_name(&prog, 1), "b.1") == 0);
    AstExp* init = item(&prog, 1)->as.decl.init.exp;
    assert(init->kind == EXP_VAR);
    assert(strcmp(init->as.variable.identifier, "a.0") == 0);

    destroy_program(&prog);
    printf("  PASS: test_resolve_initializer_reference\n");
}

// A nested block may redeclare an outer name; the two get different unique
// names and a reference inside the block binds to the inner declaration.
void test_resolve_inner_shadows_outer() {
    AstBlock inner = ast_block_make(4);
    ast_block_append(&inner, decl_item("a", NULL));
    ast_block_append(&inner, return_var("a"));

    AstBlock body = ast_block_make(4);
    ast_block_append(&body, decl_item("a", NULL));
    ast_block_append(&body, stmt_item(make_compound_stmt(inner)));
    AstProgram prog = program_with_body(body);

    resolve_variables(&prog);

    assert(strcmp(decl_name(&prog, 0), "a.0") == 0);  // outer
    AstBlock* blk = &item(&prog, 1)->as.stmt->as.compound;
    assert(strcmp(blk->items[0].as.decl.identifier, "a.1") == 0);  // inner shadow
    AstExp* ret = blk->items[1].as.stmt->as.ret.exp;
    assert(strcmp(ret->as.variable.identifier, "a.1") == 0);  // binds to inner

    destroy_program(&prog);
    printf("  PASS: test_resolve_inner_shadows_outer\n");
}

// A nested block with no local declaration sees the enclosing scope's binding.
void test_resolve_inner_reads_outer() {
    AstBlock inner = ast_block_make(4);
    ast_block_append(&inner, return_var("a"));

    AstBlock body = ast_block_make(4);
    ast_block_append(&body, decl_item("a", NULL));
    ast_block_append(&body, stmt_item(make_compound_stmt(inner)));
    AstProgram prog = program_with_body(body);

    resolve_variables(&prog);

    assert(strcmp(decl_name(&prog, 0), "a.0") == 0);
    AstBlock* blk = &item(&prog, 1)->as.stmt->as.compound;
    AstExp* ret = blk->items[0].as.stmt->as.ret.exp;
    assert(strcmp(ret->as.variable.identifier, "a.0") == 0);  // outer binding

    destroy_program(&prog);
    printf("  PASS: test_resolve_inner_reads_outer\n");
}

// Sibling blocks are independent scopes: each may declare the same name, and
// the two declarations get distinct unique names.
void test_resolve_sibling_scopes() {
    AstBlock first = ast_block_make(4);
    ast_block_append(&first, decl_item("a", NULL));
    AstBlock second = ast_block_make(4);
    ast_block_append(&second, decl_item("a", NULL));

    AstBlock body = ast_block_make(4);
    ast_block_append(&body, stmt_item(make_compound_stmt(first)));
    ast_block_append(&body, stmt_item(make_compound_stmt(second)));
    AstProgram prog = program_with_body(body);

    resolve_variables(&prog);

    const char* a0 = item(&prog, 0)->as.stmt->as.compound.items[0].as.decl.identifier;
    const char* a1 = item(&prog, 1)->as.stmt->as.compound.items[0].as.decl.identifier;
    assert(strcmp(a0, "a.0") == 0);
    assert(strcmp(a1, "a.1") == 0);

    destroy_program(&prog);
    printf("  PASS: test_resolve_sibling_scopes\n");
}

// Declarations inside a block do not leak out: a reference after the block
// resolves to the outer declaration, not the inner shadow.
void test_resolve_inner_decl_does_not_leak() {
    AstBlock inner = ast_block_make(4);
    ast_block_append(&inner, decl_item("a", NULL));

    AstBlock body = ast_block_make(4);
    ast_block_append(&body, decl_item("a", NULL));                       // a.0 (outer)
    ast_block_append(&body, stmt_item(make_compound_stmt(inner)));       // a.1 (inner)
    ast_block_append(&body, return_var("a"));                            // -> a.0
    AstProgram prog = program_with_body(body);

    resolve_variables(&prog);

    AstExp* ret = item(&prog, 2)->as.stmt->as.ret.exp;
    assert(strcmp(ret->as.variable.identifier, "a.0") == 0);

    destroy_program(&prog);
    printf("  PASS: test_resolve_inner_decl_does_not_leak\n");
}

// make_*_stmt already heap-allocates, so an if-statement's branch pointers can
// own the result directly (resolve rewrites the branches in place; destroy_stmt
// frees them). This pass-through is kept for readability at the call sites.
static AstStatement* heap_stmt(AstStatement* stmt) {
    return stmt;
}

// An if-statement resolves its condition and both branches against the current
// scope (an unbraced if introduces no scope of its own), rewriting every
// reference to the matching unique name.
void test_resolve_if_statement() {
    AstBlock body = ast_block_make(4);
    ast_block_append(&body, decl_item("a", NULL));   // a.0
    ast_block_append(&body, decl_item("b", NULL));   // b.1
    // if (a) return b; else return a;
    ast_block_append(&body, stmt_item(make_if_stmt(
        create_variable_exp("a"),
        heap_stmt(make_return_stmt(create_variable_exp("b"))),
        heap_stmt(make_return_stmt(create_variable_exp("a"))))));
    AstProgram prog = program_with_body(body);

    resolve_variables(&prog);

    assert(strcmp(decl_name(&prog, 0), "a.0") == 0);
    assert(strcmp(decl_name(&prog, 1), "b.1") == 0);

    AstStmtIf* if_stmt = &item(&prog, 2)->as.stmt->as.if_cond;
    assert(strcmp(if_stmt->cond->as.variable.identifier, "a.0") == 0);
    assert(strcmp(if_stmt->then_br->as.ret.exp->as.variable.identifier, "b.1") == 0);
    assert(strcmp(if_stmt->else_br->as.ret.exp->as.variable.identifier, "a.0") == 0);

    destroy_program(&prog);
    printf("  PASS: test_resolve_if_statement\n");
}

// A conditional (ternary) expression resolves all three sub-expressions, so
// each variable reference inside it is rewritten to its unique name.
void test_resolve_conditional_expression() {
    AstBlock body = ast_block_make(4);
    ast_block_append(&body, decl_item("a", NULL));   // a.0
    ast_block_append(&body, decl_item("b", NULL));   // b.1
    // return a ? a : b;
    ast_block_append(&body, stmt_item(make_return_stmt(create_conditional_exp(
        create_variable_exp("a"),
        create_variable_exp("a"),
        create_variable_exp("b")))));
    AstProgram prog = program_with_body(body);

    resolve_variables(&prog);

    AstExp* cond = item(&prog, 2)->as.stmt->as.ret.exp;
    assert(cond->kind == EXP_CONDITIONAL);
    assert(strcmp(cond->as.conditional.lhs->as.variable.identifier, "a.0") == 0);
    assert(strcmp(cond->as.conditional.mid->as.variable.identifier, "a.0") == 0);
    assert(strcmp(cond->as.conditional.rhs->as.variable.identifier, "b.1") == 0);

    destroy_program(&prog);
    printf("  PASS: test_resolve_conditional_expression\n");
}

// ---------------------------------------------------------------------------
// resolve_variables — error cases (exit(1))
// ---------------------------------------------------------------------------

// Runs `build()` through resolve_variables in a forked child (stderr silenced)
// and asserts the child exits non-zero, i.e. the pass rejected the program.
static void expect_resolve_error(const char* description, AstBlock (*build)(void)) {
    fflush(stdout);
    pid_t pid = fork();
    assert(pid >= 0);
    if (pid == 0) {
        freopen("/dev/null", "w", stderr);
        AstProgram prog = program_with_body(build());
        resolve_variables(&prog);  // expected to exit(1) before returning
        destroy_program(&prog);
        _exit(0);                        // reached only if it wrongly succeeded
    }
    int status = 0;
    assert(waitpid(pid, &status, 0) == pid);
    if (!(WIFEXITED(status) && WEXITSTATUS(status) != 0)) {
        printf("  FAIL: %s (expected resolution error, none occurred)\n", description);
        exit(1);
    }
    printf("  PASS: %s\n", description);
}

static AstBlock build_duplicate_same_scope(void) {
    AstBlock body = ast_block_make(4);
    ast_block_append(&body, decl_item("a", NULL));
    ast_block_append(&body, decl_item("a", NULL));
    return body;
}

static AstBlock build_undeclared_use(void) {
    AstBlock body = ast_block_make(4);
    ast_block_append(&body, return_var("a"));
    return body;
}

static AstBlock build_inner_decl_used_outside(void) {
    AstBlock inner = ast_block_make(4);
    ast_block_append(&inner, decl_item("a", NULL));
    AstBlock body = ast_block_make(4);
    ast_block_append(&body, stmt_item(make_compound_stmt(inner)));
    ast_block_append(&body, return_var("a"));  // a not visible out here
    return body;
}

static AstBlock build_duplicate_in_inner_scope(void) {
    // Outer `a` is fine to shadow once, but two `a` in the inner scope is a
    // duplicate — confirms is_cur_scope flags the inner redeclaration even
    // though an outer binding with the same name exists.
    AstBlock inner = ast_block_make(4);
    ast_block_append(&inner, decl_item("a", NULL));
    ast_block_append(&inner, decl_item("a", NULL));
    AstBlock body = ast_block_make(4);
    ast_block_append(&body, decl_item("a", NULL));
    ast_block_append(&body, stmt_item(make_compound_stmt(inner)));
    return body;
}

void test_resolve_errors() {
    expect_resolve_error("duplicate declaration in same scope", build_duplicate_same_scope);
    expect_resolve_error("use of undeclared variable", build_undeclared_use);
    expect_resolve_error("inner declaration used outside its scope", build_inner_decl_used_outside);
    expect_resolve_error("duplicate declaration in inner scope", build_duplicate_in_inner_scope);
}

int main(void) {
    printf("Running scoping tests...\n");

    test_varmap_get_miss_empty();
    test_varmap_put_get_hit();
    test_varmap_get_returns_latest();
    test_varmap_grows_past_capacity();
    test_varmap_copy_is_deep_and_demotes_scope();
    test_varmap_copy_empty();

    test_resolve_single_declaration();
    test_resolve_distinct_names();
    test_resolve_initializer_reference();
    test_resolve_inner_shadows_outer();
    test_resolve_inner_reads_outer();
    test_resolve_sibling_scopes();
    test_resolve_inner_decl_does_not_leak();
    test_resolve_if_statement();
    test_resolve_conditional_expression();

    test_resolve_errors();

    printf("All scoping tests passed!\n");
    return 0;
}
