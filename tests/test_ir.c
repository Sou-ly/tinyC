#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../parser/ast.h"
#include "../ir/ir.h"

// --- helpers ---

// Wrap a list of statements into a single-function ("main") program.
static AstProgram* program_of(AstStatement** stmts, int num_stmts) {
    AstDeclaration** decls = malloc(sizeof(AstDeclaration*));
    decls[0] = create_function_decl("main", stmts, num_stmts);
    return create_program(decls, 1);
}

// Wrap a single statement into a one-statement "main" program.
static AstProgram* program_of_stmt(AstStatement* stmt) {
    AstStatement** body = malloc(sizeof(AstStatement*));
    body[0] = stmt;
    return program_of(body, 1);
}

// A temp's name is allocated once (at the producing instruction's dst) and
// shallow-copied into later uses, so free it only where it was produced.
static void free_ir_program(IrProgram* program) {
    for (int f = 0; f < program->size; f++) {
        IrFunction* fn = &program->functions[f];
        for (int i = 0; i < fn->size; i++) {
            IrInstruction* ins = &fn->instructions[i];
            if (ins->type == IR_UNARY && ins->unary.dst.kind == IR_VARIABLE) {
                free(ins->unary.dst.name);
            }
        }
        free(fn->instructions);
        free(fn->name);
    }
    free(program->functions);
}

// --- tests ---

// int main() { return 2; }
void test_emit_return_constant() {
    AstProgram* ast = program_of_stmt(create_return_stmt(create_int_expr(2)));
    IrProgram ir = emit_ir(ast);

    assert(ir.size == 1);
    IrFunction* fn = &ir.functions[0];
    assert(strcmp(fn->name, "main") == 0);
    assert(fn->size == 1);

    IrInstruction* ret = &fn->instructions[0];
    assert(ret->type == IR_RETURN);
    assert(ret->ret.val.kind == IR_CONSTANT);
    assert(ret->ret.val.int_val == 2);

    free_ir_program(&ir);
    destroy_program(ast);
    printf("  PASS: test_emit_return_constant\n");
}

// The IR function name must be an independent copy of the AST name.
void test_emit_name_is_owned_copy() {
    AstProgram* ast = program_of_stmt(create_return_stmt(create_int_expr(0)));
    IrProgram ir = emit_ir(ast);

    assert(ir.functions[0].name != ast->decls[0]->function.name);
    assert(strcmp(ir.functions[0].name, "main") == 0);

    free_ir_program(&ir);
    destroy_program(ast);
    printf("  PASS: test_emit_name_is_owned_copy\n");
}

// int main() { return -5; }  ->  unary NEG into a temp, then return the temp.
void test_emit_return_negate() {
    AstProgram* ast = program_of_stmt(
        create_return_stmt(create_unary_expr(UNARY_MINUS, create_int_expr(5))));
    IrProgram ir = emit_ir(ast);

    IrFunction* fn = &ir.functions[0];
    assert(fn->size == 2);

    IrInstruction* unary = &fn->instructions[0];
    assert(unary->type == IR_UNARY);
    assert(unary->unary.op == IR_NEG);
    assert(unary->unary.src.kind == IR_CONSTANT);
    assert(unary->unary.src.int_val == 5);
    assert(unary->unary.dst.kind == IR_VARIABLE);

    IrInstruction* ret = &fn->instructions[1];
    assert(ret->type == IR_RETURN);
    assert(ret->ret.val.kind == IR_VARIABLE);
    // The return value is exactly the temp produced by the unary op.
    assert(strcmp(ret->ret.val.name, unary->unary.dst.name) == 0);

    free_ir_program(&ir);
    destroy_program(ast);
    printf("  PASS: test_emit_return_negate\n");
}

// int main() { return ~3; }  ->  NOT maps to the complement op.
void test_emit_return_complement() {
    AstProgram* ast = program_of_stmt(
        create_return_stmt(create_unary_expr(UNARY_NOT, create_int_expr(3))));
    IrProgram ir = emit_ir(ast);

    IrFunction* fn = &ir.functions[0];
    assert(fn->size == 2);
    assert(fn->instructions[0].type == IR_UNARY);
    assert(fn->instructions[0].unary.op == IR_COMP);
    assert(fn->instructions[0].unary.src.int_val == 3);

    free_ir_program(&ir);
    destroy_program(ast);
    printf("  PASS: test_emit_return_complement\n");
}

// int main() { return -(~5); }  ->  two temps chained, then return the outer.
void test_emit_nested_unary() {
    AstExpression* inner = create_unary_expr(UNARY_NOT, create_int_expr(5));
    AstExpression* outer = create_unary_expr(UNARY_MINUS, inner);
    AstProgram* ast = program_of_stmt(create_return_stmt(outer));
    IrProgram ir = emit_ir(ast);

    IrFunction* fn = &ir.functions[0];
    assert(fn->size == 3);

    IrInstruction* in0 = &fn->instructions[0];  // ~5 -> t0
    IrInstruction* in1 = &fn->instructions[1];  // -t0 -> t1
    IrInstruction* in2 = &fn->instructions[2];  // return t1

    assert(in0->type == IR_UNARY && in0->unary.op == IR_COMP);
    assert(in0->unary.src.kind == IR_CONSTANT && in0->unary.src.int_val == 5);

    assert(in1->type == IR_UNARY && in1->unary.op == IR_NEG);
    // The outer op consumes the inner op's result.
    assert(in1->unary.src.kind == IR_VARIABLE);
    assert(strcmp(in1->unary.src.name, in0->unary.dst.name) == 0);

    assert(in2->type == IR_RETURN);
    assert(strcmp(in2->ret.val.name, in1->unary.dst.name) == 0);

    free_ir_program(&ir);
    destroy_program(ast);
    printf("  PASS: test_emit_nested_unary\n");
}

// A bare-constant expression statement produces no instruction.
// int main() { 5; return 0; }  ->  only the return is emitted.
void test_emit_expr_statement_no_instruction() {
    AstStatement** body = malloc(2 * sizeof(AstStatement*));
    body[0] = create_expr_stmt(create_int_expr(5));
    body[1] = create_return_stmt(create_int_expr(0));
    AstProgram* ast = program_of(body, 2);
    IrProgram ir = emit_ir(ast);

    IrFunction* fn = &ir.functions[0];
    assert(fn->size == 1);
    assert(fn->instructions[0].type == IR_RETURN);

    free_ir_program(&ir);
    destroy_program(ast);
    printf("  PASS: test_emit_expr_statement_no_instruction\n");
}

// A program with more than one function lowers each independently.
void test_emit_multiple_functions() {
    AstStatement** body_a = malloc(sizeof(AstStatement*));
    body_a[0] = create_return_stmt(create_int_expr(1));
    AstStatement** body_b = malloc(sizeof(AstStatement*));
    body_b[0] = create_return_stmt(create_int_expr(2));

    AstDeclaration** decls = malloc(2 * sizeof(AstDeclaration*));
    decls[0] = create_function_decl("foo", body_a, 1);
    decls[1] = create_function_decl("bar", body_b, 1);
    AstProgram* ast = create_program(decls, 2);
    IrProgram ir = emit_ir(ast);

    assert(ir.size == 2);
    assert(strcmp(ir.functions[0].name, "foo") == 0);
    assert(strcmp(ir.functions[1].name, "bar") == 0);
    assert(ir.functions[0].instructions[0].ret.val.int_val == 1);
    assert(ir.functions[1].instructions[0].ret.val.int_val == 2);

    free_ir_program(&ir);
    destroy_program(ast);
    printf("  PASS: test_emit_multiple_functions\n");
}

int main(void) {
    printf("Running IR tests...\n");
    test_emit_return_constant();
    test_emit_name_is_owned_copy();
    test_emit_return_negate();
    test_emit_return_complement();
    test_emit_nested_unary();
    test_emit_expr_statement_no_instruction();
    test_emit_multiple_functions();
    printf("All IR tests passed!\n");
    return 0;
}
