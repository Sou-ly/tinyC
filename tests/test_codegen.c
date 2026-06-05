#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../codegen/asm_ast.h"
#include "../codegen/codegen.h"
#include "../codegen/emit.h"
#include "../parser/ast.h"

// --- asm_ast unit tests ---

void test_asm_reg() {
    AsmOperand op = asm_reg("w0");
    assert(op.kind == OPERAND_REG);
    assert(strcmp(op.reg, "w0") == 0);
    destroy_operand(&op);
    printf("  PASS: test_asm_reg\n");
}

void test_asm_imm() {
    AsmOperand op = asm_imm(42);
    assert(op.kind == OPERAND_IMM);
    assert(op.imm == 42);
    printf("  PASS: test_asm_imm\n");
}

void test_asm_mov() {
    AsmInstr instr = asm_mov(asm_reg("w0"), asm_imm(7));
    assert(instr.kind == ASM_MOV);
    assert(instr.num_operands == 2);
    assert(instr.operands[0].kind == OPERAND_REG);
    assert(strcmp(instr.operands[0].reg, "w0") == 0);
    assert(instr.operands[1].kind == OPERAND_IMM);
    assert(instr.operands[1].imm == 7);
    destroy_instr(&instr);
    printf("  PASS: test_asm_mov\n");
}

void test_asm_ret() {
    AsmInstr instr = asm_ret();
    assert(instr.kind == ASM_RET);
    assert(instr.num_operands == 0);
    assert(instr.operands == NULL);
    printf("  PASS: test_asm_ret\n");
}

void test_create_asm_function() {
    AsmInstr* instrs = malloc(sizeof(AsmInstr) * 2);
    instrs[0] = asm_mov(asm_reg("w0"), asm_imm(5));
    instrs[1] = asm_ret();

    AsmFunction* fn = create_asm_function("main", instrs, 2);
    assert(fn != NULL);
    assert(strcmp(fn->name, "main") == 0);
    assert(fn->num_instrs == 2);
    assert(fn->instrs[0].kind == ASM_MOV);
    assert(fn->instrs[1].kind == ASM_RET);
    destroy_asm_function(fn);
    printf("  PASS: test_create_asm_function\n");
}

void test_create_asm_program() {
    AsmInstr* instrs = malloc(sizeof(AsmInstr) * 1);
    instrs[0] = asm_ret();
    AsmFunction* fn = create_asm_function("foo", instrs, 1);

    AsmFunction** functions = malloc(sizeof(AsmFunction*));
    functions[0] = fn;

    AsmProgram* prog = create_asm_program(functions, 1);
    assert(prog != NULL);
    assert(prog->num_functions == 1);
    assert(strcmp(prog->functions[0]->name, "foo") == 0);
    destroy_asm_program(prog);
    printf("  PASS: test_create_asm_program\n");
}

// --- codegen integration test ---
// Converts AST for: int main() { return 2; }

void test_codegen_return_2() {
    AstStatement** body = malloc(sizeof(AstStatement*));
    body[0] = create_return_stmt(create_int_expr(2));
    AstDeclaration** decls = malloc(sizeof(AstDeclaration*));
    decls[0] = create_function_decl("main", body, 1);
    AstProgram* program = create_program(decls, 1);

    AsmProgram* asm_prog = codegen(program);

    assert(asm_prog != NULL);
    assert(asm_prog->num_functions == 1);
    assert(strcmp(asm_prog->functions[0]->name, "main") == 0);
    assert(asm_prog->functions[0]->num_instrs == 2);
    assert(asm_prog->functions[0]->instrs[0].kind == ASM_MOV);
    assert(asm_prog->functions[0]->instrs[0].operands[0].kind == OPERAND_REG);
    assert(strcmp(asm_prog->functions[0]->instrs[0].operands[0].reg, "w0") == 0);
    assert(asm_prog->functions[0]->instrs[0].operands[1].kind == OPERAND_IMM);
    assert(asm_prog->functions[0]->instrs[0].operands[1].imm == 2);
    assert(asm_prog->functions[0]->instrs[1].kind == ASM_RET);

    destroy_asm_program(asm_prog);
    destroy_program(program);
    printf("  PASS: test_codegen_return_2\n");
}

void test_codegen_return_0() {
    AstStatement** body = malloc(sizeof(AstStatement*));
    body[0] = create_return_stmt(create_int_expr(0));
    AstDeclaration** decls = malloc(sizeof(AstDeclaration*));
    decls[0] = create_function_decl("main", body, 1);
    AstProgram* program = create_program(decls, 1);

    AsmProgram* asm_prog = codegen(program);

    assert(asm_prog->functions[0]->instrs[0].operands[1].imm == 0);

    destroy_asm_program(asm_prog);
    destroy_program(program);
    printf("  PASS: test_codegen_return_0\n");
}

// --- emit integration test ---

void test_emit_return_2() {
    AstStatement** body = malloc(sizeof(AstStatement*));
    body[0] = create_return_stmt(create_int_expr(2));
    AstDeclaration** decls = malloc(sizeof(AstDeclaration*));
    decls[0] = create_function_decl("main", body, 1);
    AstProgram* program = create_program(decls, 1);

    AsmProgram* asm_prog = codegen(program);

    char* buf = NULL;
    size_t buf_size = 0;
    FILE* out = open_memstream(&buf, &buf_size);
    emit_arm64(asm_prog, out);
    fclose(out);

    assert(strstr(buf, ".global _main") != NULL);
    assert(strstr(buf, "_main:") != NULL);
    assert(strstr(buf, "mov w0, #2") != NULL);
    assert(strstr(buf, "ret") != NULL);

    free(buf);
    destroy_asm_program(asm_prog);
    destroy_program(program);
    printf("  PASS: test_emit_return_2\n");
}

int main(void) {
    printf("Running codegen tests...\n");
    test_asm_reg();
    test_asm_imm();
    test_asm_mov();
    test_asm_ret();
    test_create_asm_function();
    test_create_asm_program();
    test_codegen_return_2();
    test_codegen_return_0();
    test_emit_return_2();
    printf("All codegen tests passed!\n");
    return 0;
}
