#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../src/parser/ast.h"
#include "../src/ir/ir.h"

// --- helpers ---

// Wrap a list of statements into a single-function ("main") program.
static AstProgram program_of(AstStatement* stmts, int num_stmts) {
    AstDeclaration* decls = malloc(sizeof(AstDeclaration));
    decls[0] = make_function_decl("main", stmts, num_stmts);
    return make_program(decls, 1);
}

// Wrap a single statement into a one-statement "main" program.
static AstProgram program_of_stmt(AstStatement stmt) {
    AstStatement* body = malloc(sizeof(AstStatement));
    body[0] = stmt;
    return program_of(body, 1);
}

// A temp's or label's name is allocated once and shallow-copied into later
// uses (jump targets, copy dsts, return values), so collect the unique
// pointers first to free each allocation exactly once.
static void free_ir_program(IrProgram* program) {
    for (int f = 0; f < program->size; f++) {
        IrFunction* fn = &program->functions[f];
        char** owned = malloc(fn->size * sizeof(char*));
        int num_owned = 0;
        for (int i = 0; i < fn->size; i++) {
            IrInstruction* ins = &fn->instructions[i];
            char* name = NULL;
            switch (ins->type) {
                case IR_UNOP:
                    if (ins->unary.dst.kind == IR_VARIABLE) name = ins->unary.dst.name;
                    break;
                case IR_BINOP:
                    if (ins->binop.dst.kind == IR_VARIABLE) name = ins->binop.dst.name;
                    break;
                case IR_COPY:
                    if (ins->copy.dst.kind == IR_VARIABLE) name = ins->copy.dst.name;
                    break;
                case IR_LABEL:
                    name = ins->label.identifier;
                    break;
                default:
                    break;
            }
            bool seen = false;
            for (int o = 0; o < num_owned; o++) {
                if (owned[o] == name) seen = true;
            }
            if (name && !seen) owned[num_owned++] = name;
        }
        for (int o = 0; o < num_owned; o++) {
            free(owned[o]);
        }
        free(owned);
        free(fn->instructions);
        free(fn->name);
    }
    free(program->functions);
}

// jump_zero and jump_not_zero are distinct union members, so pick the right
// one based on the instruction type.
static IrVal jump_cond(const IrInstruction* ins) {
    return ins->type == IR_JUMP_ZERO ? ins->jump_zero.cond : ins->jump_not_zero.cond;
}

static const char* jump_target(const IrInstruction* ins) {
    return ins->type == IR_JUMP_ZERO ? ins->jump_zero.target : ins->jump_not_zero.target;
}

// --- tests ---

// int main() { return 2; }
void test_emit_return_constant() {
    AstProgram ast = program_of_stmt(make_return_stmt(create_int_exp(2)));
    IrProgram ir = emit_ir(&ast);

    assert(ir.size == 1);
    IrFunction* fn = &ir.functions[0];
    assert(strcmp(fn->name, "main") == 0);
    assert(fn->size == 1);

    IrInstruction* ret = &fn->instructions[0];
    assert(ret->type == IR_RETURN);
    assert(ret->ret.val.kind == IR_CONSTANT);
    assert(ret->ret.val.int_val == 2);

    free_ir_program(&ir);
    destroy_program(&ast);
    printf("  PASS: test_emit_return_constant\n");
}

// The IR function name must be an independent copy of the AST name.
void test_emit_name_is_owned_copy() {
    AstProgram ast = program_of_stmt(make_return_stmt(create_int_exp(0)));
    IrProgram ir = emit_ir(&ast);

    assert(ir.functions[0].name != ast.decls[0].function.name);
    assert(strcmp(ir.functions[0].name, "main") == 0);

    free_ir_program(&ir);
    destroy_program(&ast);
    printf("  PASS: test_emit_name_is_owned_copy\n");
}

// int main() { return -5; }  ->  unary NEG into a temp, then return the temp.
void test_emit_return_negate() {
    AstProgram ast = program_of_stmt(
        make_return_stmt(create_unary_exp(UNOP_MINUS, create_int_exp(5))));
    IrProgram ir = emit_ir(&ast);

    IrFunction* fn = &ir.functions[0];
    assert(fn->size == 2);

    IrInstruction* unary = &fn->instructions[0];
    assert(unary->type == IR_UNOP);
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
    destroy_program(&ast);
    printf("  PASS: test_emit_return_negate\n");
}

// int main() { return ~3; }  ->  NOT maps to the complement op.
void test_emit_return_complement() {
    AstProgram ast = program_of_stmt(
        make_return_stmt(create_unary_exp(UNOP_COMP, create_int_exp(3))));
    IrProgram ir = emit_ir(&ast);

    IrFunction* fn = &ir.functions[0];
    assert(fn->size == 2);
    assert(fn->instructions[0].type == IR_UNOP);
    assert(fn->instructions[0].unary.op == IR_COMP);
    assert(fn->instructions[0].unary.src.int_val == 3);

    free_ir_program(&ir);
    destroy_program(&ast);
    printf("  PASS: test_emit_return_complement\n");
}

// int main() { return -(~5); }  ->  two temps chained, then return the outer.
void test_emit_nested_unary() {
    AstExp* inner = create_unary_exp(UNOP_COMP, create_int_exp(5));
    AstExp* outer = create_unary_exp(UNOP_MINUS, inner);
    AstProgram ast = program_of_stmt(make_return_stmt(outer));
    IrProgram ir = emit_ir(&ast);

    IrFunction* fn = &ir.functions[0];
    assert(fn->size == 3);

    IrInstruction* in0 = &fn->instructions[0];  // ~5 -> t0
    IrInstruction* in1 = &fn->instructions[1];  // -t0 -> t1
    IrInstruction* in2 = &fn->instructions[2];  // return t1

    assert(in0->type == IR_UNOP && in0->unary.op == IR_COMP);
    assert(in0->unary.src.kind == IR_CONSTANT && in0->unary.src.int_val == 5);

    assert(in1->type == IR_UNOP && in1->unary.op == IR_NEG);
    // The outer op consumes the inner op's result.
    assert(in1->unary.src.kind == IR_VARIABLE);
    assert(strcmp(in1->unary.src.name, in0->unary.dst.name) == 0);

    assert(in2->type == IR_RETURN);
    assert(strcmp(in2->ret.val.name, in1->unary.dst.name) == 0);

    free_ir_program(&ir);
    destroy_program(&ast);
    printf("  PASS: test_emit_nested_unary\n");
}

// A bare-constant expression statement produces no instruction.
// int main() { 5; return 0; }  ->  only the return is emitted.
void test_emit_expr_statement_no_instruction() {
    AstStatement* body = malloc(2 * sizeof(AstStatement));
    body[0] = make_exp_stmt(create_int_exp(5));
    body[1] = make_return_stmt(create_int_exp(0));
    AstProgram ast = program_of(body, 2);
    IrProgram ir = emit_ir(&ast);

    IrFunction* fn = &ir.functions[0];
    assert(fn->size == 1);
    assert(fn->instructions[0].type == IR_RETURN);

    free_ir_program(&ir);
    destroy_program(&ast);
    printf("  PASS: test_emit_expr_statement_no_instruction\n");
}

// A program with more than one function lowers each independently.
void test_emit_multiple_functions() {
    AstStatement* body_a = malloc(sizeof(AstStatement));
    body_a[0] = make_return_stmt(create_int_exp(1));
    AstStatement* body_b = malloc(sizeof(AstStatement));
    body_b[0] = make_return_stmt(create_int_exp(2));

    AstDeclaration* decls = malloc(2 * sizeof(AstDeclaration));
    decls[0] = make_function_decl("foo", body_a, 1);
    decls[1] = make_function_decl("bar", body_b, 1);
    AstProgram ast = make_program(decls, 2);
    IrProgram ir = emit_ir(&ast);

    assert(ir.size == 2);
    assert(strcmp(ir.functions[0].name, "foo") == 0);
    assert(strcmp(ir.functions[1].name, "bar") == 0);
    assert(ir.functions[0].instructions[0].ret.val.int_val == 1);
    assert(ir.functions[1].instructions[0].ret.val.int_val == 2);

    free_ir_program(&ir);
    destroy_program(&ast);
    printf("  PASS: test_emit_multiple_functions\n");
}

// int main() { return 1 + 2; }
//   t0 = add 1, 2
//   return t0
void test_emit_binop_add() {
    AstProgram ast = program_of_stmt(make_return_stmt(
        create_binop_exp(BINOP_ADD, create_int_exp(1), create_int_exp(2))));
    IrProgram ir = emit_ir(&ast);

    IrFunction* fn = &ir.functions[0];
    assert(fn->size == 2);

    IrInstruction* binop = &fn->instructions[0];
    assert(binop->type == IR_BINOP);
    assert(binop->binop.op == IR_ADD);
    assert(binop->binop.lhs.kind == IR_CONSTANT);
    assert(binop->binop.lhs.int_val == 1);
    assert(binop->binop.rhs.kind == IR_CONSTANT);
    assert(binop->binop.rhs.int_val == 2);
    assert(binop->binop.dst.kind == IR_VARIABLE);

    IrInstruction* ret = &fn->instructions[1];
    assert(ret->type == IR_RETURN);
    assert(ret->ret.val.kind == IR_VARIABLE);
    // The return value is exactly the temp produced by the binop.
    assert(strcmp(ret->ret.val.name, binop->binop.dst.name) == 0);

    free_ir_program(&ir);
    destroy_program(&ast);
    printf("  PASS: test_emit_binop_add\n");
}

// Each AST binary operator must lower to its matching IR opcode.
void test_emit_binop_all_ops() {
    struct { AstBinopType ast_op; IrBinopType ir_op; } cases[] = {
        { BINOP_ADD, IR_ADD },
        { BINOP_SUB, IR_SUB },
        { BINOP_MUL, IR_MUL },
        { BINOP_DIV, IR_DIV },
        { BINOP_MOD, IR_MOD },
        { BINOP_AND, IR_AND },
        { BINOP_OR, IR_OR },
        { BINOP_XOR, IR_XOR },
        { BINOP_LSHIFT, IR_LSHIFT },
        { BINOP_RSHIFT, IR_RSHIFT },
    };
    for (size_t c = 0; c < sizeof(cases) / sizeof(cases[0]); c++) {
        AstProgram ast = program_of_stmt(make_return_stmt(
            create_binop_exp(cases[c].ast_op,
                             create_int_exp(7), create_int_exp(3))));
        IrProgram ir = emit_ir(&ast);

        IrInstruction* binop = &ir.functions[0].instructions[0];
        assert(binop->type == IR_BINOP);
        assert(binop->binop.op == cases[c].ir_op);
        assert(binop->binop.lhs.int_val == 7);
        assert(binop->binop.rhs.int_val == 3);

        free_ir_program(&ir);
        destroy_program(&ast);
    }
    printf("  PASS: test_emit_binop_all_ops\n");
}

// int main() { return (1 + 2) * 3; }
// LHS is fully evaluated before RHS, so the inner add is emitted first and the
// outer multiply consumes that temp as its left operand.
void test_emit_binop_nested_lhs_first() {
    AstExp* inner = create_binop_exp(BINOP_ADD,
                                     create_int_exp(1), create_int_exp(2));
    AstExp* outer = create_binop_exp(BINOP_MUL, inner, create_int_exp(3));
    AstProgram ast = program_of_stmt(make_return_stmt(outer));
    IrProgram ir = emit_ir(&ast);

    IrFunction* fn = &ir.functions[0];
    assert(fn->size == 3);

    IrInstruction* add = &fn->instructions[0];   // (1 + 2) -> t0
    IrInstruction* mul = &fn->instructions[1];   // t0 * 3  -> t1
    IrInstruction* ret = &fn->instructions[2];   // return t1

    assert(add->type == IR_BINOP && add->binop.op == IR_ADD);
    assert(add->binop.lhs.int_val == 1 && add->binop.rhs.int_val == 2);

    assert(mul->type == IR_BINOP && mul->binop.op == IR_MUL);
    // The multiply's left operand is the add's result temp; its right is 3.
    assert(mul->binop.lhs.kind == IR_VARIABLE);
    assert(strcmp(mul->binop.lhs.name, add->binop.dst.name) == 0);
    assert(mul->binop.rhs.kind == IR_CONSTANT && mul->binop.rhs.int_val == 3);

    assert(ret->type == IR_RETURN);
    assert(strcmp(ret->ret.val.name, mul->binop.dst.name) == 0);

    free_ir_program(&ir);
    destroy_program(&ast);
    printf("  PASS: test_emit_binop_nested_lhs_first\n");
}

// int main() { return 1 + (2 * 3); }
// The RHS sub-expression is lowered first (it is the deeper operand evaluated
// when we recurse into the right child), and the outer add keeps 1 as its
// constant left operand and the multiply temp as its right operand.
void test_emit_binop_rhs_subexpression() {
    AstExp* inner = create_binop_exp(BINOP_MUL,
                                     create_int_exp(2), create_int_exp(3));
    AstExp* outer = create_binop_exp(BINOP_ADD, create_int_exp(1), inner);
    AstProgram ast = program_of_stmt(make_return_stmt(outer));
    IrProgram ir = emit_ir(&ast);

    IrFunction* fn = &ir.functions[0];
    assert(fn->size == 3);

    IrInstruction* mul = &fn->instructions[0];   // (2 * 3) -> t0
    IrInstruction* add = &fn->instructions[1];   // 1 + t0  -> t1

    assert(mul->type == IR_BINOP && mul->binop.op == IR_MUL);
    assert(add->type == IR_BINOP && add->binop.op == IR_ADD);
    assert(add->binop.lhs.kind == IR_CONSTANT && add->binop.lhs.int_val == 1);
    assert(add->binop.rhs.kind == IR_VARIABLE);
    assert(strcmp(add->binop.rhs.name, mul->binop.dst.name) == 0);

    free_ir_program(&ir);
    destroy_program(&ast);
    printf("  PASS: test_emit_binop_rhs_subexpression\n");
}

// A binop's result temp must be a distinct name from any nested temp.
void test_emit_binop_distinct_temps() {
    AstExp* inner = create_binop_exp(BINOP_SUB,
                                     create_int_exp(9), create_int_exp(4));
    AstExp* outer = create_binop_exp(BINOP_DIV, inner, create_int_exp(2));
    AstProgram ast = program_of_stmt(make_return_stmt(outer));
    IrProgram ir = emit_ir(&ast);

    IrFunction* fn = &ir.functions[0];
    IrInstruction* sub = &fn->instructions[0];
    IrInstruction* div = &fn->instructions[1];
    assert(strcmp(sub->binop.dst.name, div->binop.dst.name) != 0);

    free_ir_program(&ir);
    destroy_program(&ast);
    printf("  PASS: test_emit_binop_distinct_temps\n");
}

// int main() { return (5 & 3) | (1 << 4); }
// Both bitwise sub-expressions are lowered into temps before the outer OR,
// which consumes the two temps as its operands.
void test_emit_binop_bitwise_nested() {
    AstExp* and_exp = create_binop_exp(BINOP_AND,
                                       create_int_exp(5), create_int_exp(3));
    AstExp* shl_exp = create_binop_exp(BINOP_LSHIFT,
                                       create_int_exp(1), create_int_exp(4));
    AstExp* or_exp = create_binop_exp(BINOP_OR, and_exp, shl_exp);
    AstProgram ast = program_of_stmt(make_return_stmt(or_exp));
    IrProgram ir = emit_ir(&ast);

    IrFunction* fn = &ir.functions[0];
    assert(fn->size == 4);

    IrInstruction* and_ins = &fn->instructions[0];  // 5 & 3   -> t0
    IrInstruction* shl_ins = &fn->instructions[1];  // 1 << 4  -> t1
    IrInstruction* or_ins  = &fn->instructions[2];  // t0 | t1 -> t2
    IrInstruction* ret     = &fn->instructions[3];  // return t2

    assert(and_ins->type == IR_BINOP && and_ins->binop.op == IR_AND);
    assert(and_ins->binop.lhs.int_val == 5 && and_ins->binop.rhs.int_val == 3);

    assert(shl_ins->type == IR_BINOP && shl_ins->binop.op == IR_LSHIFT);
    assert(shl_ins->binop.lhs.int_val == 1 && shl_ins->binop.rhs.int_val == 4);

    assert(or_ins->type == IR_BINOP && or_ins->binop.op == IR_OR);
    assert(or_ins->binop.lhs.kind == IR_VARIABLE);
    assert(strcmp(or_ins->binop.lhs.name, and_ins->binop.dst.name) == 0);
    assert(or_ins->binop.rhs.kind == IR_VARIABLE);
    assert(strcmp(or_ins->binop.rhs.name, shl_ins->binop.dst.name) == 0);

    assert(ret->type == IR_RETURN);
    assert(strcmp(ret->ret.val.name, or_ins->binop.dst.name) == 0);

    free_ir_program(&ir);
    destroy_program(&ast);
    printf("  PASS: test_emit_binop_bitwise_nested\n");
}

// int main() { return 1 op 2; } for a short-circuiting op lowers to:
//   jump(1, short)        conditional: JUMP_ZERO for &&, JUMP_NOT_ZERO for ||
//   jump(2, short)
//   dst = !short_circuit_value
//   jump end
// short:
//   dst = short_circuit_value
// end:
//   return dst
static void check_short_circuit(AstBinopType op, IrInstructionType cond_jump_type,
                                int short_circuit_value) {
    AstProgram ast = program_of_stmt(make_return_stmt(
        create_binop_exp(op, create_int_exp(1), create_int_exp(2))));
    IrProgram ir = emit_ir(&ast);

    IrFunction* fn = &ir.functions[0];
    assert(fn->size == 8);

    IrInstruction* jump_lhs    = &fn->instructions[0];
    IrInstruction* jump_rhs    = &fn->instructions[1];
    IrInstruction* store_fall  = &fn->instructions[2];
    IrInstruction* jump_end    = &fn->instructions[3];
    IrInstruction* short_label = &fn->instructions[4];
    IrInstruction* store_short = &fn->instructions[5];
    IrInstruction* end_label   = &fn->instructions[6];
    IrInstruction* ret         = &fn->instructions[7];

    assert(short_label->type == IR_LABEL);
    assert(end_label->type == IR_LABEL);
    assert(strcmp(short_label->label.identifier, end_label->label.identifier) != 0);

    // both operands take the same conditional jump to the short-circuit label
    assert(jump_lhs->type == cond_jump_type);
    assert(jump_cond(jump_lhs).kind == IR_CONSTANT);
    assert(jump_cond(jump_lhs).int_val == 1);
    assert(strcmp(jump_target(jump_lhs), short_label->label.identifier) == 0);

    assert(jump_rhs->type == cond_jump_type);
    assert(jump_cond(jump_rhs).kind == IR_CONSTANT);
    assert(jump_cond(jump_rhs).int_val == 2);
    assert(strcmp(jump_target(jump_rhs), short_label->label.identifier) == 0);

    // fall-through stores the opposite of the short-circuit value, then skips
    // past the short-circuit store
    assert(store_fall->type == IR_COPY);
    assert(store_fall->copy.src.kind == IR_CONSTANT);
    assert(store_fall->copy.src.int_val == !short_circuit_value);
    assert(store_fall->copy.dst.kind == IR_VARIABLE);

    assert(jump_end->type == IR_JUMP);
    assert(strcmp(jump_end->jump.target, end_label->label.identifier) == 0);

    // short-circuit stores its value into the same destination temp
    assert(store_short->type == IR_COPY);
    assert(store_short->copy.src.kind == IR_CONSTANT);
    assert(store_short->copy.src.int_val == short_circuit_value);
    assert(strcmp(store_short->copy.dst.name, store_fall->copy.dst.name) == 0);

    assert(ret->type == IR_RETURN);
    assert(ret->ret.val.kind == IR_VARIABLE);
    assert(strcmp(ret->ret.val.name, store_fall->copy.dst.name) == 0);

    free_ir_program(&ir);
    destroy_program(&ast);
}

// int main() { return 1 && 2; }  ->  jump-if-zero to the 0-result label.
void test_emit_logical_and_short_circuit() {
    check_short_circuit(BINOP_LAND, IR_JUMP_ZERO, 0);
    printf("  PASS: test_emit_logical_and_short_circuit\n");
}

// int main() { return 1 || 2; }  ->  jump-if-not-zero to the 1-result label.
void test_emit_logical_or_short_circuit() {
    check_short_circuit(BINOP_LOR, IR_JUMP_NOT_ZERO, 1);
    printf("  PASS: test_emit_logical_or_short_circuit\n");
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
    test_emit_binop_add();
    test_emit_binop_all_ops();
    test_emit_binop_nested_lhs_first();
    test_emit_binop_rhs_subexpression();
    test_emit_binop_distinct_temps();
    test_emit_binop_bitwise_nested();
    test_emit_logical_and_short_circuit();
    test_emit_logical_or_short_circuit();
    printf("All IR tests passed!\n");
    return 0;
}
