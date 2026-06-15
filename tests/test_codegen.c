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

static AstProgram make_test_program(AstExp* expr) {
    AstStatement* body = malloc(sizeof(AstStatement));
    body[0] = make_return_stmt(expr);
    AstDeclaration* decls = malloc(sizeof(AstDeclaration));
    decls[0] = make_function_decl("main", body, 1);
    return make_program(decls, 1);
}

// --- codegen integration test ---

void test_codegen_return_2() {
    AstProgram program = make_test_program(create_int_exp(2));

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
    AstProgram program = make_test_program(create_int_exp(0));

    IrProgram ir = emit_ir(&program);
    x86_Program asm_prog = codegen(&ir);

    assert(asm_prog.functions[0].instrs.head->mov.src.imm == 0);

    destroy_x86_program(&asm_prog);
    destroy_program(&program);
    printf("  PASS: test_codegen_return_0\n");
}

// --- emit integration test ---

void test_emit_return_2() {
    AstProgram program = make_test_program(create_int_exp(2));

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
    AstExp* lit = create_int_exp(2);
    AstExp* neg = create_unary_exp(UNOP_MINUS, lit);
    AstExp* comp = create_unary_exp(UNOP_COMP, neg);
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
    assert(i->unop.unop == x86_COMP);
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
    AstExp* lit = create_int_exp(2);
    AstExp* neg = create_unary_exp(UNOP_MINUS, lit);
    AstExp* comp = create_unary_exp(UNOP_COMP, neg);
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

// --- binop codegen tests ---

// The arithmetic binops (add/sub/mul) all lower the same way:
//   mov  dst, lhs
//   <op> rhs, dst
// Only the x86 opcode differs, so we drive them from a table.
void test_codegen_binop_arithmetic() {
    struct { AstBinopType ast_op; x86_Binop x86_op; } cases[] = {
        { BINOP_ADD, x86_ADD },
        { BINOP_SUB, x86_SUB },
        { BINOP_MUL, x86_MUL },
    };
    for (size_t c = 0; c < sizeof(cases) / sizeof(cases[0]); c++) {
        AstProgram program = make_test_program(
            create_binop_exp(cases[c].ast_op,
                             create_int_exp(4), create_int_exp(5)));
        IrProgram ir = emit_ir(&program);
        x86_Program asm_prog = codegen(&ir);

        x86_Instr* i = asm_prog.functions[0].instrs.head;

        // mov dst, $4   (lhs copied into the destination temp)
        assert(i != NULL && i->kind == x86_MOV);
        assert(i->mov.dst.kind == x86_ID);
        assert(i->mov.src.kind == x86_IMM && i->mov.src.imm == 4);
        i = i->next;

        // <op> $5, dst
        assert(i != NULL && i->kind == x86_BINOP);
        assert(i->binop.optype == cases[c].x86_op);
        assert(i->binop.rhs.kind == x86_IMM && i->binop.rhs.imm == 5);
        assert(i->binop.dst.kind == x86_ID);
        i = i->next;

        // mov %eax, dst  (return lowering)
        assert(i != NULL && i->kind == x86_MOV);
        assert(i->mov.dst.kind == x86_REG && i->mov.dst.reg == x86_AX);
        i = i->next;

        assert(i != NULL && i->kind == x86_RET);
        assert(i->next == NULL);

        destroy_x86_program(&asm_prog);
        destroy_program(&program);
    }
    printf("  PASS: test_codegen_binop_arithmetic\n");
}

// The bitwise binops (and/or/xor/shl/shr) lower through the same generic
//   mov  dst, lhs
//   <op> rhs, dst
// pattern as the arithmetic ones; only the x86 opcode differs.
void test_codegen_binop_bitwise() {
    struct { AstBinopType ast_op; x86_Binop x86_op; } cases[] = {
        { BINOP_AND, x86_AND },
        { BINOP_OR, x86_OR },
        { BINOP_XOR, x86_XOR },
        { BINOP_LSHIFT, x86_LSHIFT },
        { BINOP_RSHIFT, x86_RSHIFT },
    };
    for (size_t c = 0; c < sizeof(cases) / sizeof(cases[0]); c++) {
        AstProgram program = make_test_program(
            create_binop_exp(cases[c].ast_op,
                             create_int_exp(12), create_int_exp(2)));
        IrProgram ir = emit_ir(&program);
        x86_Program asm_prog = codegen(&ir);

        x86_Instr* i = asm_prog.functions[0].instrs.head;

        // mov dst, $12   (lhs copied into the destination temp)
        assert(i != NULL && i->kind == x86_MOV);
        assert(i->mov.dst.kind == x86_ID);
        assert(i->mov.src.kind == x86_IMM && i->mov.src.imm == 12);
        i = i->next;

        // <op> $2, dst
        assert(i != NULL && i->kind == x86_BINOP);
        assert(i->binop.optype == cases[c].x86_op);
        assert(i->binop.rhs.kind == x86_IMM && i->binop.rhs.imm == 2);
        assert(i->binop.dst.kind == x86_ID);
        i = i->next;

        // mov %eax, dst  (return lowering)
        assert(i != NULL && i->kind == x86_MOV);
        assert(i->mov.dst.kind == x86_REG && i->mov.dst.reg == x86_AX);
        i = i->next;

        assert(i != NULL && i->kind == x86_RET);
        assert(i->next == NULL);

        destroy_x86_program(&asm_prog);
        destroy_program(&program);
    }
    printf("  PASS: test_codegen_binop_bitwise\n");
}

// int main() { return 6 / 3; }  lowers division through eax/cdq/idiv:
//   mov  %eax, dividend   (lhs)
//   cdq
//   idivl divisor         (rhs)
//   mov  dst, %eax        (quotient is in eax)
void test_codegen_binop_div() {
    AstProgram program = make_test_program(
        create_binop_exp(BINOP_DIV, create_int_exp(6), create_int_exp(3)));
    IrProgram ir = emit_ir(&program);
    x86_Program asm_prog = codegen(&ir);

    x86_Instr* i = asm_prog.functions[0].instrs.head;

    // mov %eax, $6  (lhs / dividend loaded into eax)
    assert(i != NULL && i->kind == x86_MOV);
    assert(i->mov.dst.kind == x86_REG && i->mov.dst.reg == x86_AX);
    assert(i->mov.src.kind == x86_IMM && i->mov.src.imm == 6);
    i = i->next;

    // cdq
    assert(i != NULL && i->kind == x86_CDQ);
    i = i->next;

    // idivl $3  (rhs / divisor is the idiv operand)
    assert(i != NULL && i->kind == x86_IDIV);
    assert(i->idiv.operand.kind == x86_IMM && i->idiv.operand.imm == 3);
    i = i->next;

    // mov dst, %eax  (quotient read from eax)
    assert(i != NULL && i->kind == x86_MOV);
    assert(i->mov.dst.kind == x86_ID);
    assert(i->mov.src.kind == x86_REG && i->mov.src.reg == x86_AX);
    i = i->next;

    // return lowering + ret
    assert(i != NULL && i->kind == x86_MOV);
    i = i->next;
    assert(i != NULL && i->kind == x86_RET);

    destroy_x86_program(&asm_prog);
    destroy_program(&program);
    printf("  PASS: test_codegen_binop_div\n");
}

// int main() { return 7 % 2; }  is identical to division except the result is
// taken from edx (the remainder register) instead of eax.
void test_codegen_binop_mod() {
    AstProgram program = make_test_program(
        create_binop_exp(BINOP_MOD, create_int_exp(7), create_int_exp(2)));
    IrProgram ir = emit_ir(&program);
    x86_Program asm_prog = codegen(&ir);

    x86_Instr* i = asm_prog.functions[0].instrs.head;

    // mov %eax, $7  (lhs / dividend)
    assert(i != NULL && i->kind == x86_MOV);
    assert(i->mov.dst.kind == x86_REG && i->mov.dst.reg == x86_AX);
    assert(i->mov.src.kind == x86_IMM && i->mov.src.imm == 7);
    i = i->next;

    // cdq
    assert(i != NULL && i->kind == x86_CDQ);
    i = i->next;

    // idivl $2  (rhs / divisor)
    assert(i != NULL && i->kind == x86_IDIV);
    assert(i->idiv.operand.kind == x86_IMM && i->idiv.operand.imm == 2);
    i = i->next;

    // mov dst, %edx  (remainder read from edx)
    assert(i != NULL && i->kind == x86_MOV);
    assert(i->mov.dst.kind == x86_ID);
    assert(i->mov.src.kind == x86_REG && i->mov.src.reg == x86_DX);

    destroy_x86_program(&asm_prog);
    destroy_program(&program);
    printf("  PASS: test_codegen_binop_mod\n");
}

// --- emit tests for the new instruction kinds ---

// Build an x86 function by hand exercising every new emit path (binop opcodes,
// idivl, cdq) and every register name (eax, edx, r10d, r11d), then check the
// rendered assembly text.
void test_emit_binop_and_div_instructions() {
    x86_InstrList instrs = x86_instr_list_new();
    // addl $1, %eax
    x86_instr_list_append(&instrs, (x86_Instr){.kind = x86_BINOP, .binop = {
        .optype = x86_ADD,
        .rhs = (x86_Operand){.kind = x86_IMM, .imm = 1},
        .dst = (x86_Operand){.kind = x86_REG, .reg = x86_AX}}});
    // subl %r10d, %r11d
    x86_instr_list_append(&instrs, (x86_Instr){.kind = x86_BINOP, .binop = {
        .optype = x86_SUB,
        .rhs = (x86_Operand){.kind = x86_REG, .reg = x86_R10},
        .dst = (x86_Operand){.kind = x86_REG, .reg = x86_R11}}});
    // imull %edx, %eax
    x86_instr_list_append(&instrs, (x86_Instr){.kind = x86_BINOP, .binop = {
        .optype = x86_MUL,
        .rhs = (x86_Operand){.kind = x86_REG, .reg = x86_DX},
        .dst = (x86_Operand){.kind = x86_REG, .reg = x86_AX}}});
    // cdq
    x86_instr_list_append(&instrs, (x86_Instr){.kind = x86_CDQ});
    // idivl %r10d
    x86_instr_list_append(&instrs, (x86_Instr){.kind = x86_IDIV, .idiv = {
        .operand = (x86_Operand){.kind = x86_REG, .reg = x86_R10}}});
    x86_instr_list_append(&instrs, (x86_Instr){.kind = x86_RET});

    x86_Function* functions = malloc(sizeof(x86_Function));
    functions[0] = make_x86_function("main", instrs);
    x86_Program prog = make_x86_program(functions, 1);

    char* buf = NULL;
    size_t buf_size = 0;
    FILE* out = open_memstream(&buf, &buf_size);
    emit_asm(&prog, out);
    fclose(out);

    assert(strstr(buf, "addl $1, %eax") != NULL);
    assert(strstr(buf, "subl %r10d, %r11d") != NULL);
    assert(strstr(buf, "imull %edx, %eax") != NULL);
    assert(strstr(buf, "cdq") != NULL);
    assert(strstr(buf, "idivl %r10d") != NULL);
    // No opcode/register should fall through to the "???" placeholders.
    assert(strstr(buf, "???") == NULL);

    free(buf);
    destroy_x86_program(&prog);
    printf("  PASS: test_emit_binop_and_div_instructions\n");
}

// Build an x86 function by hand exercising every bitwise opcode in AT&T
// syntax, then check the rendered assembly text exactly.
void test_emit_bitwise_instructions() {
    struct { x86_Binop op; const char* rendered; } cases[] = {
        { x86_AND,    "andl $5, %eax" },
        { x86_OR,     "orl $5, %eax" },
        { x86_XOR,    "xorl $5, %eax" },
        { x86_LSHIFT, "shll $5, %eax" },
        { x86_RSHIFT, "shrl $5, %eax" },
    };

    x86_InstrList instrs = x86_instr_list_new();
    for (size_t c = 0; c < sizeof(cases) / sizeof(cases[0]); c++) {
        x86_instr_list_append(&instrs, (x86_Instr){.kind = x86_BINOP, .binop = {
            .optype = cases[c].op,
            .rhs = (x86_Operand){.kind = x86_IMM, .imm = 5},
            .dst = (x86_Operand){.kind = x86_REG, .reg = x86_AX}}});
    }
    // xorl %r10d, %r11d  (register form)
    x86_instr_list_append(&instrs, (x86_Instr){.kind = x86_BINOP, .binop = {
        .optype = x86_XOR,
        .rhs = (x86_Operand){.kind = x86_REG, .reg = x86_R10},
        .dst = (x86_Operand){.kind = x86_REG, .reg = x86_R11}}});
    x86_instr_list_append(&instrs, (x86_Instr){.kind = x86_RET});

    x86_Function* functions = malloc(sizeof(x86_Function));
    functions[0] = make_x86_function("main", instrs);
    x86_Program prog = make_x86_program(functions, 1);

    char* buf = NULL;
    size_t buf_size = 0;
    FILE* out = open_memstream(&buf, &buf_size);
    emit_asm(&prog, out);
    fclose(out);

    for (size_t c = 0; c < sizeof(cases) / sizeof(cases[0]); c++) {
        assert(strstr(buf, cases[c].rendered) != NULL);
    }
    assert(strstr(buf, "xorl %r10d, %r11d") != NULL);
    assert(strstr(buf, "???") == NULL);

    free(buf);
    destroy_x86_program(&prog);
    printf("  PASS: test_emit_bitwise_instructions\n");
}

// After register renaming, no binop/idiv operand should still be a pseudo.
// (rename_registers must visit x86_BINOP and x86_IDIV operands.)
void test_rename_binop_clears_pseudos() {
    AstProgram program = make_test_program(
        create_binop_exp(BINOP_ADD, create_int_exp(1), create_int_exp(2)));
    IrProgram ir = emit_ir(&program);
    x86_Program asm_prog = codegen(&ir);

    rename_registers(&asm_prog.functions[0]);

    for (x86_Instr* i = asm_prog.functions[0].instrs.head; i; i = i->next) {
        if (i->kind == x86_BINOP) {
            assert(i->binop.rhs.kind != x86_ID);
            assert(i->binop.dst.kind != x86_ID);
        }
        if (i->kind == x86_IDIV) {
            assert(i->idiv.operand.kind != x86_ID);
        }
        if (i->kind == x86_MOV) {
            assert(i->mov.src.kind != x86_ID);
            assert(i->mov.dst.kind != x86_ID);
        }
    }

    destroy_x86_program(&asm_prog);
    destroy_program(&program);
    printf("  PASS: test_rename_binop_clears_pseudos\n");
}

// End-to-end: a division program renders cdq + idivl through the full pipeline
// and leaves no pseudo-registers behind.
void test_emit_div_program() {
    AstProgram program = make_test_program(
        create_binop_exp(BINOP_DIV, create_int_exp(8), create_int_exp(4)));
    IrProgram ir = emit_ir(&program);
    x86_Program asm_prog = codegen(&ir);

    int stack_offset = rename_registers(&asm_prog.functions[0]);
    allocate_stack(&asm_prog.functions[0], stack_offset);

    char* buf = NULL;
    size_t buf_size = 0;
    FILE* out = open_memstream(&buf, &buf_size);
    emit_asm(&asm_prog, out);
    fclose(out);

    assert(strstr(buf, "cdq") != NULL);
    assert(strstr(buf, "idivl") != NULL);
    assert(strstr(buf, "<pseudo:") == NULL);

    free(buf);
    destroy_x86_program(&asm_prog);
    destroy_program(&program);
    printf("  PASS: test_emit_div_program\n");
}

// End-to-end: int main() { return ((5 & 3) | (1 << 2)) ^ (16 >> 1); }
// exercises every bitwise opcode through the full pipeline (codegen, register
// rename, stack allocation, emission) and leaves no pseudo-registers behind.
void test_emit_bitwise_program() {
    AstExp* and_exp = create_binop_exp(BINOP_AND,
                                       create_int_exp(5), create_int_exp(3));
    AstExp* shl_exp = create_binop_exp(BINOP_LSHIFT,
                                       create_int_exp(1), create_int_exp(2));
    AstExp* or_exp = create_binop_exp(BINOP_OR, and_exp, shl_exp);
    AstExp* shr_exp = create_binop_exp(BINOP_RSHIFT,
                                       create_int_exp(16), create_int_exp(1));
    AstExp* xor_exp = create_binop_exp(BINOP_XOR, or_exp, shr_exp);
    AstProgram program = make_test_program(xor_exp);

    IrProgram ir = emit_ir(&program);
    x86_Program asm_prog = codegen(&ir);

    int stack_offset = rename_registers(&asm_prog.functions[0]);
    allocate_stack(&asm_prog.functions[0], stack_offset);

    char* buf = NULL;
    size_t buf_size = 0;
    FILE* out = open_memstream(&buf, &buf_size);
    emit_asm(&asm_prog, out);
    fclose(out);

    assert(strstr(buf, "andl") != NULL);
    assert(strstr(buf, "orl") != NULL);
    assert(strstr(buf, "xorl") != NULL);
    assert(strstr(buf, "shll") != NULL);
    assert(strstr(buf, "shrl") != NULL);
    assert(strstr(buf, "<pseudo:") == NULL);
    assert(strstr(buf, "???") == NULL);

    free(buf);
    destroy_x86_program(&asm_prog);
    destroy_program(&program);
    printf("  PASS: test_emit_bitwise_program\n");
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
    test_codegen_binop_arithmetic();
    test_codegen_binop_bitwise();
    test_codegen_binop_div();
    test_codegen_binop_mod();
    test_emit_binop_and_div_instructions();
    test_emit_bitwise_instructions();
    test_rename_binop_clears_pseudos();
    test_emit_bitwise_program();
    test_emit_div_program();
    printf("All codegen tests passed!\n");
    return 0;
}
