#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../src/parser/ast.h"
#include "../src/ir/ir.h"

// --- helpers ---

// Wrap a list of statements into a single-function ("main") program.
// Takes ownership of `stmts` (each statement is copied into the function).
static AstProgram program_of(AstStatement* stmts, int num_stmts) {
    AstFunction fn = ast_function_make("main", ast_block_make(num_stmts > 0 ? num_stmts : 1));
    for (int i = 0; i < num_stmts; i++) {
        ast_function_append(&fn, (AstBlockItem){
            .type = AST_STATEMENT,
            .as.stmt = stmts[i],
        });
    }
    free(stmts);
    AstFunction* functions = malloc(sizeof(AstFunction));
    functions[0] = fn;
    return ast_program_create(functions, 1);
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
                    if (ins->as.unary.dst.kind == IR_VARIABLE) name = ins->as.unary.dst.as.name;
                    break;
                case IR_BINOP:
                    if (ins->as.binop.dst.kind == IR_VARIABLE) name = ins->as.binop.dst.as.name;
                    break;
                case IR_COPY:
                    if (ins->as.copy.dst.kind == IR_VARIABLE) name = ins->as.copy.dst.as.name;
                    break;
                case IR_LABEL:
                    name = ins->as.label.identifier;
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
    return ins->type == IR_JUMP_ZERO ? ins->as.jump_zero.cond : ins->as.jump_not_zero.cond;
}

static const char* jump_target(const IrInstruction* ins) {
    return ins->type == IR_JUMP_ZERO ? ins->as.jump_zero.target : ins->as.jump_not_zero.target;
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
    assert(ret->as.ret.val.kind == IR_CONSTANT);
    assert(ret->as.ret.val.as.int_val == 2);

    free_ir_program(&ir);
    destroy_program(&ast);
    printf("  PASS: test_emit_return_constant\n");
}

// The IR function name must be an independent copy of the AST name.
void test_emit_name_is_owned_copy() {
    AstProgram ast = program_of_stmt(make_return_stmt(create_int_exp(0)));
    IrProgram ir = emit_ir(&ast);

    assert(ir.functions[0].name != ast.functions[0].identifier);
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
    assert(unary->as.unary.op == IR_NEG);
    assert(unary->as.unary.src.kind == IR_CONSTANT);
    assert(unary->as.unary.src.as.int_val == 5);
    assert(unary->as.unary.dst.kind == IR_VARIABLE);

    IrInstruction* ret = &fn->instructions[1];
    assert(ret->type == IR_RETURN);
    assert(ret->as.ret.val.kind == IR_VARIABLE);
    // The return value is exactly the temp produced by the unary op.
    assert(strcmp(ret->as.ret.val.as.name, unary->as.unary.dst.as.name) == 0);

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
    assert(fn->instructions[0].as.unary.op == IR_COMP);
    assert(fn->instructions[0].as.unary.src.as.int_val == 3);

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

    assert(in0->type == IR_UNOP && in0->as.unary.op == IR_COMP);
    assert(in0->as.unary.src.kind == IR_CONSTANT && in0->as.unary.src.as.int_val == 5);

    assert(in1->type == IR_UNOP && in1->as.unary.op == IR_NEG);
    // The outer op consumes the inner op's result.
    assert(in1->as.unary.src.kind == IR_VARIABLE);
    assert(strcmp(in1->as.unary.src.as.name, in0->as.unary.dst.as.name) == 0);

    assert(in2->type == IR_RETURN);
    assert(strcmp(in2->as.ret.val.as.name, in1->as.unary.dst.as.name) == 0);

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
    AstFunction foo = ast_function_make("foo", ast_block_make(1));
    ast_function_append(&foo, (AstBlockItem){
        .type = AST_STATEMENT, .as.stmt = make_return_stmt(create_int_exp(1))});
    AstFunction bar = ast_function_make("bar", ast_block_make(1));
    ast_function_append(&bar, (AstBlockItem){
        .type = AST_STATEMENT, .as.stmt = make_return_stmt(create_int_exp(2))});

    AstFunction* functions = malloc(2 * sizeof(AstFunction));
    functions[0] = foo;
    functions[1] = bar;
    AstProgram ast = ast_program_create(functions, 2);
    IrProgram ir = emit_ir(&ast);

    assert(ir.size == 2);
    assert(strcmp(ir.functions[0].name, "foo") == 0);
    assert(strcmp(ir.functions[1].name, "bar") == 0);
    assert(ir.functions[0].instructions[0].as.ret.val.as.int_val == 1);
    assert(ir.functions[1].instructions[0].as.ret.val.as.int_val == 2);

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
    assert(binop->as.binop.op == IR_ADD);
    assert(binop->as.binop.lhs.kind == IR_CONSTANT);
    assert(binop->as.binop.lhs.as.int_val == 1);
    assert(binop->as.binop.rhs.kind == IR_CONSTANT);
    assert(binop->as.binop.rhs.as.int_val == 2);
    assert(binop->as.binop.dst.kind == IR_VARIABLE);

    IrInstruction* ret = &fn->instructions[1];
    assert(ret->type == IR_RETURN);
    assert(ret->as.ret.val.kind == IR_VARIABLE);
    // The return value is exactly the temp produced by the binop.
    assert(strcmp(ret->as.ret.val.as.name, binop->as.binop.dst.as.name) == 0);

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
        assert(binop->as.binop.op == cases[c].ir_op);
        assert(binop->as.binop.lhs.as.int_val == 7);
        assert(binop->as.binop.rhs.as.int_val == 3);

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

    assert(add->type == IR_BINOP && add->as.binop.op == IR_ADD);
    assert(add->as.binop.lhs.as.int_val == 1 && add->as.binop.rhs.as.int_val == 2);

    assert(mul->type == IR_BINOP && mul->as.binop.op == IR_MUL);
    // The multiply's left operand is the add's result temp; its right is 3.
    assert(mul->as.binop.lhs.kind == IR_VARIABLE);
    assert(strcmp(mul->as.binop.lhs.as.name, add->as.binop.dst.as.name) == 0);
    assert(mul->as.binop.rhs.kind == IR_CONSTANT && mul->as.binop.rhs.as.int_val == 3);

    assert(ret->type == IR_RETURN);
    assert(strcmp(ret->as.ret.val.as.name, mul->as.binop.dst.as.name) == 0);

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

    assert(mul->type == IR_BINOP && mul->as.binop.op == IR_MUL);
    assert(add->type == IR_BINOP && add->as.binop.op == IR_ADD);
    assert(add->as.binop.lhs.kind == IR_CONSTANT && add->as.binop.lhs.as.int_val == 1);
    assert(add->as.binop.rhs.kind == IR_VARIABLE);
    assert(strcmp(add->as.binop.rhs.as.name, mul->as.binop.dst.as.name) == 0);

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
    assert(strcmp(sub->as.binop.dst.as.name, div->as.binop.dst.as.name) != 0);

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

    assert(and_ins->type == IR_BINOP && and_ins->as.binop.op == IR_AND);
    assert(and_ins->as.binop.lhs.as.int_val == 5 && and_ins->as.binop.rhs.as.int_val == 3);

    assert(shl_ins->type == IR_BINOP && shl_ins->as.binop.op == IR_LSHIFT);
    assert(shl_ins->as.binop.lhs.as.int_val == 1 && shl_ins->as.binop.rhs.as.int_val == 4);

    assert(or_ins->type == IR_BINOP && or_ins->as.binop.op == IR_OR);
    assert(or_ins->as.binop.lhs.kind == IR_VARIABLE);
    assert(strcmp(or_ins->as.binop.lhs.as.name, and_ins->as.binop.dst.as.name) == 0);
    assert(or_ins->as.binop.rhs.kind == IR_VARIABLE);
    assert(strcmp(or_ins->as.binop.rhs.as.name, shl_ins->as.binop.dst.as.name) == 0);

    assert(ret->type == IR_RETURN);
    assert(strcmp(ret->as.ret.val.as.name, or_ins->as.binop.dst.as.name) == 0);

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
    assert(strcmp(short_label->as.label.identifier, end_label->as.label.identifier) != 0);

    // both operands take the same conditional jump to the short-circuit label
    assert(jump_lhs->type == cond_jump_type);
    assert(jump_cond(jump_lhs).kind == IR_CONSTANT);
    assert(jump_cond(jump_lhs).as.int_val == 1);
    assert(strcmp(jump_target(jump_lhs), short_label->as.label.identifier) == 0);

    assert(jump_rhs->type == cond_jump_type);
    assert(jump_cond(jump_rhs).kind == IR_CONSTANT);
    assert(jump_cond(jump_rhs).as.int_val == 2);
    assert(strcmp(jump_target(jump_rhs), short_label->as.label.identifier) == 0);

    // fall-through stores the opposite of the short-circuit value, then skips
    // past the short-circuit store
    assert(store_fall->type == IR_COPY);
    assert(store_fall->as.copy.src.kind == IR_CONSTANT);
    assert(store_fall->as.copy.src.as.int_val == !short_circuit_value);
    assert(store_fall->as.copy.dst.kind == IR_VARIABLE);

    assert(jump_end->type == IR_JUMP);
    assert(strcmp(jump_end->as.jump.target, end_label->as.label.identifier) == 0);

    // short-circuit stores its value into the same destination temp
    assert(store_short->type == IR_COPY);
    assert(store_short->as.copy.src.kind == IR_CONSTANT);
    assert(store_short->as.copy.src.as.int_val == short_circuit_value);
    assert(strcmp(store_short->as.copy.dst.as.name, store_fall->as.copy.dst.as.name) == 0);

    assert(ret->type == IR_RETURN);
    assert(ret->as.ret.val.kind == IR_VARIABLE);
    assert(strcmp(ret->as.ret.val.as.name, store_fall->as.copy.dst.as.name) == 0);

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

// Each relational AST operator lowers to a plain IR_BINOP carrying the matching
// relational opcode (they are not short-circuited like && / ||).
void test_emit_relational_ops() {
    struct { AstBinopType ast_op; IrBinopType ir_op; } cases[] = {
        { BINOP_EQ,      IR_EQ      },
        { BINOP_NEQ,     IR_NEQ     },
        { BINOP_LESS,    IR_LESS    },
        { BINOP_GREATER, IR_GREATER },
        { BINOP_LEQ,     IR_LEQ     },
        { BINOP_GEQ,     IR_GEQ     },
    };
    for (size_t c = 0; c < sizeof(cases) / sizeof(cases[0]); c++) {
        AstProgram ast = program_of_stmt(make_return_stmt(
            create_binop_exp(cases[c].ast_op,
                             create_int_exp(4), create_int_exp(5))));
        IrProgram ir = emit_ir(&ast);

        IrFunction* fn = &ir.functions[0];
        assert(fn->size == 2);
        IrInstruction* binop = &fn->instructions[0];
        assert(binop->type == IR_BINOP);
        assert(binop->as.binop.op == cases[c].ir_op);
        assert(binop->as.binop.lhs.as.int_val == 4);
        assert(binop->as.binop.rhs.as.int_val == 5);

        IrInstruction* ret = &fn->instructions[1];
        assert(ret->type == IR_RETURN);
        assert(strcmp(ret->as.ret.val.as.name, binop->as.binop.dst.as.name) == 0);

        free_ir_program(&ir);
        destroy_program(&ast);
    }
    printf("  PASS: test_emit_relational_ops\n");
}

// int main() { return !5; }  ->  logical NOT lowers to the IR_NOT unary op.
void test_emit_logical_not() {
    AstProgram ast = program_of_stmt(
        make_return_stmt(create_unary_exp(UNOP_NOT, create_int_exp(5))));
    IrProgram ir = emit_ir(&ast);

    IrFunction* fn = &ir.functions[0];
    assert(fn->size == 2);
    assert(fn->instructions[0].type == IR_UNOP);
    assert(fn->instructions[0].as.unary.op == IR_NOT);
    assert(fn->instructions[0].as.unary.src.kind == IR_CONSTANT);
    assert(fn->instructions[0].as.unary.src.as.int_val == 5);

    free_ir_program(&ir);
    destroy_program(&ast);
    printf("  PASS: test_emit_logical_not\n");
}

// Nested short-circuit operators each allocate their own pair of labels, so a
// program with two && expressions must emit four distinct labels.
void test_emit_nested_short_circuit_unique_labels() {
    // (1 && 2) && 3
    AstExp* inner = create_binop_exp(BINOP_LAND,
                                     create_int_exp(1), create_int_exp(2));
    AstExp* outer = create_binop_exp(BINOP_LAND, inner, create_int_exp(3));
    AstProgram ast = program_of_stmt(make_return_stmt(outer));
    IrProgram ir = emit_ir(&ast);

    IrFunction* fn = &ir.functions[0];
    const char* labels[8];
    int num_labels = 0;
    for (int i = 0; i < fn->size; i++) {
        if (fn->instructions[i].type == IR_LABEL) {
            labels[num_labels++] = fn->instructions[i].as.label.identifier;
        }
    }
    assert(num_labels == 4);
    for (int a = 0; a < num_labels; a++) {
        for (int b = a + 1; b < num_labels; b++) {
            assert(strcmp(labels[a], labels[b]) != 0);
        }
    }

    free_ir_program(&ir);
    destroy_program(&ast);
    printf("  PASS: test_emit_nested_short_circuit_unique_labels\n");
}

// int main() { x = 5; }  ->  a plain assignment lowers to a single COPY of the
// rhs into the variable (no arithmetic).
void test_emit_assign_plain() {
    AstProgram ast = program_of_stmt(make_exp_stmt(
        create_assign_exp(ASSIGN_NOP, create_variable_exp("x"), create_int_exp(5))));
    IrProgram ir = emit_ir(&ast);

    IrFunction* fn = &ir.functions[0];
    assert(fn->size == 1);
    IrInstruction* copy = &fn->instructions[0];
    assert(copy->type == IR_COPY);
    assert(copy->as.copy.src.kind == IR_CONSTANT && copy->as.copy.src.as.int_val == 5);
    assert(copy->as.copy.dst.kind == IR_VARIABLE);
    assert(strcmp(copy->as.copy.dst.as.name, "x") == 0);

    free_ir_program(&ir);
    destroy_program(&ast);
    printf("  PASS: test_emit_assign_plain\n");
}

// Each compound assignment `x <op>= e` lowers to a single BINOP that reads the
// variable as its left operand and writes the result back into the same
// variable: `x = x <op> e`.
void test_emit_compound_assign_all_ops() {
    struct { AstAssignOp assign_op; IrBinopType ir_op; const char* name; } cases[] = {
        { ASSIGN_ADD,    IR_ADD,    "+=" },
        { ASSIGN_SUB,    IR_SUB,    "-=" },
        { ASSIGN_MUL,    IR_MUL,    "*=" },
        { ASSIGN_DIV,    IR_DIV,    "/=" },
        { ASSIGN_MOD,    IR_MOD,    "%=" },
        { ASSIGN_AND,    IR_AND,    "&=" },
        { ASSIGN_OR,     IR_OR,     "|=" },
        { ASSIGN_XOR,    IR_XOR,    "^=" },
        { ASSIGN_RSHIFT, IR_RSHIFT, ">>=" },
        { ASSIGN_LSHIFT, IR_LSHIFT, "<<=" },
    };
    for (size_t c = 0; c < sizeof(cases) / sizeof(cases[0]); c++) {
        AstProgram ast = program_of_stmt(make_exp_stmt(
            create_assign_exp(cases[c].assign_op,
                              create_variable_exp("x"), create_int_exp(5))));
        IrProgram ir = emit_ir(&ast);

        IrFunction* fn = &ir.functions[0];
        assert(fn->size == 1);
        IrInstruction* binop = &fn->instructions[0];
        if (binop->type != IR_BINOP || binop->as.binop.op != cases[c].ir_op) {
            printf("  FAIL: compound assign %s lowered to wrong op "
                   "(expected IR op %d, got type %d op %d)\n",
                   cases[c].name, cases[c].ir_op, binop->type, binop->as.binop.op);
            exit(1);
        }
        // left operand and destination are both the variable; rhs is the constant
        assert(binop->as.binop.lhs.kind == IR_VARIABLE);
        assert(strcmp(binop->as.binop.lhs.as.name, "x") == 0);
        assert(binop->as.binop.dst.kind == IR_VARIABLE);
        assert(strcmp(binop->as.binop.dst.as.name, "x") == 0);
        assert(binop->as.binop.rhs.kind == IR_CONSTANT && binop->as.binop.rhs.as.int_val == 5);

        free_ir_program(&ir);
        destroy_program(&ast);
    }
    printf("  PASS: test_emit_compound_assign_all_ops\n");
}

// --- increment / decrement lowering ---
//
// These assume the local lowering discussed for the (not-yet-implemented)
// inc/dec pass; adjust the expected shapes if a different lowering is chosen.

// Prefix `++x` / `--x` mutates the variable in place and the expression yields
// the variable itself:
//   x = x <+/-> 1
//   return x
static void check_prefix(const char* desc, AstUnopType op, IrBinopType ir_op) {
    AstProgram ast = program_of_stmt(make_return_stmt(
        create_unary_exp(op, create_variable_exp("x"))));
    IrProgram ir = emit_ir(&ast);

    IrFunction* fn = &ir.functions[0];
    assert(fn->size == 2);

    IrInstruction* binop = &fn->instructions[0];
    assert(binop->type == IR_BINOP);
    assert(binop->as.binop.op == ir_op);
    assert(binop->as.binop.lhs.kind == IR_VARIABLE && strcmp(binop->as.binop.lhs.as.name, "x") == 0);
    assert(binop->as.binop.rhs.kind == IR_CONSTANT && binop->as.binop.rhs.as.int_val == 1);
    assert(binop->as.binop.dst.kind == IR_VARIABLE && strcmp(binop->as.binop.dst.as.name, "x") == 0);

    IrInstruction* ret = &fn->instructions[1];
    assert(ret->type == IR_RETURN);
    assert(ret->as.ret.val.kind == IR_VARIABLE && strcmp(ret->as.ret.val.as.name, "x") == 0);

    free_ir_program(&ir);
    destroy_program(&ast);
    printf("  PASS: %s\n", desc);
}

// Postfix `x++` / `x--` saves the old value into a fresh temp, mutates the
// variable, and yields the saved temp (not the mutated variable):
//   tmp = x
//   x = x <+/-> 1
//   return tmp
static void check_postfix(const char* desc, AstUnopType op, IrBinopType ir_op) {
    AstProgram ast = program_of_stmt(make_return_stmt(
        create_unary_exp(op, create_variable_exp("x"))));
    IrProgram ir = emit_ir(&ast);

    IrFunction* fn = &ir.functions[0];
    assert(fn->size == 3);

    IrInstruction* copy = &fn->instructions[0];
    assert(copy->type == IR_COPY);
    assert(copy->as.copy.src.kind == IR_VARIABLE && strcmp(copy->as.copy.src.as.name, "x") == 0);
    assert(copy->as.copy.dst.kind == IR_VARIABLE);
    assert(strcmp(copy->as.copy.dst.as.name, "x") != 0);  // a fresh temp, not x

    IrInstruction* binop = &fn->instructions[1];
    assert(binop->type == IR_BINOP);
    assert(binop->as.binop.op == ir_op);
    assert(binop->as.binop.lhs.kind == IR_VARIABLE && strcmp(binop->as.binop.lhs.as.name, "x") == 0);
    assert(binop->as.binop.rhs.kind == IR_CONSTANT && binop->as.binop.rhs.as.int_val == 1);
    assert(binop->as.binop.dst.kind == IR_VARIABLE && strcmp(binop->as.binop.dst.as.name, "x") == 0);

    IrInstruction* ret = &fn->instructions[2];
    assert(ret->type == IR_RETURN);
    assert(ret->as.ret.val.kind == IR_VARIABLE);
    assert(strcmp(ret->as.ret.val.as.name, copy->as.copy.dst.as.name) == 0);  // the saved old value
    assert(strcmp(ret->as.ret.val.as.name, "x") != 0);

    free_ir_program(&ir);
    destroy_program(&ast);
    printf("  PASS: %s\n", desc);
}

void test_emit_prefix_increment()  { check_prefix("return ++x", UNOP_PREINC, IR_ADD); }
void test_emit_prefix_decrement()  { check_prefix("return --x", UNOP_PREDEC, IR_SUB); }
void test_emit_postfix_increment() { check_postfix("return x++", UNOP_POSTINC, IR_ADD); }
void test_emit_postfix_decrement() { check_postfix("return x--", UNOP_POSTDEC, IR_SUB); }

// --- conditional expression (ternary) lowering ---

// Heap-allocate a statement so it can be owned by an if-statement's branch
// pointers (which destroy_stmt frees).
static AstStatement* heap_stmt(AstStatement stmt) {
    AstStatement* p = malloc(sizeof(AstStatement));
    *p = stmt;
    return p;
}

// int main() { return 1 ? 2 : 3; }  lowers to:
//   jump_zero(1, false)
//   copy 2 -> result
//   jump end
// false:
//   copy 3 -> result
// end:
//   return result
void test_emit_conditional_expression() {
    AstProgram ast = program_of_stmt(make_return_stmt(
        create_conditional_exp(create_int_exp(1), create_int_exp(2), create_int_exp(3))));
    IrProgram ir = emit_ir(&ast);

    IrFunction* fn = &ir.functions[0];
    assert(fn->size == 7);

    IrInstruction* jz         = &fn->instructions[0];
    IrInstruction* copy_true  = &fn->instructions[1];
    IrInstruction* jmp_end    = &fn->instructions[2];
    IrInstruction* false_lbl  = &fn->instructions[3];
    IrInstruction* copy_false = &fn->instructions[4];
    IrInstruction* end_lbl    = &fn->instructions[5];
    IrInstruction* ret        = &fn->instructions[6];

    // condition (constant 1) jumps to the false branch when zero
    assert(jz->type == IR_JUMP_ZERO);
    assert(jump_cond(jz).kind == IR_CONSTANT && jump_cond(jz).as.int_val == 1);
    assert(strcmp(jump_target(jz), false_lbl->as.label.identifier) == 0);

    // true branch copies 2 into the shared result temp, then skips the false arm
    assert(copy_true->type == IR_COPY);
    assert(copy_true->as.copy.src.kind == IR_CONSTANT && copy_true->as.copy.src.as.int_val == 2);
    assert(copy_true->as.copy.dst.kind == IR_VARIABLE);
    assert(jmp_end->type == IR_JUMP);
    assert(strcmp(jmp_end->as.jump.target, end_lbl->as.label.identifier) == 0);

    // false branch copies 3 into the *same* result temp
    assert(false_lbl->type == IR_LABEL);
    assert(copy_false->type == IR_COPY);
    assert(copy_false->as.copy.src.kind == IR_CONSTANT && copy_false->as.copy.src.as.int_val == 3);
    assert(strcmp(copy_false->as.copy.dst.as.name, copy_true->as.copy.dst.as.name) == 0);

    // the two labels are distinct, and the result temp is what's returned
    assert(end_lbl->type == IR_LABEL);
    assert(strcmp(false_lbl->as.label.identifier, end_lbl->as.label.identifier) != 0);
    assert(ret->type == IR_RETURN);
    assert(strcmp(ret->as.ret.val.as.name, copy_true->as.copy.dst.as.name) == 0);

    free_ir_program(&ir);
    destroy_program(&ast);
    printf("  PASS: test_emit_conditional_expression\n");
}

// Each ternary allocates its own label pair, so nesting one in the true arm
// yields four distinct labels.
void test_emit_conditional_nested_unique_labels() {
    // 1 ? (2 ? 3 : 4) : 5
    AstExp* inner = create_conditional_exp(
        create_int_exp(2), create_int_exp(3), create_int_exp(4));
    AstExp* outer = create_conditional_exp(create_int_exp(1), inner, create_int_exp(5));
    AstProgram ast = program_of_stmt(make_return_stmt(outer));
    IrProgram ir = emit_ir(&ast);

    IrFunction* fn = &ir.functions[0];
    const char* labels[8];
    int num_labels = 0;
    for (int i = 0; i < fn->size; i++) {
        if (fn->instructions[i].type == IR_LABEL) {
            labels[num_labels++] = fn->instructions[i].as.label.identifier;
        }
    }
    assert(num_labels == 4);
    for (int a = 0; a < num_labels; a++) {
        for (int b = a + 1; b < num_labels; b++) {
            assert(strcmp(labels[a], labels[b]) != 0);
        }
    }

    free_ir_program(&ir);
    destroy_program(&ast);
    printf("  PASS: test_emit_conditional_nested_unique_labels\n");
}

// --- if-statement lowering ---

// int main() { if (1) return 2; }  with no else lowers to a single forward
// branch — no else label and no unconditional jump:
//   jump_zero(1, end)
//   return 2
// end:
void test_emit_if_no_else() {
    AstProgram ast = program_of_stmt(make_if_stmt(
        create_int_exp(1),
        heap_stmt(make_return_stmt(create_int_exp(2))),
        NULL));
    IrProgram ir = emit_ir(&ast);

    IrFunction* fn = &ir.functions[0];
    assert(fn->size == 3);

    IrInstruction* jz      = &fn->instructions[0];
    IrInstruction* ret     = &fn->instructions[1];
    IrInstruction* end_lbl = &fn->instructions[2];

    assert(jz->type == IR_JUMP_ZERO);
    assert(jump_cond(jz).kind == IR_CONSTANT && jump_cond(jz).as.int_val == 1);
    // the false path jumps straight to the end label
    assert(strcmp(jump_target(jz), end_lbl->as.label.identifier) == 0);

    assert(ret->type == IR_RETURN);
    assert(ret->as.ret.val.kind == IR_CONSTANT && ret->as.ret.val.as.int_val == 2);

    assert(end_lbl->type == IR_LABEL);

    // no unconditional jump and no second label are emitted when there is no else
    for (int i = 0; i < fn->size; i++) {
        assert(fn->instructions[i].type != IR_JUMP);
    }

    free_ir_program(&ir);
    destroy_program(&ast);
    printf("  PASS: test_emit_if_no_else\n");
}

// int main() { if (1) return 2; else return 3; }  lowers to:
//   jump_zero(1, else)
//   return 2
//   jump end
// else:
//   return 3
// end:
void test_emit_if_with_else() {
    AstProgram ast = program_of_stmt(make_if_stmt(
        create_int_exp(1),
        heap_stmt(make_return_stmt(create_int_exp(2))),
        heap_stmt(make_return_stmt(create_int_exp(3)))));
    IrProgram ir = emit_ir(&ast);

    IrFunction* fn = &ir.functions[0];
    assert(fn->size == 6);

    IrInstruction* jz       = &fn->instructions[0];
    IrInstruction* then_ret = &fn->instructions[1];
    IrInstruction* jmp_end  = &fn->instructions[2];
    IrInstruction* else_lbl = &fn->instructions[3];
    IrInstruction* else_ret = &fn->instructions[4];
    IrInstruction* end_lbl  = &fn->instructions[5];

    // condition jumps to the else label when zero
    assert(jz->type == IR_JUMP_ZERO);
    assert(strcmp(jump_target(jz), else_lbl->as.label.identifier) == 0);

    // then branch returns 2, then skips past the else branch
    assert(then_ret->type == IR_RETURN && then_ret->as.ret.val.as.int_val == 2);
    assert(jmp_end->type == IR_JUMP);
    assert(strcmp(jmp_end->as.jump.target, end_lbl->as.label.identifier) == 0);

    // else branch returns 3
    assert(else_lbl->type == IR_LABEL);
    assert(else_ret->type == IR_RETURN && else_ret->as.ret.val.as.int_val == 3);

    // the else and end labels are distinct
    assert(end_lbl->type == IR_LABEL);
    assert(strcmp(else_lbl->as.label.identifier, end_lbl->as.label.identifier) != 0);

    free_ir_program(&ir);
    destroy_program(&ast);
    printf("  PASS: test_emit_if_with_else\n");
}

// The condition is a real expression: `if (a < b)` lowers the comparison into a
// temp first, and that temp (not a constant) is what the jump tests.
void test_emit_if_cond_is_expression() {
    AstProgram ast = program_of_stmt(make_if_stmt(
        create_binop_exp(BINOP_LESS, create_variable_exp("a"), create_variable_exp("b")),
        heap_stmt(make_return_stmt(create_int_exp(1))),
        NULL));
    IrProgram ir = emit_ir(&ast);

    IrFunction* fn = &ir.functions[0];

    // the relational compare is lowered before the branch
    IrInstruction* cmp = &fn->instructions[0];
    assert(cmp->type == IR_BINOP && cmp->as.binop.op == IR_LESS);

    IrInstruction* jz = &fn->instructions[1];
    assert(jz->type == IR_JUMP_ZERO);
    // the branch tests the compare's result temp, not a constant
    assert(jump_cond(jz).kind == IR_VARIABLE);
    assert(strcmp(jump_cond(jz).as.name, cmp->as.binop.dst.as.name) == 0);

    free_ir_program(&ir);
    destroy_program(&ast);
    printf("  PASS: test_emit_if_cond_is_expression\n");
}

// A nested if (if inside the then branch) lowers recursively: the outer branch
// guards the inner branch, producing two distinct jump_zero/label pairs.
void test_emit_if_nested() {
    AstStatement* inner = heap_stmt(make_if_stmt(
        create_int_exp(1),
        heap_stmt(make_return_stmt(create_int_exp(2))),
        NULL));
    AstProgram ast = program_of_stmt(make_if_stmt(create_int_exp(3), inner, NULL));
    IrProgram ir = emit_ir(&ast);

    IrFunction* fn = &ir.functions[0];
    int jump_zeros = 0, labels = 0;
    for (int i = 0; i < fn->size; i++) {
        if (fn->instructions[i].type == IR_JUMP_ZERO) jump_zeros++;
        if (fn->instructions[i].type == IR_LABEL) labels++;
    }
    // one branch + end label per if
    assert(jump_zeros == 2);
    assert(labels == 2);

    free_ir_program(&ir);
    destroy_program(&ast);
    printf("  PASS: test_emit_if_nested\n");
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
    test_emit_relational_ops();
    test_emit_logical_not();
    test_emit_nested_short_circuit_unique_labels();
    test_emit_assign_plain();
    test_emit_compound_assign_all_ops();
    test_emit_prefix_increment();
    test_emit_prefix_decrement();
    test_emit_postfix_increment();
    test_emit_postfix_decrement();
    test_emit_conditional_expression();
    test_emit_conditional_nested_unique_labels();
    test_emit_if_no_else();
    test_emit_if_with_else();
    test_emit_if_cond_is_expression();
    test_emit_if_nested();
    printf("All IR tests passed!\n");
    return 0;
}
