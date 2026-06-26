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
    x86_instr_list_append(&instrs, (x86_Instr){.kind = x86_MOV, .as.mov = {
        .dst = (x86_Operand){.kind = x86_REG, .as.reg = x86_AX},
        .src = (x86_Operand){.kind = x86_IMM, .as.imm = 5}
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
    AstFunction fn = ast_function_make("main", 8);
    ast_function_append(&fn, (AstBlockItem){
        .type = AST_STATEMENT,
        .as.stmt = make_return_stmt(expr),
    });
    AstFunction* functions = malloc(sizeof(AstFunction));
    functions[0] = fn;
    return ast_program_create(functions, 1);
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
    assert(first->as.mov.src.kind == x86_IMM);
    assert(first->as.mov.src.as.imm == 2);
    assert(first->next->kind == x86_RET);

    destroy_x86_program(&asm_prog);
    destroy_program(&program);
    printf("  PASS: test_codegen_return_2\n");
}

void test_codegen_return_0() {
    AstProgram program = make_test_program(create_int_exp(0));

    IrProgram ir = emit_ir(&program);
    x86_Program asm_prog = codegen(&ir);

    assert(asm_prog.functions[0].instrs.head->as.mov.src.as.imm == 0);

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
    assert(i->as.mov.src.kind == x86_IMM);
    assert(i->as.mov.src.as.imm == 2);
    assert(i->as.mov.dst.kind == x86_ID);
    i = i->next;

    // negl tmp0
    assert(i != NULL);
    assert(i->kind == x86_UNOP);
    assert(i->as.unop.unop == x86_NEG);
    assert(i->as.unop.operand.kind == x86_ID);
    i = i->next;

    // mov tmp1, tmp0
    assert(i != NULL);
    assert(i->kind == x86_MOV);
    assert(i->as.mov.src.kind == x86_ID);
    assert(i->as.mov.dst.kind == x86_ID);
    i = i->next;

    // notl tmp1
    assert(i != NULL);
    assert(i->kind == x86_UNOP);
    assert(i->as.unop.unop == x86_COMP);
    assert(i->as.unop.operand.kind == x86_ID);
    i = i->next;

    // mov %eax, tmp1
    assert(i != NULL);
    assert(i->kind == x86_MOV);
    assert(i->as.mov.dst.kind == x86_REG);
    assert(i->as.mov.dst.as.reg == x86_AX);
    assert(i->as.mov.src.kind == x86_ID);
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
        assert(i->as.mov.dst.kind == x86_ID);
        assert(i->as.mov.src.kind == x86_IMM && i->as.mov.src.as.imm == 4);
        i = i->next;

        // <op> $5, dst
        assert(i != NULL && i->kind == x86_BINOP);
        assert(i->as.binop.optype == cases[c].x86_op);
        assert(i->as.binop.rhs.kind == x86_IMM && i->as.binop.rhs.as.imm == 5);
        assert(i->as.binop.dst.kind == x86_ID);
        i = i->next;

        // mov %eax, dst  (return lowering)
        assert(i != NULL && i->kind == x86_MOV);
        assert(i->as.mov.dst.kind == x86_REG && i->as.mov.dst.as.reg == x86_AX);
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
        assert(i->as.mov.dst.kind == x86_ID);
        assert(i->as.mov.src.kind == x86_IMM && i->as.mov.src.as.imm == 12);
        i = i->next;

        // <op> $2, dst
        assert(i != NULL && i->kind == x86_BINOP);
        assert(i->as.binop.optype == cases[c].x86_op);
        assert(i->as.binop.rhs.kind == x86_IMM && i->as.binop.rhs.as.imm == 2);
        assert(i->as.binop.dst.kind == x86_ID);
        i = i->next;

        // mov %eax, dst  (return lowering)
        assert(i != NULL && i->kind == x86_MOV);
        assert(i->as.mov.dst.kind == x86_REG && i->as.mov.dst.as.reg == x86_AX);
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
    assert(i->as.mov.dst.kind == x86_REG && i->as.mov.dst.as.reg == x86_AX);
    assert(i->as.mov.src.kind == x86_IMM && i->as.mov.src.as.imm == 6);
    i = i->next;

    // cdq
    assert(i != NULL && i->kind == x86_CDQ);
    i = i->next;

    // idivl $3  (rhs / divisor is the idiv operand)
    assert(i != NULL && i->kind == x86_IDIV);
    assert(i->as.idiv.operand.kind == x86_IMM && i->as.idiv.operand.as.imm == 3);
    i = i->next;

    // mov dst, %eax  (quotient read from eax)
    assert(i != NULL && i->kind == x86_MOV);
    assert(i->as.mov.dst.kind == x86_ID);
    assert(i->as.mov.src.kind == x86_REG && i->as.mov.src.as.reg == x86_AX);
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
    assert(i->as.mov.dst.kind == x86_REG && i->as.mov.dst.as.reg == x86_AX);
    assert(i->as.mov.src.kind == x86_IMM && i->as.mov.src.as.imm == 7);
    i = i->next;

    // cdq
    assert(i != NULL && i->kind == x86_CDQ);
    i = i->next;

    // idivl $2  (rhs / divisor)
    assert(i != NULL && i->kind == x86_IDIV);
    assert(i->as.idiv.operand.kind == x86_IMM && i->as.idiv.operand.as.imm == 2);
    i = i->next;

    // mov dst, %edx  (remainder read from edx)
    assert(i != NULL && i->kind == x86_MOV);
    assert(i->as.mov.dst.kind == x86_ID);
    assert(i->as.mov.src.kind == x86_REG && i->as.mov.src.as.reg == x86_DX);

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
    x86_instr_list_append(&instrs, (x86_Instr){.kind = x86_BINOP, .as.binop = {
        .optype = x86_ADD,
        .rhs = (x86_Operand){.kind = x86_IMM, .as.imm = 1},
        .dst = (x86_Operand){.kind = x86_REG, .as.reg = x86_AX}}});
    // subl %r10d, %r11d
    x86_instr_list_append(&instrs, (x86_Instr){.kind = x86_BINOP, .as.binop = {
        .optype = x86_SUB,
        .rhs = (x86_Operand){.kind = x86_REG, .as.reg = x86_R10},
        .dst = (x86_Operand){.kind = x86_REG, .as.reg = x86_R11}}});
    // imull %edx, %eax
    x86_instr_list_append(&instrs, (x86_Instr){.kind = x86_BINOP, .as.binop = {
        .optype = x86_MUL,
        .rhs = (x86_Operand){.kind = x86_REG, .as.reg = x86_DX},
        .dst = (x86_Operand){.kind = x86_REG, .as.reg = x86_AX}}});
    // cdq
    x86_instr_list_append(&instrs, (x86_Instr){.kind = x86_CDQ});
    // idivl %r10d
    x86_instr_list_append(&instrs, (x86_Instr){.kind = x86_IDIV, .as.idiv = {
        .operand = (x86_Operand){.kind = x86_REG, .as.reg = x86_R10}}});
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
        x86_instr_list_append(&instrs, (x86_Instr){.kind = x86_BINOP, .as.binop = {
            .optype = cases[c].op,
            .rhs = (x86_Operand){.kind = x86_IMM, .as.imm = 5},
            .dst = (x86_Operand){.kind = x86_REG, .as.reg = x86_AX}}});
    }
    // xorl %r10d, %r11d  (register form)
    x86_instr_list_append(&instrs, (x86_Instr){.kind = x86_BINOP, .as.binop = {
        .optype = x86_XOR,
        .rhs = (x86_Operand){.kind = x86_REG, .as.reg = x86_R10},
        .dst = (x86_Operand){.kind = x86_REG, .as.reg = x86_R11}}});
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
            assert(i->as.binop.rhs.kind != x86_ID);
            assert(i->as.binop.dst.kind != x86_ID);
        }
        if (i->kind == x86_IDIV) {
            assert(i->as.idiv.operand.kind != x86_ID);
        }
        if (i->kind == x86_MOV) {
            assert(i->as.mov.src.kind != x86_ID);
            assert(i->as.mov.dst.kind != x86_ID);
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

// --- constructors for the relational / control-flow instruction kinds ---

void test_x86_new_instr_constructors() {
    x86_Instr cmp = x86_cmp_instr(x86_operand_reg(x86_AX), x86_operand_imm(7));
    assert(cmp.kind == x86_CMP);
    assert(cmp.as.cmp.lhs.kind == x86_REG && cmp.as.cmp.lhs.as.reg == x86_AX);
    assert(cmp.as.cmp.rhs.kind == x86_IMM && cmp.as.cmp.rhs.as.imm == 7);

    x86_Instr jmp = x86_jmp_instr("end");
    assert(jmp.kind == x86_JMP);
    assert(strcmp(jmp.as.jmp.identifier, "end") == 0);

    x86_Instr jmpcc = x86_jmpcc_instr(x86_NE, "loop");
    assert(jmpcc.kind == x86_JMPCC);
    assert(jmpcc.as.jmpcc.cond == x86_NE);
    assert(strcmp(jmpcc.as.jmpcc.identifier, "loop") == 0);

    x86_Instr setcc = x86_setcc_instr(x86_GE, x86_operand_reg(x86_AX));
    assert(setcc.kind == x86_SETCC);
    assert(setcc.as.setcc.cond == x86_GE);
    assert(setcc.as.setcc.op.kind == x86_REG && setcc.as.setcc.op.as.reg == x86_AX);

    x86_Instr label = x86_label_instr("L0");
    assert(label.kind == x86_LABEL);
    assert(strcmp(label.as.label.identifier, "L0") == 0);

    printf("  PASS: test_x86_new_instr_constructors\n");
}

// --- relational operator codegen ---

// Each relational binop lowers to:
//   cmp  lhs, rhs
//   mov  dst, $0      (zero the result; movl does not touch flags)
//   setCC dst         (set the low byte from the compare flags)
// Only the condition code differs, so drive them from a table.
void test_codegen_relational_ops() {
    struct { AstBinopType ast_op; x86_ConditionCode cond; } cases[] = {
        { BINOP_EQ,      x86_E  },
        { BINOP_NEQ,     x86_NE },
        { BINOP_LESS,    x86_L  },
        { BINOP_GREATER, x86_G  },
        { BINOP_LEQ,     x86_LE },
        { BINOP_GEQ,     x86_GE },
    };
    for (size_t c = 0; c < sizeof(cases) / sizeof(cases[0]); c++) {
        AstProgram program = make_test_program(
            create_binop_exp(cases[c].ast_op,
                             create_int_exp(4), create_int_exp(5)));
        IrProgram ir = emit_ir(&program);
        x86_Program asm_prog = codegen(&ir);

        x86_Instr* i = asm_prog.functions[0].instrs.head;

        // cmp $4, $5
        assert(i != NULL && i->kind == x86_CMP);
        assert(i->as.cmp.lhs.kind == x86_IMM && i->as.cmp.lhs.as.imm == 4);
        assert(i->as.cmp.rhs.kind == x86_IMM && i->as.cmp.rhs.as.imm == 5);
        i = i->next;

        // mov dst, $0
        assert(i != NULL && i->kind == x86_MOV);
        assert(i->as.mov.dst.kind == x86_ID);
        assert(i->as.mov.src.kind == x86_IMM && i->as.mov.src.as.imm == 0);
        i = i->next;

        // setCC dst
        assert(i != NULL && i->kind == x86_SETCC);
        assert(i->as.setcc.cond == cases[c].cond);
        assert(i->as.setcc.op.kind == x86_ID);
        i = i->next;

        // return lowering: mov %eax, dst ; ret
        assert(i != NULL && i->kind == x86_MOV);
        assert(i->as.mov.dst.kind == x86_REG && i->as.mov.dst.as.reg == x86_AX);
        i = i->next;
        assert(i != NULL && i->kind == x86_RET);
        assert(i->next == NULL);

        destroy_x86_program(&asm_prog);
        destroy_program(&program);
    }
    printf("  PASS: test_codegen_relational_ops\n");
}

// int main() { return !5; }  lowers logical NOT to:
//   cmp  $0, src       (compare the operand against zero)
//   mov  dst, $0       (clear the result without disturbing flags)
//   sete dst           (result is 1 exactly when src was 0)
void test_codegen_logical_not() {
    AstProgram program = make_test_program(
        create_unary_exp(UNOP_NOT, create_int_exp(5)));
    IrProgram ir = emit_ir(&program);
    x86_Program asm_prog = codegen(&ir);

    x86_Instr* i = asm_prog.functions[0].instrs.head;

    // cmp src($5), $0  -- the operand being tested, not the destination
    assert(i != NULL && i->kind == x86_CMP);
    assert(i->as.cmp.lhs.kind == x86_IMM && i->as.cmp.lhs.as.imm == 5);
    assert(i->as.cmp.rhs.kind == x86_IMM && i->as.cmp.rhs.as.imm == 0);
    i = i->next;

    // mov dst, $0  -- the result temp is zeroed
    assert(i != NULL && i->kind == x86_MOV);
    assert(i->as.mov.dst.kind == x86_ID);
    assert(i->as.mov.src.kind == x86_IMM && i->as.mov.src.as.imm == 0);
    const char* dst_name = i->as.mov.dst.as.identifier;
    i = i->next;

    // sete dst  -- sets the same temp that was zeroed
    assert(i != NULL && i->kind == x86_SETCC);
    assert(i->as.setcc.cond == x86_E);
    assert(i->as.setcc.op.kind == x86_ID);
    assert(strcmp(i->as.setcc.op.as.identifier, dst_name) == 0);

    destroy_x86_program(&asm_prog);
    destroy_program(&program);
    printf("  PASS: test_codegen_logical_not\n");
}

// int main() { return 1 && 2; }  exercises the short-circuit lowering all the
// way to x86: each operand is compared against zero and conditionally jumps to
// the short-circuit label; the two result stores are plain movs; control flow
// uses an unconditional jump and two labels.
void test_codegen_short_circuit_and() {
    AstProgram program = make_test_program(
        create_binop_exp(BINOP_LAND, create_int_exp(1), create_int_exp(2)));
    IrProgram ir = emit_ir(&program);
    x86_Program asm_prog = codegen(&ir);

    int cmp = 0, jmpcc = 0, jmp = 0, label = 0, mov = 0;
    for (x86_Instr* i = asm_prog.functions[0].instrs.head; i; i = i->next) {
        switch (i->kind) {
            case x86_CMP:   cmp++; break;
            case x86_JMPCC:
                jmpcc++;
                // && short-circuits when an operand is zero -> jump-if-equal
                assert(i->as.jmpcc.cond == x86_E);
                break;
            case x86_JMP:   jmp++; break;
            case x86_LABEL: label++; break;
            case x86_MOV:   mov++; break;
            default: break;
        }
    }
    // one compare + conditional jump per operand
    assert(cmp == 2);
    assert(jmpcc == 2);
    // one unconditional jump skips the short-circuit store
    assert(jmp == 1);
    // short-circuit label + end label
    assert(label == 2);

    destroy_x86_program(&asm_prog);
    destroy_program(&program);
    printf("  PASS: test_codegen_short_circuit_and\n");
}

// rename_registers must rewrite the operands of CMP and SETCC too, or a
// relational result would still reference a pseudo after allocation.
void test_rename_clears_cmp_setcc_pseudos() {
    // (1 + 1) < (2 + 2): operands of the compare are temps, so they become
    // stack slots and force rename to touch the cmp operands.
    AstExp* lhs = create_binop_exp(BINOP_ADD, create_int_exp(1), create_int_exp(1));
    AstExp* rhs = create_binop_exp(BINOP_ADD, create_int_exp(2), create_int_exp(2));
    AstProgram program = make_test_program(create_binop_exp(BINOP_LESS, lhs, rhs));
    IrProgram ir = emit_ir(&program);
    x86_Program asm_prog = codegen(&ir);

    rename_registers(&asm_prog.functions[0]);

    for (x86_Instr* i = asm_prog.functions[0].instrs.head; i; i = i->next) {
        if (i->kind == x86_CMP) {
            assert(i->as.cmp.lhs.kind != x86_ID);
            assert(i->as.cmp.rhs.kind != x86_ID);
        }
        if (i->kind == x86_SETCC) {
            assert(i->as.setcc.op.kind != x86_ID);
        }
    }

    destroy_x86_program(&asm_prog);
    destroy_program(&program);
    printf("  PASS: test_rename_clears_cmp_setcc_pseudos\n");
}

// --- allocate_stack second-pass register fixing ---

static x86_Program one_instr_program(x86_Instr instr) {
    x86_InstrList instrs = x86_instr_list_new();
    x86_instr_list_append(&instrs, instr);
    x86_instr_list_append(&instrs, x86_ret());
    x86_Function* fns = malloc(sizeof(x86_Function));
    fns[0] = make_x86_function("main", instrs);
    return make_x86_program(fns, 1);
}

// cmp mem, mem is illegal: the lhs must be loaded into %r10d first.
void test_allocate_stack_cmp_two_memory() {
    x86_Program prog = one_instr_program(
        x86_cmp_instr(x86_operand_stack(-4), x86_operand_stack(-8)));
    allocate_stack(&prog.functions[0], 0);

    x86_Instr* i = prog.functions[0].instrs.head;
    assert(i->kind == x86_ALLOC); i = i->next;   // prepended frame setup

    // movl -4(%rbp), %r10d
    assert(i->kind == x86_MOV);
    assert(i->as.mov.dst.kind == x86_REG && i->as.mov.dst.as.reg == x86_R10);
    assert(i->as.mov.src.kind == x86_STACK && i->as.mov.src.as.stack == -4);
    i = i->next;

    // cmpl %r10d, -8(%rbp)
    assert(i->kind == x86_CMP);
    assert(i->as.cmp.lhs.kind == x86_REG && i->as.cmp.lhs.as.reg == x86_R10);
    assert(i->as.cmp.rhs.kind == x86_STACK && i->as.cmp.rhs.as.stack == -8);

    destroy_x86_program(&prog);
    printf("  PASS: test_allocate_stack_cmp_two_memory\n");
}

// cmp's rhs may not be an immediate (it is the operand setCC reads against), so
// an immediate rhs is loaded into %r11d first.
void test_allocate_stack_cmp_imm_rhs() {
    x86_Program prog = one_instr_program(
        x86_cmp_instr(x86_operand_stack(-4), x86_operand_imm(0)));
    allocate_stack(&prog.functions[0], 0);

    x86_Instr* i = prog.functions[0].instrs.head;
    assert(i->kind == x86_ALLOC); i = i->next;

    // movl $0, %r11d
    assert(i->kind == x86_MOV);
    assert(i->as.mov.dst.kind == x86_REG && i->as.mov.dst.as.reg == x86_R11);
    assert(i->as.mov.src.kind == x86_IMM && i->as.mov.src.as.imm == 0);
    i = i->next;

    // cmpl -4(%rbp), %r11d
    assert(i->kind == x86_CMP);
    assert(i->as.cmp.lhs.kind == x86_STACK && i->as.cmp.lhs.as.stack == -4);
    assert(i->as.cmp.rhs.kind == x86_REG && i->as.cmp.rhs.as.reg == x86_R11);

    destroy_x86_program(&prog);
    printf("  PASS: test_allocate_stack_cmp_imm_rhs\n");
}

// add mem, mem is illegal: the rhs is loaded into %r10d, then the op is applied
// with %r10d as the source so the result is written back into the dst memory.
void test_allocate_stack_binop_two_memory() {
    x86_Program prog = one_instr_program(
        x86_binary(x86_ADD, x86_operand_stack(-8), x86_operand_stack(-4)));
    allocate_stack(&prog.functions[0], 0);

    x86_Instr* i = prog.functions[0].instrs.head;
    assert(i->kind == x86_ALLOC); i = i->next;

    // movl -8(%rbp), %r10d   (rhs into the scratch register)
    assert(i->kind == x86_MOV);
    assert(i->as.mov.dst.kind == x86_REG && i->as.mov.dst.as.reg == x86_R10);
    assert(i->as.mov.src.kind == x86_STACK && i->as.mov.src.as.stack == -8);
    i = i->next;

    // addl %r10d, -4(%rbp)   (result stays in the dst slot)
    assert(i->kind == x86_BINOP);
    assert(i->as.binop.optype == x86_ADD);
    assert(i->as.binop.rhs.kind == x86_REG && i->as.binop.rhs.as.reg == x86_R10);
    assert(i->as.binop.dst.kind == x86_STACK && i->as.binop.dst.as.stack == -4);

    destroy_x86_program(&prog);
    printf("  PASS: test_allocate_stack_binop_two_memory\n");
}

// --- emit tests for the relational / control-flow instruction kinds ---

// Each new instruction kind must render its own line and not fall through into
// the following case (a missing break would splice unrelated operands together).
void test_emit_cmp_setcc_jmp_label() {
    x86_InstrList instrs = x86_instr_list_new();
    x86_instr_list_append(&instrs,
        x86_cmp_instr(x86_operand_imm(0), x86_operand_reg(x86_AX)));
    x86_instr_list_append(&instrs,
        x86_setcc_instr(x86_E, x86_operand_reg(x86_AX)));
    x86_instr_list_append(&instrs, x86_jmpcc_instr(x86_NE, "skip"));
    x86_instr_list_append(&instrs, x86_jmp_instr("done"));
    x86_instr_list_append(&instrs, x86_label_instr("skip"));
    x86_instr_list_append(&instrs, x86_setcc_instr(x86_GE, x86_operand_stack(-4)));
    x86_instr_list_append(&instrs, x86_label_instr("done"));

    x86_Function* fns = malloc(sizeof(x86_Function));
    fns[0] = make_x86_function("main", instrs);
    x86_Program prog = make_x86_program(fns, 1);

    char* buf = NULL;
    size_t buf_size = 0;
    FILE* out = open_memstream(&buf, &buf_size);
    emit_asm(&prog, out);
    fclose(out);

    assert(strstr(buf, "cmpl $0, %eax") != NULL);
    assert(strstr(buf, "sete %al") != NULL);
    assert(strstr(buf, "jne .Lskip") != NULL);
    assert(strstr(buf, "jmp .Ldone") != NULL);
    assert(strstr(buf, ".Lskip:") != NULL);
    // setCC on a stack slot must emit the memory operand, not a register name.
    assert(strstr(buf, "setge -4(%rbp)") != NULL);
    assert(strstr(buf, ".Ldone:") != NULL);
    assert(strstr(buf, "???") == NULL);

    free(buf);
    destroy_x86_program(&prog);
    printf("  PASS: test_emit_cmp_setcc_jmp_label\n");
}

// End-to-end: int main() { return 1 < 2; } renders cmp + setl through the full
// pipeline and leaves no pseudo-registers behind.
void test_emit_relational_program() {
    AstProgram program = make_test_program(
        create_binop_exp(BINOP_LESS, create_int_exp(1), create_int_exp(2)));
    IrProgram ir = emit_ir(&program);
    x86_Program asm_prog = codegen(&ir);

    int stack_offset = rename_registers(&asm_prog.functions[0]);
    allocate_stack(&asm_prog.functions[0], stack_offset);

    char* buf = NULL;
    size_t buf_size = 0;
    FILE* out = open_memstream(&buf, &buf_size);
    emit_asm(&asm_prog, out);
    fclose(out);

    assert(strstr(buf, "cmpl") != NULL);
    assert(strstr(buf, "setl") != NULL);
    assert(strstr(buf, "<pseudo:") == NULL);
    assert(strstr(buf, "???") == NULL);

    free(buf);
    destroy_x86_program(&asm_prog);
    destroy_program(&program);
    printf("  PASS: test_emit_relational_program\n");
}

// End-to-end: int main() { return 1 && 2; } renders the short-circuit control
// flow (conditional jumps, an unconditional jump, and labels) and leaves no
// pseudo-registers behind.
void test_emit_short_circuit_program() {
    AstProgram program = make_test_program(
        create_binop_exp(BINOP_LAND, create_int_exp(1), create_int_exp(2)));
    IrProgram ir = emit_ir(&program);
    x86_Program asm_prog = codegen(&ir);

    int stack_offset = rename_registers(&asm_prog.functions[0]);
    allocate_stack(&asm_prog.functions[0], stack_offset);

    char* buf = NULL;
    size_t buf_size = 0;
    FILE* out = open_memstream(&buf, &buf_size);
    emit_asm(&asm_prog, out);
    fclose(out);

    assert(strstr(buf, "cmpl") != NULL);
    assert(strstr(buf, "je .L") != NULL);    // && jumps if an operand is zero
    assert(strstr(buf, "jmp .L") != NULL);   // skip past the short-circuit store
    assert(strstr(buf, ":\n") != NULL);      // at least one emitted label
    assert(strstr(buf, "<pseudo:") == NULL);
    assert(strstr(buf, "???") == NULL);

    free(buf);
    destroy_x86_program(&asm_prog);
    destroy_program(&program);
    printf("  PASS: test_emit_short_circuit_program\n");
}

// --- increment / decrement codegen ---
//
// Once inc/dec lowers to `x = x +/- 1` (plus a save temp for postfix), no IR
// instruction kinds are new, so codegen and emission already handle it. These
// drive all four forms from a table.
static const struct {
    const char* desc;
    AstUnopType op;
    x86_Binop x86_op;
    const char* mnemonic;
} incdec_cases[] = {
    { "++x", UNOP_PREINC,  x86_ADD, "addl $1" },
    { "--x", UNOP_PREDEC,  x86_SUB, "subl $1" },
    { "x++", UNOP_POSTINC, x86_ADD, "addl $1" },
    { "x--", UNOP_POSTDEC, x86_SUB, "subl $1" },
};

// int main() { return <incdec> x; }  must codegen an in-place add/sub of $1.
void test_codegen_incdec_binop() {
    for (size_t c = 0; c < sizeof(incdec_cases) / sizeof(incdec_cases[0]); c++) {
        AstProgram program = make_test_program(
            create_unary_exp(incdec_cases[c].op, create_variable_exp("x")));
        IrProgram ir = emit_ir(&program);
        x86_Program asm_prog = codegen(&ir);

        bool found = false;
        for (x86_Instr* i = asm_prog.functions[0].instrs.head; i; i = i->next) {
            if (i->kind == x86_BINOP && i->as.binop.optype == incdec_cases[c].x86_op
                && i->as.binop.rhs.kind == x86_IMM && i->as.binop.rhs.as.imm == 1) {
                found = true;
            }
        }
        assert(found);

        destroy_x86_program(&asm_prog);
        destroy_program(&program);
    }
    printf("  PASS: test_codegen_incdec_binop\n");
}

// End-to-end: each inc/dec form renders its add/sub through the full pipeline
// and leaves no pseudo-registers behind.
void test_emit_incdec_program() {
    for (size_t c = 0; c < sizeof(incdec_cases) / sizeof(incdec_cases[0]); c++) {
        AstProgram program = make_test_program(
            create_unary_exp(incdec_cases[c].op, create_variable_exp("x")));
        IrProgram ir = emit_ir(&program);
        x86_Program asm_prog = codegen(&ir);

        int stack_offset = rename_registers(&asm_prog.functions[0]);
        allocate_stack(&asm_prog.functions[0], stack_offset);

        char* buf = NULL;
        size_t buf_size = 0;
        FILE* out = open_memstream(&buf, &buf_size);
        emit_asm(&asm_prog, out);
        fclose(out);

        assert(strstr(buf, incdec_cases[c].mnemonic) != NULL);
        assert(strstr(buf, "<pseudo:") == NULL);
        assert(strstr(buf, "???") == NULL);

        free(buf);
        destroy_x86_program(&asm_prog);
        destroy_program(&program);
    }
    printf("  PASS: test_emit_incdec_program\n");
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
    test_x86_new_instr_constructors();
    test_codegen_relational_ops();
    test_codegen_logical_not();
    test_codegen_short_circuit_and();
    test_rename_clears_cmp_setcc_pseudos();
    test_allocate_stack_cmp_two_memory();
    test_allocate_stack_cmp_imm_rhs();
    test_allocate_stack_binop_two_memory();
    test_emit_cmp_setcc_jmp_label();
    test_emit_relational_program();
    test_emit_short_circuit_program();
    test_codegen_incdec_binop();
    test_emit_incdec_program();
    printf("All codegen tests passed!\n");
    return 0;
}
