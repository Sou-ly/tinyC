#define _GNU_SOURCE
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include "../src/parser/ast.h"
#include "../src/parser/parser.h"

// Tests for the typecheck pass and the SymbolTable that backs it.
//
// typecheck runs after resolve_variables (unique variable names, calls already
// resolved), so every whole-program test runs both passes in order. Error
// cases exit(1) with a message on stderr; both accept and reject cases run in
// a forked child so a crash or an unexpected exit cannot kill the suite.
//
// Failures don't abort the run: some tests document behavior that is still to
// be implemented, so the runner counts failures and reports at the end.

static int failures = 0;

// ---------------------------------------------------------------------------
// AST builders
// ---------------------------------------------------------------------------

static AstBlockItem stmt_item(AstStatement* stmt) {
    return (AstBlockItem){ .kind = AST_STATEMENT, .as.statement = stmt };
}

static AstBlockItem decl_item(const char* name, AstExp* initializer) {
    return (AstBlockItem){
        .kind = AST_DECLARATION,
        .as.declaration = ast_declaration_variable(
            ast_variable_declaration(strdup(name), initializer, STORAGE_UNSPECIFIED)),
    };
}

// A parameter list from a NULL-terminated array of names (each is strdup'd,
// since resolve_variables frees and replaces them).
static AstParamList params_of(const char* const* names) {
    AstParamList params = {0};
    for (size_t i = 0; names[i] != NULL; i++)
        list_push(&params, strdup(names[i]));
    return params;
}

// A call `name(0, 1, ..., argc-1)` — int-literal args, we only care about arity.
static AstExp* call_with_ints(const char* name, int argc) {
    AstArgList args = {0};
    for (int i = 0; i < argc; i++) {
        AstExp* arg = ast_exp_int(i);
        list_push(&args, *arg);
        free(arg);
    }
    return ast_exp_function_call(strdup(name), args);
}

// A function definition `name(params) { body }`.
static AstFunctionDeclaration func_def(const char* name, AstParamList params, AstBlock body) {
    return ast_function_declaration(strdup(name), params, SOME(OptionalBlock, body), STORAGE_UNSPECIFIED);
}

// A bodyless prototype `name(params);`.
static AstFunctionDeclaration func_proto(const char* name, AstParamList params) {
    return ast_function_declaration(strdup(name), params, NONE(OptionalBlock), STORAGE_UNSPECIFIED);
}

// A body that is just `return <exp>;`.
static AstBlock body_return(AstExp* exp) {
    AstBlock body = (AstBlock){0};
    ast_block_append(&body, stmt_item(ast_stmt_return(exp)));
    return body;
}

// A program from `count` function declarations (copied into heap storage that
// ast_program_destroy owns).
static AstProgram program_of_functions(const AstFunctionDeclaration* functions, int count) {
    AstDeclaration* heap = malloc(sizeof(AstDeclaration) * count);
    for (int i = 0; i < count; i++) heap[i] = ast_declaration_function(functions[i]);
    return ast_program_create(heap, count);
}

// `int foo(int a) { return a; }`
static AstFunctionDeclaration foo_identity_def(void) {
    return func_def("foo", params_of((const char*[]){"a", NULL}),
                    body_return(ast_exp_var("a")));
}

// `int main() { <body> }`
static AstFunctionDeclaration main_def(AstBlock body) {
    return func_def("main", (AstParamList){0}, body);
}

// ---------------------------------------------------------------------------
// SymbolTable unit tests
// ---------------------------------------------------------------------------

static Symbol int_symbol(const char* key) {
    return (Symbol){ .key = strdup(key), .kind = SYM_INT };
}

static Symbol func_symbol(const char* key, size_t param_count, bool defined) {
    return (Symbol){ .key = strdup(key), .kind = SYM_FUNCTION,
                     .param_count = param_count, .defined = defined };
}

// A lookup on an empty table returns NULL rather than a sentinel struct.
void test_symtab_get_miss_empty() {
    SymbolTable table = symtab_create(4);
    assert(symtab_get(&table, "x") == NULL);
    symtab_destroy(&table);
    printf("  PASS: test_symtab_get_miss_empty\n");
}

// put then get returns the stored symbol with every field intact.
void test_symtab_put_get_hit() {
    SymbolTable table = symtab_create(4);
    symtab_put(&table, func_symbol("foo", 2, true));
    symtab_put(&table, int_symbol("x.0"));

    Symbol* foo = symtab_get(&table, "foo");
    assert(foo != NULL);
    assert(foo->kind == SYM_FUNCTION);
    assert(foo->param_count == 2);
    assert(foo->defined == true);

    Symbol* x = symtab_get(&table, "x.0");
    assert(x != NULL);
    assert(x->kind == SYM_INT);

    assert(symtab_get(&table, "bar") == NULL);  // miss still NULL with entries present
    symtab_destroy(&table);
    printf("  PASS: test_symtab_put_get_hit\n");
}

// get returns a pointer into the table, so an entry can be updated in place —
// this is how a prototype's `defined` flips when its definition shows up.
void test_symtab_update_through_pointer() {
    SymbolTable table = symtab_create(4);
    symtab_put(&table, func_symbol("foo", 1, false));

    symtab_get(&table, "foo")->defined = true;
    assert(symtab_get(&table, "foo")->defined == true);

    symtab_destroy(&table);
    printf("  PASS: test_symtab_update_through_pointer\n");
}

// Pushing past the initial capacity grows the table without losing entries.
void test_symtab_growth() {
    SymbolTable table = symtab_create(2);
    char name[16];
    for (int i = 0; i < 20; i++) {
        snprintf(name, sizeof(name), "v.%d", i);
        symtab_put(&table, int_symbol(name));
    }
    for (int i = 0; i < 20; i++) {
        snprintf(name, sizeof(name), "v.%d", i);
        assert(symtab_get(&table, name) != NULL);
    }
    symtab_destroy(&table);
    printf("  PASS: test_symtab_growth\n");
}

// ---------------------------------------------------------------------------
// typecheck — forked pass/reject harness
// ---------------------------------------------------------------------------

// Runs `build()` through resolve_variables + typecheck in a forked child
// (stderr silenced) and checks the child's exit status: 0 when the program
// must be accepted, non-zero when it must be rejected. A crash (signal) is
// always a failure.
static void run_typecheck_case(const char* description, AstProgram (*build)(void),
                               bool expect_ok) {
    fflush(stdout);
    pid_t pid = fork();
    assert(pid >= 0);
    if (pid == 0) {
        freopen("/dev/null", "w", stderr);
        AstProgram prog = build();
        resolve_variables(&prog);
        typecheck(&prog);
        ast_program_destroy(&prog);
        _exit(0);
    }
    int status = 0;
    assert(waitpid(pid, &status, 0) == pid);

    if (!WIFEXITED(status)) {
        printf("  FAIL: %s (child crashed)\n", description);
        failures++;
        return;
    }
    bool ok = WEXITSTATUS(status) == 0;
    if (ok != expect_ok) {
        printf("  FAIL: %s (%s)\n", description,
               expect_ok ? "expected acceptance, got an error"
                         : "expected typecheck error, none occurred");
        failures++;
        return;
    }
    printf("  PASS: %s\n", description);
}

static void expect_typecheck_ok(const char* description, AstProgram (*build)(void)) {
    run_typecheck_case(description, build, true);
}

static void expect_typecheck_error(const char* description, AstProgram (*build)(void)) {
    run_typecheck_case(description, build, false);
}

// ---------------------------------------------------------------------------
// typecheck — programs that must be accepted
// ---------------------------------------------------------------------------

// int foo(int a); int main() { return foo(1); } int foo(int a) { return a; }
static AstProgram build_prototype_call_definition(void) {
    AstFunctionDeclaration decls[3];
    decls[0] = func_proto("foo", params_of((const char*[]){"a", NULL}));
    decls[1] = main_def(body_return(call_with_ints("foo", 1)));
    decls[2] = foo_identity_def();
    return program_of_functions(decls, 3);
}

// int main() { return foo(1); } int foo(int a) { return a; }
// No prototype: the callee is declared later at top level, which the resolver
// accepts, so typecheck must accept it too (and must not crash on the
// not-yet-recorded name).
static AstProgram build_call_before_definition(void) {
    AstFunctionDeclaration decls[2];
    decls[0] = main_def(body_return(call_with_ints("foo", 1)));
    decls[1] = foo_identity_def();
    return program_of_functions(decls, 2);
}

// int foo(int a); int foo(int a); — repeating a prototype is fine.
static AstProgram build_repeated_prototypes(void) {
    AstFunctionDeclaration decls[2];
    decls[0] = func_proto("foo", params_of((const char*[]){"a", NULL}));
    decls[1] = func_proto("foo", params_of((const char*[]){"a", NULL}));
    return program_of_functions(decls, 2);
}

// int foo(int a) { return a; } int foo(int a); — a prototype after the
// definition is legal C; only a second *definition* is an error.
static AstProgram build_prototype_after_definition(void) {
    AstFunctionDeclaration decls[2];
    decls[0] = foo_identity_def();
    decls[1] = func_proto("foo", params_of((const char*[]){"a", NULL}));
    return program_of_functions(decls, 2);
}

// ---------------------------------------------------------------------------
// typecheck — programs that must be rejected
// ---------------------------------------------------------------------------

// int foo(int a) { return a; } int foo(int a) { return a; }
static AstProgram build_defined_twice(void) {
    AstFunctionDeclaration decls[2];
    decls[0] = foo_identity_def();
    decls[1] = foo_identity_def();
    return program_of_functions(decls, 2);
}

// int foo(int a); int foo(int a) { return a; } int foo(int a) { return a; }
// The second definition must still be caught when a prototype came first —
// the table's entry for foo has to be updated in place, not shadowed.
static AstProgram build_prototype_then_two_definitions(void) {
    AstFunctionDeclaration decls[3];
    decls[0] = func_proto("foo", params_of((const char*[]){"a", NULL}));
    decls[1] = foo_identity_def();
    decls[2] = foo_identity_def();
    return program_of_functions(decls, 3);
}

// int foo(int a); int foo(int a, int b); — same name, different arity.
static AstProgram build_conflicting_arity(void) {
    AstFunctionDeclaration decls[2];
    decls[0] = func_proto("foo", params_of((const char*[]){"a", NULL}));
    decls[1] = func_proto("foo", params_of((const char*[]){"a", "b", NULL}));
    return program_of_functions(decls, 2);
}

// int foo(int a) { return a; } int main() { return foo(1, 2); }
static AstProgram build_call_wrong_arg_count(void) {
    AstFunctionDeclaration decls[2];
    decls[0] = foo_identity_def();
    decls[1] = main_def(body_return(call_with_ints("foo", 2)));
    return program_of_functions(decls, 2);
}

// int main() { int x = 3; return x(1); }
static AstProgram build_variable_called_as_function(void) {
    AstBlock body = (AstBlock){0};
    ast_block_append(&body, decl_item("x", ast_exp_int(3)));
    ast_block_append(&body, stmt_item(ast_stmt_return(call_with_ints("x", 1))));
    AstFunctionDeclaration decls[1] = { main_def(body) };
    return program_of_functions(decls, 1);
}

// int foo(int a) { return a; } int main() { return foo; }
static AstProgram build_function_used_as_variable(void) {
    AstFunctionDeclaration decls[2];
    decls[0] = foo_identity_def();
    decls[1] = main_def(body_return(ast_exp_var("foo")));
    return program_of_functions(decls, 2);
}

int main() {
    printf("SymbolTable unit tests:\n");
    test_symtab_get_miss_empty();
    test_symtab_put_get_hit();
    test_symtab_update_through_pointer();
    test_symtab_growth();

    printf("typecheck accepts:\n");
    expect_typecheck_ok("prototype, call, then definition", build_prototype_call_definition);
    expect_typecheck_ok("call before the callee's definition", build_call_before_definition);
    expect_typecheck_ok("repeated prototypes with the same arity", build_repeated_prototypes);
    expect_typecheck_ok("prototype after the definition", build_prototype_after_definition);

    printf("typecheck rejects:\n");
    expect_typecheck_error("function defined twice", build_defined_twice);
    expect_typecheck_error("prototype then two definitions", build_prototype_then_two_definitions);
    expect_typecheck_error("conflicting arity between declarations", build_conflicting_arity);
    expect_typecheck_error("call with the wrong number of arguments", build_call_wrong_arg_count);
    expect_typecheck_error("variable called like a function", build_variable_called_as_function);
    expect_typecheck_error("function name used as a variable", build_function_used_as_variable);

    if (failures > 0) {
        printf("%d typecheck test(s) failing\n", failures);
        return 1;
    }
    printf("All typecheck tests passed!\n");
    return 0;
}
