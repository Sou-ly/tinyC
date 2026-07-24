#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../src/parser/ast.h"
#include "../src/ir/ir.h"

// --- helpers ---

// A function definition named `identifier` whose body is `body`.
static AstFunctionDeclaration function_of(const char* identifier, AstBlock body) {
    return ast_function_declaration(strdup(identifier), (AstParamList){0},
                                    SOME(OptionalBlock, body));
}

// A function definition named `identifier` with the given parameter names and
// body. Copies each name so the caller keeps ownership of `names`.
static AstFunctionDeclaration function_with_params(const char* identifier,
                                                   const char** names, int num_names,
                                                   AstBlock body) {
    AstParamList params = {0};
    for (int i = 0; i < num_names; i++) {
        list_push(&params, strdup(names[i]));
    }
    return ast_function_declaration(strdup(identifier), params,
                                    SOME(OptionalBlock, body));
}

// Build an argument list from heap-allocated expressions. Mirrors the parser:
// each AstExp is copied into the list by value and its wrapper freed.
static AstArgList arg_list_of(AstExp** exps, int num_exps) {
    AstArgList args = {0};
    for (int i = 0; i < num_exps; i++) {
        list_push(&args, *exps[i]);
        free(exps[i]);
    }
    return args;
}

static AstBlockItem statement_item(AstStatement* stmt) {
    return (AstBlockItem){ .kind = AST_STATEMENT, .as.statement = stmt };
}

// Wrap a list of statements into a single-function ("main") program.
// Takes ownership of `stmts` (each statement is copied into the function).
static AstProgram program_of(AstStatement** stmts, int num_stmts) {
    AstBlock body = {0};
    for (int i = 0; i < num_stmts; i++) {
        ast_block_append(&body, statement_item(stmts[i]));
    }
    free(stmts);
    AstFunctionDeclaration* functions = malloc(sizeof(AstFunctionDeclaration));
    functions[0] = function_of("main", body);
    return ast_program_create(functions, 1);
}

// Wrap a single statement into a one-statement "main" program.
static AstProgram program_of_stmt(AstStatement* stmt) {
    AstStatement** body = malloc(sizeof(AstStatement*));
    body[0] = stmt;
    return program_of(body, 1);
}

// A temp's or label's name is allocated once and shallow-copied into later
// uses (jump targets, copy dsts, return values), so collect the unique
// pointers first to free each allocation exactly once.
static void free_ir_program(IrProgram* program) {
    for (int f = 0; f < program->count; f++) {
        IrFunction* fn = &program->items[f];
        char** owned = malloc(fn->instructions.count * sizeof(char*));
        int num_owned = 0;
        for (int i = 0; i < fn->instructions.count; i++) {
            IrInstruction* ins = &fn->instructions.items[i];
            char* name = NULL;
            switch (ins->kind) {
                case IR_UNOP:
                    if (ins->as.unary.dst.kind == IR_VARIABLE) name = ins->as.unary.dst.as.identifier;
                    break;
                case IR_BINOP:
                    if (ins->as.binop.dst.kind == IR_VARIABLE) name = ins->as.binop.dst.as.identifier;
                    break;
                case IR_COPY:
                    if (ins->as.copy.dst.kind == IR_VARIABLE) name = ins->as.copy.dst.as.identifier;
                    break;
                case IR_FUNCALL:
                    if (ins->as.funcall.dst.kind == IR_VARIABLE) name = ins->as.funcall.dst.as.identifier;
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
        // A call instruction separately owns its callee-name copy and the
        // backing array of its argument list.
        for (int i = 0; i < fn->instructions.count; i++) {
            IrInstruction* ins = &fn->instructions.items[i];
            if (ins->kind == IR_FUNCALL) {
                free(ins->as.funcall.identifier);
                list_free(&ins->as.funcall.args);
            }
        }
        list_free(&fn->instructions);
        for (size_t p = 0; p < fn->params.count; p++) {
            free(fn->params.items[p]);
        }
        list_free(&fn->params);
        free(fn->identifier);
    }
    list_free(program);
}

// jump_zero and jump_not_zero are distinct union members, so pick the right
// one based on the instruction type.
static IrVal jump_cond(const IrInstruction* ins) {
    return ins->kind == IR_JUMP_ZERO ? ins->as.jump_zero.cond : ins->as.jump_not_zero.cond;
}

static const char* jump_target(const IrInstruction* ins) {
    return ins->kind == IR_JUMP_ZERO ? ins->as.jump_zero.target : ins->as.jump_not_zero.target;
}

// --- tests ---

// int main() { return 2; }
void test_emit_return_constant() {
    AstProgram ast = program_of_stmt(ast_stmt_return(ast_exp_int(2)));
    IrProgram ir = emit_ir(&ast);

    assert(ir.count == 1);
    IrFunction* fn = &ir.items[0];
    assert(strcmp(fn->identifier, "main") == 0);
    assert(fn->instructions.count == 1);

    IrInstruction* ret = &fn->instructions.items[0];
    assert(ret->kind == IR_RETURN);
    assert(ret->as.ret.val.kind == IR_CONSTANT);
    assert(ret->as.ret.val.as.int_val == 2);

    free_ir_program(&ir);
    ast_program_destroy(&ast);
    printf("  PASS: test_emit_return_constant\n");
}

// The IR function name must be an independent copy of the AST name.
void test_emit_name_is_owned_copy() {
    AstProgram ast = program_of_stmt(ast_stmt_return(ast_exp_int(0)));
    IrProgram ir = emit_ir(&ast);

    assert(ir.items[0].identifier != ast.items[0].identifier);
    assert(strcmp(ir.items[0].identifier, "main") == 0);

    free_ir_program(&ir);
    ast_program_destroy(&ast);
    printf("  PASS: test_emit_name_is_owned_copy\n");
}

// int main() { return -5; }  ->  unary NEG into a temp, then return the temp.
void test_emit_return_negate() {
    AstProgram ast = program_of_stmt(
        ast_stmt_return(ast_exp_unary(UNOP_MINUS, ast_exp_int(5))));
    IrProgram ir = emit_ir(&ast);

    IrFunction* fn = &ir.items[0];
    assert(fn->instructions.count == 2);

    IrInstruction* unary = &fn->instructions.items[0];
    assert(unary->kind == IR_UNOP);
    assert(unary->as.unary.op == IR_NEG);
    assert(unary->as.unary.src.kind == IR_CONSTANT);
    assert(unary->as.unary.src.as.int_val == 5);
    assert(unary->as.unary.dst.kind == IR_VARIABLE);

    IrInstruction* ret = &fn->instructions.items[1];
    assert(ret->kind == IR_RETURN);
    assert(ret->as.ret.val.kind == IR_VARIABLE);
    // The return value is exactly the temp produced by the unary op.
    assert(strcmp(ret->as.ret.val.as.identifier, unary->as.unary.dst.as.identifier) == 0);

    free_ir_program(&ir);
    ast_program_destroy(&ast);
    printf("  PASS: test_emit_return_negate\n");
}

// int main() { return ~3; }  ->  NOT maps to the complement op.
void test_emit_return_complement() {
    AstProgram ast = program_of_stmt(
        ast_stmt_return(ast_exp_unary(UNOP_COMP, ast_exp_int(3))));
    IrProgram ir = emit_ir(&ast);

    IrFunction* fn = &ir.items[0];
    assert(fn->instructions.count == 2);
    assert(fn->instructions.items[0].kind == IR_UNOP);
    assert(fn->instructions.items[0].as.unary.op == IR_COMP);
    assert(fn->instructions.items[0].as.unary.src.as.int_val == 3);

    free_ir_program(&ir);
    ast_program_destroy(&ast);
    printf("  PASS: test_emit_return_complement\n");
}

// int main() { return -(~5); }  ->  two temps chained, then return the outer.
void test_emit_nested_unary() {
    AstExp* inner = ast_exp_unary(UNOP_COMP, ast_exp_int(5));
    AstExp* outer = ast_exp_unary(UNOP_MINUS, inner);
    AstProgram ast = program_of_stmt(ast_stmt_return(outer));
    IrProgram ir = emit_ir(&ast);

    IrFunction* fn = &ir.items[0];
    assert(fn->instructions.count == 3);

    IrInstruction* in0 = &fn->instructions.items[0];  // ~5 -> t0
    IrInstruction* in1 = &fn->instructions.items[1];  // -t0 -> t1
    IrInstruction* in2 = &fn->instructions.items[2];  // return t1

    assert(in0->kind == IR_UNOP && in0->as.unary.op == IR_COMP);
    assert(in0->as.unary.src.kind == IR_CONSTANT && in0->as.unary.src.as.int_val == 5);

    assert(in1->kind == IR_UNOP && in1->as.unary.op == IR_NEG);
    // The outer op consumes the inner op's result.
    assert(in1->as.unary.src.kind == IR_VARIABLE);
    assert(strcmp(in1->as.unary.src.as.identifier, in0->as.unary.dst.as.identifier) == 0);

    assert(in2->kind == IR_RETURN);
    assert(strcmp(in2->as.ret.val.as.identifier, in1->as.unary.dst.as.identifier) == 0);

    free_ir_program(&ir);
    ast_program_destroy(&ast);
    printf("  PASS: test_emit_nested_unary\n");
}

// A bare-constant expression statement produces no instruction.
// int main() { 5; return 0; }  ->  only the return is emitted.
void test_emit_expr_statement_no_instruction() {
    AstStatement** body = malloc(2 * sizeof(AstStatement*));
    body[0] = ast_stmt_exp(ast_exp_int(5));
    body[1] = ast_stmt_return(ast_exp_int(0));
    AstProgram ast = program_of(body, 2);
    IrProgram ir = emit_ir(&ast);

    IrFunction* fn = &ir.items[0];
    assert(fn->instructions.count == 1);
    assert(fn->instructions.items[0].kind == IR_RETURN);

    free_ir_program(&ir);
    ast_program_destroy(&ast);
    printf("  PASS: test_emit_expr_statement_no_instruction\n");
}

// A program with more than one function lowers each independently.
void test_emit_multiple_functions() {
    AstBlock foo_body = {0};
    ast_block_append(&foo_body, statement_item(ast_stmt_return(ast_exp_int(1))));
    AstBlock bar_body = {0};
    ast_block_append(&bar_body, statement_item(ast_stmt_return(ast_exp_int(2))));

    AstFunctionDeclaration* functions = malloc(2 * sizeof(AstFunctionDeclaration));
    functions[0] = function_of("foo", foo_body);
    functions[1] = function_of("bar", bar_body);
    AstProgram ast = ast_program_create(functions, 2);
    IrProgram ir = emit_ir(&ast);

    assert(ir.count == 2);
    assert(strcmp(ir.items[0].identifier, "foo") == 0);
    assert(strcmp(ir.items[1].identifier, "bar") == 0);
    assert(ir.items[0].instructions.items[0].as.ret.val.as.int_val == 1);
    assert(ir.items[1].instructions.items[0].as.ret.val.as.int_val == 2);

    free_ir_program(&ir);
    ast_program_destroy(&ast);
    printf("  PASS: test_emit_multiple_functions\n");
}

// int main() { return 1 + 2; }
//   t0 = add 1, 2
//   return t0
void test_emit_binop_add() {
    AstProgram ast = program_of_stmt(ast_stmt_return(
        ast_exp_binop(BINOP_ADD, ast_exp_int(1), ast_exp_int(2))));
    IrProgram ir = emit_ir(&ast);

    IrFunction* fn = &ir.items[0];
    assert(fn->instructions.count == 2);

    IrInstruction* binop = &fn->instructions.items[0];
    assert(binop->kind == IR_BINOP);
    assert(binop->as.binop.op == IR_ADD);
    assert(binop->as.binop.lhs.kind == IR_CONSTANT);
    assert(binop->as.binop.lhs.as.int_val == 1);
    assert(binop->as.binop.rhs.kind == IR_CONSTANT);
    assert(binop->as.binop.rhs.as.int_val == 2);
    assert(binop->as.binop.dst.kind == IR_VARIABLE);

    IrInstruction* ret = &fn->instructions.items[1];
    assert(ret->kind == IR_RETURN);
    assert(ret->as.ret.val.kind == IR_VARIABLE);
    // The return value is exactly the temp produced by the binop.
    assert(strcmp(ret->as.ret.val.as.identifier, binop->as.binop.dst.as.identifier) == 0);

    free_ir_program(&ir);
    ast_program_destroy(&ast);
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
        AstProgram ast = program_of_stmt(ast_stmt_return(
            ast_exp_binop(cases[c].ast_op,
                             ast_exp_int(7), ast_exp_int(3))));
        IrProgram ir = emit_ir(&ast);

        IrInstruction* binop = &ir.items[0].instructions.items[0];
        assert(binop->kind == IR_BINOP);
        assert(binop->as.binop.op == cases[c].ir_op);
        assert(binop->as.binop.lhs.as.int_val == 7);
        assert(binop->as.binop.rhs.as.int_val == 3);

        free_ir_program(&ir);
        ast_program_destroy(&ast);
    }
    printf("  PASS: test_emit_binop_all_ops\n");
}

// int main() { return (1 + 2) * 3; }
// LHS is fully evaluated before RHS, so the inner add is emitted first and the
// outer multiply consumes that temp as its left operand.
void test_emit_binop_nested_lhs_first() {
    AstExp* inner = ast_exp_binop(BINOP_ADD,
                                     ast_exp_int(1), ast_exp_int(2));
    AstExp* outer = ast_exp_binop(BINOP_MUL, inner, ast_exp_int(3));
    AstProgram ast = program_of_stmt(ast_stmt_return(outer));
    IrProgram ir = emit_ir(&ast);

    IrFunction* fn = &ir.items[0];
    assert(fn->instructions.count == 3);

    IrInstruction* add = &fn->instructions.items[0];   // (1 + 2) -> t0
    IrInstruction* mul = &fn->instructions.items[1];   // t0 * 3  -> t1
    IrInstruction* ret = &fn->instructions.items[2];   // return t1

    assert(add->kind == IR_BINOP && add->as.binop.op == IR_ADD);
    assert(add->as.binop.lhs.as.int_val == 1 && add->as.binop.rhs.as.int_val == 2);

    assert(mul->kind == IR_BINOP && mul->as.binop.op == IR_MUL);
    // The multiply's left operand is the add's result temp; its right is 3.
    assert(mul->as.binop.lhs.kind == IR_VARIABLE);
    assert(strcmp(mul->as.binop.lhs.as.identifier, add->as.binop.dst.as.identifier) == 0);
    assert(mul->as.binop.rhs.kind == IR_CONSTANT && mul->as.binop.rhs.as.int_val == 3);

    assert(ret->kind == IR_RETURN);
    assert(strcmp(ret->as.ret.val.as.identifier, mul->as.binop.dst.as.identifier) == 0);

    free_ir_program(&ir);
    ast_program_destroy(&ast);
    printf("  PASS: test_emit_binop_nested_lhs_first\n");
}

// int main() { return 1 + (2 * 3); }
// The RHS sub-expression is lowered first (it is the deeper operand evaluated
// when we recurse into the right child), and the outer add keeps 1 as its
// constant left operand and the multiply temp as its right operand.
void test_emit_binop_rhs_subexpression() {
    AstExp* inner = ast_exp_binop(BINOP_MUL,
                                     ast_exp_int(2), ast_exp_int(3));
    AstExp* outer = ast_exp_binop(BINOP_ADD, ast_exp_int(1), inner);
    AstProgram ast = program_of_stmt(ast_stmt_return(outer));
    IrProgram ir = emit_ir(&ast);

    IrFunction* fn = &ir.items[0];
    assert(fn->instructions.count == 3);

    IrInstruction* mul = &fn->instructions.items[0];   // (2 * 3) -> t0
    IrInstruction* add = &fn->instructions.items[1];   // 1 + t0  -> t1

    assert(mul->kind == IR_BINOP && mul->as.binop.op == IR_MUL);
    assert(add->kind == IR_BINOP && add->as.binop.op == IR_ADD);
    assert(add->as.binop.lhs.kind == IR_CONSTANT && add->as.binop.lhs.as.int_val == 1);
    assert(add->as.binop.rhs.kind == IR_VARIABLE);
    assert(strcmp(add->as.binop.rhs.as.identifier, mul->as.binop.dst.as.identifier) == 0);

    free_ir_program(&ir);
    ast_program_destroy(&ast);
    printf("  PASS: test_emit_binop_rhs_subexpression\n");
}

// A binop's result temp must be a distinct name from any nested temp.
void test_emit_binop_distinct_temps() {
    AstExp* inner = ast_exp_binop(BINOP_SUB,
                                     ast_exp_int(9), ast_exp_int(4));
    AstExp* outer = ast_exp_binop(BINOP_DIV, inner, ast_exp_int(2));
    AstProgram ast = program_of_stmt(ast_stmt_return(outer));
    IrProgram ir = emit_ir(&ast);

    IrFunction* fn = &ir.items[0];
    IrInstruction* sub = &fn->instructions.items[0];
    IrInstruction* div = &fn->instructions.items[1];
    assert(strcmp(sub->as.binop.dst.as.identifier, div->as.binop.dst.as.identifier) != 0);

    free_ir_program(&ir);
    ast_program_destroy(&ast);
    printf("  PASS: test_emit_binop_distinct_temps\n");
}

// int main() { return (5 & 3) | (1 << 4); }
// Both bitwise sub-expressions are lowered into temps before the outer OR,
// which consumes the two temps as its operands.
void test_emit_binop_bitwise_nested() {
    AstExp* and_exp = ast_exp_binop(BINOP_AND,
                                       ast_exp_int(5), ast_exp_int(3));
    AstExp* shl_exp = ast_exp_binop(BINOP_LSHIFT,
                                       ast_exp_int(1), ast_exp_int(4));
    AstExp* or_exp = ast_exp_binop(BINOP_OR, and_exp, shl_exp);
    AstProgram ast = program_of_stmt(ast_stmt_return(or_exp));
    IrProgram ir = emit_ir(&ast);

    IrFunction* fn = &ir.items[0];
    assert(fn->instructions.count == 4);

    IrInstruction* and_ins = &fn->instructions.items[0];  // 5 & 3   -> t0
    IrInstruction* shl_ins = &fn->instructions.items[1];  // 1 << 4  -> t1
    IrInstruction* or_ins  = &fn->instructions.items[2];  // t0 | t1 -> t2
    IrInstruction* ret     = &fn->instructions.items[3];  // return t2

    assert(and_ins->kind == IR_BINOP && and_ins->as.binop.op == IR_AND);
    assert(and_ins->as.binop.lhs.as.int_val == 5 && and_ins->as.binop.rhs.as.int_val == 3);

    assert(shl_ins->kind == IR_BINOP && shl_ins->as.binop.op == IR_LSHIFT);
    assert(shl_ins->as.binop.lhs.as.int_val == 1 && shl_ins->as.binop.rhs.as.int_val == 4);

    assert(or_ins->kind == IR_BINOP && or_ins->as.binop.op == IR_OR);
    assert(or_ins->as.binop.lhs.kind == IR_VARIABLE);
    assert(strcmp(or_ins->as.binop.lhs.as.identifier, and_ins->as.binop.dst.as.identifier) == 0);
    assert(or_ins->as.binop.rhs.kind == IR_VARIABLE);
    assert(strcmp(or_ins->as.binop.rhs.as.identifier, shl_ins->as.binop.dst.as.identifier) == 0);

    assert(ret->kind == IR_RETURN);
    assert(strcmp(ret->as.ret.val.as.identifier, or_ins->as.binop.dst.as.identifier) == 0);

    free_ir_program(&ir);
    ast_program_destroy(&ast);
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
static void check_short_circuit(AstBinopType op, IrInstructionKind cond_jump_type,
                                int short_circuit_value) {
    AstProgram ast = program_of_stmt(ast_stmt_return(
        ast_exp_binop(op, ast_exp_int(1), ast_exp_int(2))));
    IrProgram ir = emit_ir(&ast);

    IrFunction* fn = &ir.items[0];
    assert(fn->instructions.count == 8);

    IrInstruction* jump_lhs    = &fn->instructions.items[0];
    IrInstruction* jump_rhs    = &fn->instructions.items[1];
    IrInstruction* store_fall  = &fn->instructions.items[2];
    IrInstruction* jump_end    = &fn->instructions.items[3];
    IrInstruction* short_label = &fn->instructions.items[4];
    IrInstruction* store_short = &fn->instructions.items[5];
    IrInstruction* end_label   = &fn->instructions.items[6];
    IrInstruction* ret         = &fn->instructions.items[7];

    assert(short_label->kind == IR_LABEL);
    assert(end_label->kind == IR_LABEL);
    assert(strcmp(short_label->as.label.identifier, end_label->as.label.identifier) != 0);

    // both operands take the same conditional jump to the short-circuit label
    assert(jump_lhs->kind == cond_jump_type);
    assert(jump_cond(jump_lhs).kind == IR_CONSTANT);
    assert(jump_cond(jump_lhs).as.int_val == 1);
    assert(strcmp(jump_target(jump_lhs), short_label->as.label.identifier) == 0);

    assert(jump_rhs->kind == cond_jump_type);
    assert(jump_cond(jump_rhs).kind == IR_CONSTANT);
    assert(jump_cond(jump_rhs).as.int_val == 2);
    assert(strcmp(jump_target(jump_rhs), short_label->as.label.identifier) == 0);

    // fall-through stores the opposite of the short-circuit value, then skips
    // past the short-circuit store
    assert(store_fall->kind == IR_COPY);
    assert(store_fall->as.copy.src.kind == IR_CONSTANT);
    assert(store_fall->as.copy.src.as.int_val == !short_circuit_value);
    assert(store_fall->as.copy.dst.kind == IR_VARIABLE);

    assert(jump_end->kind == IR_JUMP);
    assert(strcmp(jump_end->as.jump.target, end_label->as.label.identifier) == 0);

    // short-circuit stores its value into the same destination temp
    assert(store_short->kind == IR_COPY);
    assert(store_short->as.copy.src.kind == IR_CONSTANT);
    assert(store_short->as.copy.src.as.int_val == short_circuit_value);
    assert(strcmp(store_short->as.copy.dst.as.identifier, store_fall->as.copy.dst.as.identifier) == 0);

    assert(ret->kind == IR_RETURN);
    assert(ret->as.ret.val.kind == IR_VARIABLE);
    assert(strcmp(ret->as.ret.val.as.identifier, store_fall->as.copy.dst.as.identifier) == 0);

    free_ir_program(&ir);
    ast_program_destroy(&ast);
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
        AstProgram ast = program_of_stmt(ast_stmt_return(
            ast_exp_binop(cases[c].ast_op,
                             ast_exp_int(4), ast_exp_int(5))));
        IrProgram ir = emit_ir(&ast);

        IrFunction* fn = &ir.items[0];
        assert(fn->instructions.count == 2);
        IrInstruction* binop = &fn->instructions.items[0];
        assert(binop->kind == IR_BINOP);
        assert(binop->as.binop.op == cases[c].ir_op);
        assert(binop->as.binop.lhs.as.int_val == 4);
        assert(binop->as.binop.rhs.as.int_val == 5);

        IrInstruction* ret = &fn->instructions.items[1];
        assert(ret->kind == IR_RETURN);
        assert(strcmp(ret->as.ret.val.as.identifier, binop->as.binop.dst.as.identifier) == 0);

        free_ir_program(&ir);
        ast_program_destroy(&ast);
    }
    printf("  PASS: test_emit_relational_ops\n");
}

// int main() { return !5; }  ->  logical NOT lowers to the IR_NOT unary op.
void test_emit_logical_not() {
    AstProgram ast = program_of_stmt(
        ast_stmt_return(ast_exp_unary(UNOP_NOT, ast_exp_int(5))));
    IrProgram ir = emit_ir(&ast);

    IrFunction* fn = &ir.items[0];
    assert(fn->instructions.count == 2);
    assert(fn->instructions.items[0].kind == IR_UNOP);
    assert(fn->instructions.items[0].as.unary.op == IR_NOT);
    assert(fn->instructions.items[0].as.unary.src.kind == IR_CONSTANT);
    assert(fn->instructions.items[0].as.unary.src.as.int_val == 5);

    free_ir_program(&ir);
    ast_program_destroy(&ast);
    printf("  PASS: test_emit_logical_not\n");
}

// Nested short-circuit operators each allocate their own pair of labels, so a
// program with two && expressions must emit four distinct labels.
void test_emit_nested_short_circuit_unique_labels() {
    // (1 && 2) && 3
    AstExp* inner = ast_exp_binop(BINOP_LAND,
                                     ast_exp_int(1), ast_exp_int(2));
    AstExp* outer = ast_exp_binop(BINOP_LAND, inner, ast_exp_int(3));
    AstProgram ast = program_of_stmt(ast_stmt_return(outer));
    IrProgram ir = emit_ir(&ast);

    IrFunction* fn = &ir.items[0];
    const char* labels[8];
    int num_labels = 0;
    for (int i = 0; i < fn->instructions.count; i++) {
        if (fn->instructions.items[i].kind == IR_LABEL) {
            labels[num_labels++] = fn->instructions.items[i].as.label.identifier;
        }
    }
    assert(num_labels == 4);
    for (int a = 0; a < num_labels; a++) {
        for (int b = a + 1; b < num_labels; b++) {
            assert(strcmp(labels[a], labels[b]) != 0);
        }
    }

    free_ir_program(&ir);
    ast_program_destroy(&ast);
    printf("  PASS: test_emit_nested_short_circuit_unique_labels\n");
}

// int main() { x = 5; }  ->  a plain assignment lowers to a single COPY of the
// rhs into the variable (no arithmetic).
void test_emit_assign_plain() {
    AstProgram ast = program_of_stmt(ast_stmt_exp(
        ast_exp_assign(ASSIGN_NOP, ast_exp_var("x"), ast_exp_int(5))));
    IrProgram ir = emit_ir(&ast);

    IrFunction* fn = &ir.items[0];
    assert(fn->instructions.count == 1);
    IrInstruction* copy = &fn->instructions.items[0];
    assert(copy->kind == IR_COPY);
    assert(copy->as.copy.src.kind == IR_CONSTANT && copy->as.copy.src.as.int_val == 5);
    assert(copy->as.copy.dst.kind == IR_VARIABLE);
    assert(strcmp(copy->as.copy.dst.as.identifier, "x") == 0);

    free_ir_program(&ir);
    ast_program_destroy(&ast);
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
        AstProgram ast = program_of_stmt(ast_stmt_exp(
            ast_exp_assign(cases[c].assign_op,
                              ast_exp_var("x"), ast_exp_int(5))));
        IrProgram ir = emit_ir(&ast);

        IrFunction* fn = &ir.items[0];
        assert(fn->instructions.count == 1);
        IrInstruction* binop = &fn->instructions.items[0];
        if (binop->kind != IR_BINOP || binop->as.binop.op != cases[c].ir_op) {
            printf("  FAIL: compound assign %s lowered to wrong op "
                   "(expected IR op %d, got type %d op %d)\n",
                   cases[c].name, cases[c].ir_op, binop->kind, binop->as.binop.op);
            exit(1);
        }
        // left operand and destination are both the variable; rhs is the constant
        assert(binop->as.binop.lhs.kind == IR_VARIABLE);
        assert(strcmp(binop->as.binop.lhs.as.identifier, "x") == 0);
        assert(binop->as.binop.dst.kind == IR_VARIABLE);
        assert(strcmp(binop->as.binop.dst.as.identifier, "x") == 0);
        assert(binop->as.binop.rhs.kind == IR_CONSTANT && binop->as.binop.rhs.as.int_val == 5);

        free_ir_program(&ir);
        ast_program_destroy(&ast);
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
    AstProgram ast = program_of_stmt(ast_stmt_return(
        ast_exp_unary(op, ast_exp_var("x"))));
    IrProgram ir = emit_ir(&ast);

    IrFunction* fn = &ir.items[0];
    assert(fn->instructions.count == 2);

    IrInstruction* binop = &fn->instructions.items[0];
    assert(binop->kind == IR_BINOP);
    assert(binop->as.binop.op == ir_op);
    assert(binop->as.binop.lhs.kind == IR_VARIABLE && strcmp(binop->as.binop.lhs.as.identifier, "x") == 0);
    assert(binop->as.binop.rhs.kind == IR_CONSTANT && binop->as.binop.rhs.as.int_val == 1);
    assert(binop->as.binop.dst.kind == IR_VARIABLE && strcmp(binop->as.binop.dst.as.identifier, "x") == 0);

    IrInstruction* ret = &fn->instructions.items[1];
    assert(ret->kind == IR_RETURN);
    assert(ret->as.ret.val.kind == IR_VARIABLE && strcmp(ret->as.ret.val.as.identifier, "x") == 0);

    free_ir_program(&ir);
    ast_program_destroy(&ast);
    printf("  PASS: %s\n", desc);
}

// Postfix `x++` / `x--` saves the old value into a fresh temp, mutates the
// variable, and yields the saved temp (not the mutated variable):
//   tmp = x
//   x = x <+/-> 1
//   return tmp
static void check_postfix(const char* desc, AstUnopType op, IrBinopType ir_op) {
    AstProgram ast = program_of_stmt(ast_stmt_return(
        ast_exp_unary(op, ast_exp_var("x"))));
    IrProgram ir = emit_ir(&ast);

    IrFunction* fn = &ir.items[0];
    assert(fn->instructions.count == 3);

    IrInstruction* copy = &fn->instructions.items[0];
    assert(copy->kind == IR_COPY);
    assert(copy->as.copy.src.kind == IR_VARIABLE && strcmp(copy->as.copy.src.as.identifier, "x") == 0);
    assert(copy->as.copy.dst.kind == IR_VARIABLE);
    assert(strcmp(copy->as.copy.dst.as.identifier, "x") != 0);  // a fresh temp, not x

    IrInstruction* binop = &fn->instructions.items[1];
    assert(binop->kind == IR_BINOP);
    assert(binop->as.binop.op == ir_op);
    assert(binop->as.binop.lhs.kind == IR_VARIABLE && strcmp(binop->as.binop.lhs.as.identifier, "x") == 0);
    assert(binop->as.binop.rhs.kind == IR_CONSTANT && binop->as.binop.rhs.as.int_val == 1);
    assert(binop->as.binop.dst.kind == IR_VARIABLE && strcmp(binop->as.binop.dst.as.identifier, "x") == 0);

    IrInstruction* ret = &fn->instructions.items[2];
    assert(ret->kind == IR_RETURN);
    assert(ret->as.ret.val.kind == IR_VARIABLE);
    assert(strcmp(ret->as.ret.val.as.identifier, copy->as.copy.dst.as.identifier) == 0);  // the saved old value
    assert(strcmp(ret->as.ret.val.as.identifier, "x") != 0);

    free_ir_program(&ir);
    ast_program_destroy(&ast);
    printf("  PASS: %s\n", desc);
}

void test_emit_prefix_increment()  { check_prefix("return ++x", UNOP_PREINC, IR_ADD); }
void test_emit_prefix_decrement()  { check_prefix("return --x", UNOP_PREDEC, IR_SUB); }
void test_emit_postfix_increment() { check_postfix("return x++", UNOP_POSTINC, IR_ADD); }
void test_emit_postfix_decrement() { check_postfix("return x--", UNOP_POSTDEC, IR_SUB); }

// --- conditional expression (ternary) lowering ---

// make_*_stmt already heap-allocates, so a branch pointer can own the result
// directly; this pass-through is kept for readability at the call sites.
static AstStatement* heap_stmt(AstStatement* stmt) {
    return stmt;
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
    AstProgram ast = program_of_stmt(ast_stmt_return(
        ast_exp_conditional(ast_exp_int(1), ast_exp_int(2), ast_exp_int(3))));
    IrProgram ir = emit_ir(&ast);

    IrFunction* fn = &ir.items[0];
    assert(fn->instructions.count == 7);

    IrInstruction* jz         = &fn->instructions.items[0];
    IrInstruction* copy_true  = &fn->instructions.items[1];
    IrInstruction* jmp_end    = &fn->instructions.items[2];
    IrInstruction* false_lbl  = &fn->instructions.items[3];
    IrInstruction* copy_false = &fn->instructions.items[4];
    IrInstruction* end_lbl    = &fn->instructions.items[5];
    IrInstruction* ret        = &fn->instructions.items[6];

    // condition (constant 1) jumps to the false branch when zero
    assert(jz->kind == IR_JUMP_ZERO);
    assert(jump_cond(jz).kind == IR_CONSTANT && jump_cond(jz).as.int_val == 1);
    assert(strcmp(jump_target(jz), false_lbl->as.label.identifier) == 0);

    // true branch copies 2 into the shared result temp, then skips the false arm
    assert(copy_true->kind == IR_COPY);
    assert(copy_true->as.copy.src.kind == IR_CONSTANT && copy_true->as.copy.src.as.int_val == 2);
    assert(copy_true->as.copy.dst.kind == IR_VARIABLE);
    assert(jmp_end->kind == IR_JUMP);
    assert(strcmp(jmp_end->as.jump.target, end_lbl->as.label.identifier) == 0);

    // false branch copies 3 into the *same* result temp
    assert(false_lbl->kind == IR_LABEL);
    assert(copy_false->kind == IR_COPY);
    assert(copy_false->as.copy.src.kind == IR_CONSTANT && copy_false->as.copy.src.as.int_val == 3);
    assert(strcmp(copy_false->as.copy.dst.as.identifier, copy_true->as.copy.dst.as.identifier) == 0);

    // the two labels are distinct, and the result temp is what's returned
    assert(end_lbl->kind == IR_LABEL);
    assert(strcmp(false_lbl->as.label.identifier, end_lbl->as.label.identifier) != 0);
    assert(ret->kind == IR_RETURN);
    assert(strcmp(ret->as.ret.val.as.identifier, copy_true->as.copy.dst.as.identifier) == 0);

    free_ir_program(&ir);
    ast_program_destroy(&ast);
    printf("  PASS: test_emit_conditional_expression\n");
}

// Each ternary allocates its own label pair, so nesting one in the true arm
// yields four distinct labels.
void test_emit_conditional_nested_unique_labels() {
    // 1 ? (2 ? 3 : 4) : 5
    AstExp* inner = ast_exp_conditional(
        ast_exp_int(2), ast_exp_int(3), ast_exp_int(4));
    AstExp* outer = ast_exp_conditional(ast_exp_int(1), inner, ast_exp_int(5));
    AstProgram ast = program_of_stmt(ast_stmt_return(outer));
    IrProgram ir = emit_ir(&ast);

    IrFunction* fn = &ir.items[0];
    const char* labels[8];
    int num_labels = 0;
    for (int i = 0; i < fn->instructions.count; i++) {
        if (fn->instructions.items[i].kind == IR_LABEL) {
            labels[num_labels++] = fn->instructions.items[i].as.label.identifier;
        }
    }
    assert(num_labels == 4);
    for (int a = 0; a < num_labels; a++) {
        for (int b = a + 1; b < num_labels; b++) {
            assert(strcmp(labels[a], labels[b]) != 0);
        }
    }

    free_ir_program(&ir);
    ast_program_destroy(&ast);
    printf("  PASS: test_emit_conditional_nested_unique_labels\n");
}

// --- if-statement lowering ---

// int main() { if (1) return 2; }  with no else lowers to a single forward
// branch — no else label and no unconditional jump:
//   jump_zero(1, end)
//   return 2
// end:
void test_emit_if_no_else() {
    AstProgram ast = program_of_stmt(ast_stmt_if(
        ast_exp_int(1),
        heap_stmt(ast_stmt_return(ast_exp_int(2))),
        NULL));
    IrProgram ir = emit_ir(&ast);

    IrFunction* fn = &ir.items[0];
    assert(fn->instructions.count == 3);

    IrInstruction* jz      = &fn->instructions.items[0];
    IrInstruction* ret     = &fn->instructions.items[1];
    IrInstruction* end_lbl = &fn->instructions.items[2];

    assert(jz->kind == IR_JUMP_ZERO);
    assert(jump_cond(jz).kind == IR_CONSTANT && jump_cond(jz).as.int_val == 1);
    // the false path jumps straight to the end label
    assert(strcmp(jump_target(jz), end_lbl->as.label.identifier) == 0);

    assert(ret->kind == IR_RETURN);
    assert(ret->as.ret.val.kind == IR_CONSTANT && ret->as.ret.val.as.int_val == 2);

    assert(end_lbl->kind == IR_LABEL);

    // no unconditional jump and no second label are emitted when there is no else
    for (int i = 0; i < fn->instructions.count; i++) {
        assert(fn->instructions.items[i].kind != IR_JUMP);
    }

    free_ir_program(&ir);
    ast_program_destroy(&ast);
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
    AstProgram ast = program_of_stmt(ast_stmt_if(
        ast_exp_int(1),
        heap_stmt(ast_stmt_return(ast_exp_int(2))),
        heap_stmt(ast_stmt_return(ast_exp_int(3)))));
    IrProgram ir = emit_ir(&ast);

    IrFunction* fn = &ir.items[0];
    assert(fn->instructions.count == 6);

    IrInstruction* jz       = &fn->instructions.items[0];
    IrInstruction* then_ret = &fn->instructions.items[1];
    IrInstruction* jmp_end  = &fn->instructions.items[2];
    IrInstruction* else_lbl = &fn->instructions.items[3];
    IrInstruction* else_ret = &fn->instructions.items[4];
    IrInstruction* end_lbl  = &fn->instructions.items[5];

    // condition jumps to the else label when zero
    assert(jz->kind == IR_JUMP_ZERO);
    assert(strcmp(jump_target(jz), else_lbl->as.label.identifier) == 0);

    // then branch returns 2, then skips past the else branch
    assert(then_ret->kind == IR_RETURN && then_ret->as.ret.val.as.int_val == 2);
    assert(jmp_end->kind == IR_JUMP);
    assert(strcmp(jmp_end->as.jump.target, end_lbl->as.label.identifier) == 0);

    // else branch returns 3
    assert(else_lbl->kind == IR_LABEL);
    assert(else_ret->kind == IR_RETURN && else_ret->as.ret.val.as.int_val == 3);

    // the else and end labels are distinct
    assert(end_lbl->kind == IR_LABEL);
    assert(strcmp(else_lbl->as.label.identifier, end_lbl->as.label.identifier) != 0);

    free_ir_program(&ir);
    ast_program_destroy(&ast);
    printf("  PASS: test_emit_if_with_else\n");
}

// The condition is a real expression: `if (a < b)` lowers the comparison into a
// temp first, and that temp (not a constant) is what the jump tests.
void test_emit_if_cond_is_expression() {
    AstProgram ast = program_of_stmt(ast_stmt_if(
        ast_exp_binop(BINOP_LESS, ast_exp_var("a"), ast_exp_var("b")),
        heap_stmt(ast_stmt_return(ast_exp_int(1))),
        NULL));
    IrProgram ir = emit_ir(&ast);

    IrFunction* fn = &ir.items[0];

    // the relational compare is lowered before the branch
    IrInstruction* cmp = &fn->instructions.items[0];
    assert(cmp->kind == IR_BINOP && cmp->as.binop.op == IR_LESS);

    IrInstruction* jz = &fn->instructions.items[1];
    assert(jz->kind == IR_JUMP_ZERO);
    // the branch tests the compare's result temp, not a constant
    assert(jump_cond(jz).kind == IR_VARIABLE);
    assert(strcmp(jump_cond(jz).as.identifier, cmp->as.binop.dst.as.identifier) == 0);

    free_ir_program(&ir);
    ast_program_destroy(&ast);
    printf("  PASS: test_emit_if_cond_is_expression\n");
}

// A nested if (if inside the then branch) lowers recursively: the outer branch
// guards the inner branch, producing two distinct jump_zero/label pairs.
void test_emit_if_nested() {
    AstStatement* inner = heap_stmt(ast_stmt_if(
        ast_exp_int(1),
        heap_stmt(ast_stmt_return(ast_exp_int(2))),
        NULL));
    AstProgram ast = program_of_stmt(ast_stmt_if(ast_exp_int(3), inner, NULL));
    IrProgram ir = emit_ir(&ast);

    IrFunction* fn = &ir.items[0];
    int jump_zeros = 0, labels = 0;
    for (int i = 0; i < fn->instructions.count; i++) {
        if (fn->instructions.items[i].kind == IR_JUMP_ZERO) jump_zeros++;
        if (fn->instructions.items[i].kind == IR_LABEL) labels++;
    }
    // one branch + end label per if
    assert(jump_zeros == 2);
    assert(labels == 2);

    free_ir_program(&ir);
    ast_program_destroy(&ast);
    printf("  PASS: test_emit_if_nested\n");
}

// --- function call lowering ---

// int main() { foo(); }  ->  a call with no arguments into a fresh temp.
void test_emit_function_call_no_args() {
    AstProgram ast = program_of_stmt(ast_stmt_exp(
        ast_exp_function_call(strdup("foo"), (AstArgList){0})));
    IrProgram ir = emit_ir(&ast);

    IrFunction* fn = &ir.items[0];
    assert(fn->instructions.count == 1);

    IrInstruction* call = &fn->instructions.items[0];
    assert(call->kind == IR_FUNCALL);
    assert(strcmp(call->as.funcall.identifier, "foo") == 0);
    assert(call->as.funcall.args.count == 0);
    assert(call->as.funcall.dst.kind == IR_VARIABLE);

    free_ir_program(&ir);
    ast_program_destroy(&ast);
    printf("  PASS: test_emit_function_call_no_args\n");
}

// The IR callee name must be an independent copy of the AST name, just like
// the function-definition identifier.
void test_emit_function_call_identifier_is_owned_copy() {
    AstExp* ast_call = ast_exp_function_call(strdup("foo"), (AstArgList){0});
    char* ast_identifier = ast_call->as.funcall.identifier;
    AstProgram ast = program_of_stmt(ast_stmt_exp(ast_call));
    IrProgram ir = emit_ir(&ast);

    IrInstruction* call = &ir.items[0].instructions.items[0];
    assert(call->as.funcall.identifier != ast_identifier);
    assert(strcmp(call->as.funcall.identifier, "foo") == 0);

    free_ir_program(&ir);
    ast_program_destroy(&ast);
    printf("  PASS: test_emit_function_call_identifier_is_owned_copy\n");
}

// int main() { foo(1, 2); }  ->  constant arguments are carried through in
// order as the call's operands.
void test_emit_function_call_constant_args() {
    AstExp* exps[] = { ast_exp_int(1), ast_exp_int(2) };
    AstProgram ast = program_of_stmt(ast_stmt_exp(
        ast_exp_function_call(strdup("foo"), arg_list_of(exps, 2))));
    IrProgram ir = emit_ir(&ast);

    IrFunction* fn = &ir.items[0];
    assert(fn->instructions.count == 1);

    IrInstruction* call = &fn->instructions.items[0];
    assert(call->kind == IR_FUNCALL);
    assert(call->as.funcall.args.count == 2);
    assert(call->as.funcall.args.items[0].kind == IR_CONSTANT);
    assert(call->as.funcall.args.items[0].as.int_val == 1);
    assert(call->as.funcall.args.items[1].kind == IR_CONSTANT);
    assert(call->as.funcall.args.items[1].as.int_val == 2);

    free_ir_program(&ir);
    ast_program_destroy(&ast);
    printf("  PASS: test_emit_function_call_constant_args\n");
}

// int main() { foo(1 + 2); }  ->  a non-trivial argument is lowered into a temp
// first, and the call consumes that temp (not a constant) as its argument.
void test_emit_function_call_expression_arg() {
    AstExp* exps[] = { ast_exp_binop(BINOP_ADD, ast_exp_int(1), ast_exp_int(2)) };
    AstProgram ast = program_of_stmt(ast_stmt_exp(
        ast_exp_function_call(strdup("foo"), arg_list_of(exps, 1))));
    IrProgram ir = emit_ir(&ast);

    IrFunction* fn = &ir.items[0];
    assert(fn->instructions.count == 2);

    IrInstruction* add  = &fn->instructions.items[0];  // 1 + 2 -> t0
    IrInstruction* call = &fn->instructions.items[1];  // foo(t0)

    assert(add->kind == IR_BINOP && add->as.binop.op == IR_ADD);
    assert(call->kind == IR_FUNCALL);
    assert(call->as.funcall.args.count == 1);
    assert(call->as.funcall.args.items[0].kind == IR_VARIABLE);
    assert(strcmp(call->as.funcall.args.items[0].as.identifier,
                  add->as.binop.dst.as.identifier) == 0);

    free_ir_program(&ir);
    ast_program_destroy(&ast);
    printf("  PASS: test_emit_function_call_expression_arg\n");
}

// int main() { return foo(); }  ->  the call's result temp is what the enclosing
// expression (here, the return) uses.
void test_emit_function_call_result_used() {
    AstProgram ast = program_of_stmt(ast_stmt_return(
        ast_exp_function_call(strdup("foo"), (AstArgList){0})));
    IrProgram ir = emit_ir(&ast);

    IrFunction* fn = &ir.items[0];
    assert(fn->instructions.count == 2);

    IrInstruction* call = &fn->instructions.items[0];
    IrInstruction* ret  = &fn->instructions.items[1];
    assert(call->kind == IR_FUNCALL);
    assert(ret->kind == IR_RETURN);
    assert(ret->as.ret.val.kind == IR_VARIABLE);
    assert(strcmp(ret->as.ret.val.as.identifier,
                  call->as.funcall.dst.as.identifier) == 0);

    free_ir_program(&ir);
    ast_program_destroy(&ast);
    printf("  PASS: test_emit_function_call_result_used\n");
}

// Arguments are evaluated left to right, so nested calls are lowered in argument
// order before the outer call: bar() then baz(), then foo(t_bar, t_baz).
void test_emit_function_call_nested_arg_order() {
    AstExp* exps[] = {
        ast_exp_function_call(strdup("bar"), (AstArgList){0}),
        ast_exp_function_call(strdup("baz"), (AstArgList){0}),
    };
    AstProgram ast = program_of_stmt(ast_stmt_exp(
        ast_exp_function_call(strdup("foo"), arg_list_of(exps, 2))));
    IrProgram ir = emit_ir(&ast);

    IrFunction* fn = &ir.items[0];
    assert(fn->instructions.count == 3);

    IrInstruction* bar = &fn->instructions.items[0];
    IrInstruction* baz = &fn->instructions.items[1];
    IrInstruction* foo = &fn->instructions.items[2];

    assert(bar->kind == IR_FUNCALL && strcmp(bar->as.funcall.identifier, "bar") == 0);
    assert(baz->kind == IR_FUNCALL && strcmp(baz->as.funcall.identifier, "baz") == 0);
    assert(foo->kind == IR_FUNCALL && strcmp(foo->as.funcall.identifier, "foo") == 0);
    // foo's arguments are exactly the result temps of bar and baz, in order.
    assert(foo->as.funcall.args.count == 2);
    assert(strcmp(foo->as.funcall.args.items[0].as.identifier,
                  bar->as.funcall.dst.as.identifier) == 0);
    assert(strcmp(foo->as.funcall.args.items[1].as.identifier,
                  baz->as.funcall.dst.as.identifier) == 0);

    free_ir_program(&ir);
    ast_program_destroy(&ast);
    printf("  PASS: test_emit_function_call_nested_arg_order\n");
}

// --- function parameter lowering ---

// int foo(a, b) { return 0; }  ->  the IR function carries the parameter names
// in order.
void test_emit_function_params() {
    const char* names[] = { "a", "b" };
    AstBlock body = {0};
    ast_block_append(&body, statement_item(ast_stmt_return(ast_exp_int(0))));
    AstFunctionDeclaration* functions = malloc(sizeof(AstFunctionDeclaration));
    functions[0] = function_with_params("foo", names, 2, body);
    AstProgram ast = ast_program_create(functions, 1);
    IrProgram ir = emit_ir(&ast);

    IrFunction* fn = &ir.items[0];
    assert(fn->params.count == 2);
    assert(strcmp(fn->params.items[0], "a") == 0);
    assert(strcmp(fn->params.items[1], "b") == 0);

    free_ir_program(&ir);
    ast_program_destroy(&ast);
    printf("  PASS: test_emit_function_params\n");
}

// Each IR parameter name must be an independent copy of the AST parameter name.
void test_emit_function_params_are_owned_copies() {
    const char* names[] = { "a" };
    AstBlock body = {0};
    ast_block_append(&body, statement_item(ast_stmt_return(ast_exp_int(0))));
    AstFunctionDeclaration* functions = malloc(sizeof(AstFunctionDeclaration));
    functions[0] = function_with_params("foo", names, 1, body);
    AstProgram ast = ast_program_create(functions, 1);
    IrProgram ir = emit_ir(&ast);

    assert(ir.items[0].params.items[0] != ast.items[0].params.items[0]);
    assert(strcmp(ir.items[0].params.items[0], "a") == 0);

    free_ir_program(&ir);
    ast_program_destroy(&ast);
    printf("  PASS: test_emit_function_params_are_owned_copies\n");
}

// A function with no parameters lowers to an empty parameter list.
void test_emit_function_no_params() {
    AstProgram ast = program_of_stmt(ast_stmt_return(ast_exp_int(0)));
    IrProgram ir = emit_ir(&ast);

    assert(ir.items[0].params.count == 0);

    free_ir_program(&ir);
    ast_program_destroy(&ast);
    printf("  PASS: test_emit_function_no_params\n");
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
    test_emit_function_call_no_args();
    test_emit_function_call_identifier_is_owned_copy();
    test_emit_function_call_constant_args();
    test_emit_function_call_expression_arg();
    test_emit_function_call_result_used();
    test_emit_function_call_nested_arg_order();
    test_emit_function_params();
    test_emit_function_params_are_owned_copies();
    test_emit_function_no_params();
    printf("All IR tests passed!\n");
    return 0;
}
