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
    x86_Instr* instrs = malloc(sizeof(x86_Instr) * 2);
    instrs[0] = (x86_Instr){.kind = x86_MOV, .mov = {
        .dst = (x86_Operand){.kind = x86_REG, .reg = x86_AX},
        .src = (x86_Operand){.kind = x86_IMM, .imm = 5}
    }};
    instrs[1] = (x86_Instr){.kind = x86_RET};

    x86_Function* fn = create_x86_function("main", instrs, 2);
    assert(fn != NULL);
    assert(strcmp(fn->name, "main") == 0);
    assert(fn->num_instrs == 2);
    assert(fn->instrs[0].kind == x86_MOV);
    assert(fn->instrs[1].kind == x86_RET);
    destroy_x86_function(fn);
    printf("  PASS: test_create_x86_function\n");
}

void test_create_x86_program() {
    x86_Instr* instrs = malloc(sizeof(x86_Instr) * 1);
    instrs[0] = (x86_Instr){.kind = x86_RET};
    x86_Function* fn = create_x86_function("foo", instrs, 1);

    x86_Function** functions = malloc(sizeof(x86_Function*));
    functions[0] = fn;

    x86_Program* prog = create_x86_program(functions, 1);
    assert(prog != NULL);
    assert(prog->num_functions == 1);
    assert(strcmp(prog->functions[0]->name, "foo") == 0);
    destroy_x86_program(prog);
    printf("  PASS: test_create_x86_program\n");
}

// --- codegen integration test ---

void test_codegen_return_2() {
    AstStatement** body = malloc(sizeof(AstStatement*));
    body[0] = create_return_stmt(create_int_expr(2));
    AstDeclaration** decls = malloc(sizeof(AstDeclaration*));
    decls[0] = create_function_decl("main", body, 1);
    AstProgram* program = create_program(decls, 1);

    IrProgram ir = emit_ir(program);
    x86_Program* asm_prog = codegen(&ir);

    assert(asm_prog != NULL);
    assert(asm_prog->num_functions == 1);
    assert(strcmp(asm_prog->functions[0]->name, "main") == 0);
    assert(asm_prog->functions[0]->num_instrs == 2);
    assert(asm_prog->functions[0]->instrs[0].kind == x86_MOV);
    assert(asm_prog->functions[0]->instrs[0].mov.src.kind == x86_IMM);
    assert(asm_prog->functions[0]->instrs[0].mov.src.imm == 2);
    assert(asm_prog->functions[0]->instrs[1].kind == x86_RET);

    destroy_x86_program(asm_prog);
    destroy_program(program);
    printf("  PASS: test_codegen_return_2\n");
}

void test_codegen_return_0() {
    AstStatement** body = malloc(sizeof(AstStatement*));
    body[0] = create_return_stmt(create_int_expr(0));
    AstDeclaration** decls = malloc(sizeof(AstDeclaration*));
    decls[0] = create_function_decl("main", body, 1);
    AstProgram* program = create_program(decls, 1);

    IrProgram ir = emit_ir(program);
    x86_Program* asm_prog = codegen(&ir);

    assert(asm_prog->functions[0]->instrs[0].mov.src.imm == 0);

    destroy_x86_program(asm_prog);
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

    IrProgram ir = emit_ir(program);
    x86_Program* asm_prog = codegen(&ir);

    char* buf = NULL;
    size_t buf_size = 0;
    FILE* out = open_memstream(&buf, &buf_size);
    emit_asm(asm_prog, out);
    fclose(out);

    assert(strstr(buf, ".global _main") != NULL);
    assert(strstr(buf, "_main:") != NULL);
    assert(strstr(buf, "ret") != NULL);

    free(buf);
    destroy_x86_program(asm_prog);
    destroy_program(program);
    printf("  PASS: test_emit_return_2\n");
}

int main(void) {
    printf("Running codegen tests...\n");
    test_create_x86_function();
    test_create_x86_program();
    test_codegen_return_2();
    test_codegen_return_0();
    test_emit_return_2();
    printf("All codegen tests passed!\n");
    return 0;
}
