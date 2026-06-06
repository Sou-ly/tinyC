#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../src/codegen/x86/x86_ast.h"
#include "../src/codegen/codegen.h"
#include "../src/codegen/emit.h"
#include "../src/parser/ast.h"
#include "../src/ir/ir.h"

// --- asm_ast unit tests ---

void test_create_x86_function() {
    x86_InstrList instrs = x86_instr_list_new();
    x86_instr_list_append(&instrs, (x86_Instr){.kind = x86_MOV, .mov = {
        .dst = (x86_Operand){.kind = x86_REG, .reg = x86_AX},
        .src = (x86_Operand){.kind = x86_IMM, .imm = 5}
    }});
    x86_instr_list_append(&instrs, (x86_Instr){.kind = x86_RET});

    x86_Function fn = make_x86_function("main", instrs);
    assert(strcmp(fn.name, "main") == 0);
    assert(fn.instrs.head->kind == x86_MOV);
    assert(fn.instrs.head->next->kind == x86_RET);
    assert(fn.instrs.head->next->next == NULL);
    destroy_x86_function(&fn);
    printf("  PASS: test_create_x86_function\n");
}

void test_create_x86_program() {
    x86_InstrList instrs = x86_instr_list_new();
    x86_instr_list_append(&instrs, (x86_Instr){.kind = x86_RET});

    x86_Function* functions = malloc(sizeof(x86_Function));
    functions[0] = make_x86_function("foo", instrs);

    x86_Program prog = make_x86_program(functions, 1);
    assert(prog.num_functions == 1);
    assert(strcmp(prog.functions[0].name, "foo") == 0);
    destroy_x86_program(&prog);
    printf("  PASS: test_create_x86_program\n");
}

// --- helpers ---

static AstProgram make_test_program(AstExpression* expr) {
    AstStatement* body = malloc(sizeof(AstStatement));
    body[0] = make_return_stmt(expr);
    AstDeclaration* decls = malloc(sizeof(AstDeclaration));
    decls[0] = make_function_decl("main", body, 1);
    return make_program(decls, 1);
}

// --- codegen integration test ---

void test_codegen_return_2() {
    AstProgram program = make_test_program(create_int_expr(2));

    IrProgram ir = emit_ir(&program);
    x86_Program asm_prog = codegen(&ir);

    assert(asm_prog.num_functions == 1);
    assert(strcmp(asm_prog.functions[0].name, "main") == 0);
    x86_Instr* first = asm_prog.functions[0].instrs.head;
    assert(first->kind == x86_MOV);
    assert(first->mov.src.kind == x86_IMM);
    assert(first->mov.src.imm == 2);
    assert(first->next->kind == x86_RET);

    destroy_x86_program(&asm_prog);
    destroy_program(&program);
    printf("  PASS: test_codegen_return_2\n");
}

void test_codegen_return_0() {
    AstProgram program = make_test_program(create_int_expr(0));

    IrProgram ir = emit_ir(&program);
    x86_Program asm_prog = codegen(&ir);

    assert(asm_prog.functions[0].instrs.head->mov.src.imm == 0);

    destroy_x86_program(&asm_prog);
    destroy_program(&program);
    printf("  PASS: test_codegen_return_0\n");
}

// --- emit integration test ---

void test_emit_return_2() {
    AstProgram program = make_test_program(create_int_expr(2));

    IrProgram ir = emit_ir(&program);
    x86_Program asm_prog = codegen(&ir);

    char* buf = NULL;
    size_t buf_size = 0;
    FILE* out = open_memstream(&buf, &buf_size);
    emit_asm(&asm_prog, out);
    fclose(out);

    assert(strstr(buf, ".global _main") != NULL);
    assert(strstr(buf, "_main:") != NULL);
    assert(strstr(buf, "ret") != NULL);

    free(buf);
    destroy_x86_program(&asm_prog);
    destroy_program(&program);
    printf("  PASS: test_emit_return_2\n");
}

// --- return ~(-2) end-to-end test ---
// ~(-2) == 1
// IR should produce:
//   tmp0 = neg 2
//   tmp1 = not tmp0
//   return tmp1
// x86 (before register rename):
//   mov tmp0, $2      ; mov imm -> pseudo
//   negl tmp0          ; negate tmp0
//   mov tmp1, tmp0     ; mov pseudo -> pseudo
//   notl tmp1          ; complement tmp1
//   mov %eax, tmp1     ; mov pseudo -> reg
//   ret

void test_codegen_return_complement_neg2() {
    // Build AST: return ~(-2)
    AstExpression* lit = create_int_expr(2);
    AstExpression* neg = create_unary_expr(UNARY_MINUS, lit);
    AstExpression* comp = create_unary_expr(UNARY_NOT, neg);
    AstProgram program = make_test_program(comp);

    IrProgram ir = emit_ir(&program);
    x86_Program asm_prog = codegen(&ir);

    // Walk the instruction list and verify the sequence
    x86_Instr* i = asm_prog.functions[0].instrs.head;

    // mov tmp0, $2
    assert(i != NULL);
    assert(i->kind == x86_MOV);
    assert(i->mov.src.kind == x86_IMM);
    assert(i->mov.src.imm == 2);
    assert(i->mov.dst.kind == x86_ID);
    i = i->next;

    // negl tmp0
    assert(i != NULL);
    assert(i->kind == x86_UNOP);
    assert(i->unop.unop == x86_NEG);
    assert(i->unop.operand.kind == x86_ID);
    i = i->next;

    // mov tmp1, tmp0
    assert(i != NULL);
    assert(i->kind == x86_MOV);
    assert(i->mov.src.kind == x86_ID);
    assert(i->mov.dst.kind == x86_ID);
    i = i->next;

    // notl tmp1
    assert(i != NULL);
    assert(i->kind == x86_UNOP);
    assert(i->unop.unop == x86_NOT);
    assert(i->unop.operand.kind == x86_ID);
    i = i->next;

    // mov %eax, tmp1
    assert(i != NULL);
    assert(i->kind == x86_MOV);
    assert(i->mov.dst.kind == x86_REG);
    assert(i->mov.dst.reg == x86_AX);
    assert(i->mov.src.kind == x86_ID);
    i = i->next;

    // ret
    assert(i != NULL);
    assert(i->kind == x86_RET);
    assert(i->next == NULL);

    destroy_x86_program(&asm_prog);
    destroy_program(&program);
    printf("  PASS: test_codegen_return_complement_neg2\n");
}

void test_emit_return_complement_neg2() {
    AstExpression* lit = create_int_expr(2);
    AstExpression* neg = create_unary_expr(UNARY_MINUS, lit);
    AstExpression* comp = create_unary_expr(UNARY_NOT, neg);
    AstProgram program = make_test_program(comp);

    IrProgram ir = emit_ir(&program);
    x86_Program asm_prog = codegen(&ir);

    // Run all passes
    int stack_offset = rename_registers(&asm_prog.functions[0]);
    allocate_stack(&asm_prog.functions[0], stack_offset);

    char* buf = NULL;
    size_t buf_size = 0;
    FILE* out = open_memstream(&buf, &buf_size);
    emit_asm(&asm_prog, out);
    fclose(out);

    assert(strstr(buf, ".global _main") != NULL);
    assert(strstr(buf, "_main:") != NULL);
    assert(strstr(buf, "negl") != NULL);
    assert(strstr(buf, "notl") != NULL);
    assert(strstr(buf, "movl") != NULL);
    assert(strstr(buf, "subq") != NULL);
    assert(strstr(buf, "ret") != NULL);
    // No pseudo-registers should remain
    assert(strstr(buf, "<pseudo:") == NULL);

    free(buf);
    destroy_x86_program(&asm_prog);
    destroy_program(&program);
    printf("  PASS: test_emit_return_complement_neg2\n");
}

int main(void) {
    printf("Running codegen tests...\n");
    test_create_x86_function();
    test_create_x86_program();
    test_codegen_return_2();
    test_codegen_return_0();
    test_emit_return_2();
    test_codegen_return_complement_neg2();
    test_emit_return_complement_neg2();
    printf("All codegen tests passed!\n");
    return 0;
}
