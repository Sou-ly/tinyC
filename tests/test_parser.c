#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../src/parser/ast.h"
#include "../src/parser/parser.h"
#include "../src/list.h"

// --- AST unit tests ---

void test_create_int_exp() {
    AstExp* e = create_int_exp(42);
    assert(e != NULL);
    assert(e->kind == EXP_INT);
    assert(e->int_lit.value == 42);
    destroy_exp(e);
    printf("  PASS: test_create_int_exp\n");
}

void test_create_unary_exp() {
    AstExp* e = create_unary_exp(UNOP_MINUS, create_int_exp(5));
    assert(e != NULL);
    assert(e->kind == EXP_UNOP);
    assert(e->unary.op_type == UNOP_MINUS);
    assert(e->unary.operand->int_lit.value == 5);
    destroy_exp(e);
    printf("  PASS: test_create_unary_exp\n");
}

void test_create_binop_exp() {
    AstExp* e = create_binop_exp(BINOP_ADD, create_int_exp(3), create_int_exp(7));
    assert(e != NULL);
    assert(e->kind == EXP_BINOP);
    assert(e->binop.op_type == BINOP_ADD);
    assert(e->binop.lhs->int_lit.value == 3);
    assert(e->binop.rhs->int_lit.value == 7);
    destroy_exp(e);
    printf("  PASS: test_create_binop_exp\n");
}

void test_create_return_stmt() {
    AstStatement s = make_return_stmt(create_int_exp(2));
    assert(s.kind == STMT_RETURN);
    assert(s.ret.exp->int_lit.value == 2);
    destroy_stmt(&s);
    printf("  PASS: test_create_return_stmt\n");
}

void test_create_function_decl() {
    AstFunction fn = ast_function_make("main", 8);
    ast_function_append(&fn, (AstBlockItem){
        .type = AST_STATEMENT,
        .stmt = make_return_stmt(create_int_exp(0)),
    });

    assert(strcmp(fn.identifier, "main") == 0);
    assert(fn.size == 1);
    assert(fn.body[0].type == AST_STATEMENT);
    assert(fn.body[0].stmt.kind == STMT_RETURN);
    ast_function_destroy(&fn);
    printf("  PASS: test_create_function_decl\n");
}

// --- Parser integration test ---
// Parses: int main() { return 2; }

void test_parse_return_2() {
    TokenList tokens = token_list_create(8);

    token_list_push(&tokens, (Token){TOK_KEYWORD,   {.kw = TOK_INT},          1, 1});
    token_list_push(&tokens, (Token){TOK_IDENTIFIER, {.ident = strdup("main")}, 1, 5});
    token_list_push(&tokens, (Token){TOK_SEPARATOR, {.sep = TOK_LPAR},       1, 9});
    token_list_push(&tokens, (Token){TOK_SEPARATOR, {.sep = TOK_RPAR},       1, 10});
    token_list_push(&tokens, (Token){TOK_SEPARATOR, {.sep = TOK_LBRACE},     1, 12});
    token_list_push(&tokens, (Token){TOK_KEYWORD,   {.kw = TOK_RETURN},       2, 5});
    token_list_push(&tokens, (Token){TOK_INT_LITERAL, {.int_val = 2},        2, 12});
    token_list_push(&tokens, (Token){TOK_SEPARATOR, {.sep = TOK_SEMICOLON},  2, 13});
    token_list_push(&tokens, (Token){TOK_SEPARATOR, {.sep = TOK_RBRACE},     3, 1});

    Parser parser = parser_create(&tokens);
    AstProgram prog = parse_program(&parser);

    assert(prog.num_functions == 1);
    assert(strcmp(prog.functions[0].identifier, "main") == 0);
    assert(prog.functions[0].size == 1);
    assert(prog.functions[0].body[0].type == AST_STATEMENT);
    assert(prog.functions[0].body[0].stmt.kind == STMT_RETURN);
    assert(prog.functions[0].body[0].stmt.ret.exp->kind == EXP_INT);
    assert(prog.functions[0].body[0].stmt.ret.exp->int_lit.value == 2);

    destroy_program(&prog);
    free(tokens.items);
    printf("  PASS: test_parse_return_2\n");
}

// --- Parser precedence tests ---
//
// Each test wraps an expression token sequence in `int main() { return <exp>; }`,
// parses it, and compares the resulting expression tree against a hand-built
// expected tree.

#define COUNT_OF(array) (sizeof(array) / sizeof((array)[0]))

static Token make_int_token(int value) {
    return (Token){TOK_INT_LITERAL, {.int_val = value}, 1, 1};
}

static Token make_op_token(TokenOperator op) {
    return (Token){TOK_OPERATOR, {.op = op}, 1, 1};
}

static Token make_sep_token(TokenSeparator sep) {
    return (Token){TOK_SEPARATOR, {.sep = sep}, 1, 1};
}

static const char* binop_name(AstBinopType op) {
    switch (op) {
        case BINOP_ADD:    return "+";
        case BINOP_SUB:    return "-";
        case BINOP_MUL:    return "*";
        case BINOP_DIV:    return "/";
        case BINOP_MOD:    return "%";
        case BINOP_AND:    return "&";
        case BINOP_OR:     return "|";
        case BINOP_XOR:    return "^";
        case BINOP_LSHIFT: return "<<";
        case BINOP_RSHIFT: return ">>";
        case BINOP_LAND:   return "&&";
        case BINOP_LOR:    return "||";
        case BINOP_EQ:     return "==";
        case BINOP_NEQ:    return "!=";
        case BINOP_LESS:   return "<";
        case BINOP_GREATER:return ">";
        case BINOP_LEQ:    return "<=";
        case BINOP_GEQ:    return ">=";
        case BINOP_ASSIGN: return "=";
    }
    return "<unknown>";
}

static void print_exp(const AstExp* exp) {
    switch (exp->kind) {
        case EXP_INT:
            printf("%d", exp->int_lit.value);
            break;
        case EXP_UNOP:
            printf("(%s ", exp->unary.op_type == UNOP_COMP ? "!" : "-");
            print_exp(exp->unary.operand);
            printf(")");
            break;
        case EXP_BINOP:
            printf("(");
            print_exp(exp->binop.lhs);
            printf(" %s ", binop_name(exp->binop.op_type));
            print_exp(exp->binop.rhs);
            printf(")");
            break;
        case EXP_VAR:
            printf("%s", exp->variable.identifier);
            break;
        case EXP_ASSIGN:
            printf("(");
            print_exp(exp->assign.lhs);
            printf(" = ");
            print_exp(exp->assign.rhs);
            printf(")");
            break;
    }
}

static bool exp_equals(const AstExp* a, const AstExp* b) {
    if (a->kind != b->kind) {
        return false;
    }
    switch (a->kind) {
        case EXP_INT:
            return a->int_lit.value == b->int_lit.value;
        case EXP_UNOP:
            return a->unary.op_type == b->unary.op_type
                && exp_equals(a->unary.operand, b->unary.operand);
        case EXP_BINOP:
            return a->binop.op_type == b->binop.op_type
                && exp_equals(a->binop.lhs, b->binop.lhs)
                && exp_equals(a->binop.rhs, b->binop.rhs);
        case EXP_VAR:
            return strcmp(a->variable.identifier, b->variable.identifier) == 0;
        case EXP_ASSIGN:
            return exp_equals(a->assign.lhs, b->assign.lhs)
                && exp_equals(a->assign.rhs, b->assign.rhs);
    }
    return false;
}

// Parses `int main() { return <exp_tokens>; }` and checks that the returned
// expression matches `expected`. Takes ownership of `expected`.
static void check_return_exp(const char* description, const Token* exp_tokens,
                             size_t num_exp_tokens, AstExp* expected) {
    TokenList tokens = token_list_create(num_exp_tokens + 8);

    token_list_push(&tokens, (Token){TOK_KEYWORD,    {.kw = TOK_INT},            1, 1});
    token_list_push(&tokens, (Token){TOK_IDENTIFIER, {.ident = strdup("main")}, 1, 1});
    token_list_push(&tokens, make_sep_token(TOK_LPAR));
    token_list_push(&tokens, make_sep_token(TOK_RPAR));
    token_list_push(&tokens, make_sep_token(TOK_LBRACE));
    token_list_push(&tokens, (Token){TOK_KEYWORD,    {.kw = TOK_RETURN},         1, 1});
    for (size_t i = 0; i < num_exp_tokens; i++) {
        token_list_push(&tokens, exp_tokens[i]);
    }
    token_list_push(&tokens, make_sep_token(TOK_SEMICOLON));
    token_list_push(&tokens, make_sep_token(TOK_RBRACE));

    Parser parser = parser_create(&tokens);
    AstProgram prog = parse_program(&parser);

    AstExp* actual = prog.functions[0].body[0].stmt.ret.exp;
    if (!exp_equals(actual, expected)) {
        printf("  FAIL: %s\n    expected: ", description);
        print_exp(expected);
        printf("\n    actual:   ");
        print_exp(actual);
        printf("\n");
        exit(1);
    }

    destroy_exp(expected);
    destroy_program(&prog);
    token_list_destroy(&tokens);
    printf("  PASS: %s\n", description);
}

// '*' binds tighter than '+', on both sides.
void test_precedence_mul_over_add() {
    Token left[] = {
        make_int_token(1), make_op_token(TOK_PLUS),
        make_int_token(2), make_op_token(TOK_STAR), make_int_token(3),
    };
    check_return_exp("1 + 2 * 3;", left, COUNT_OF(left),
        create_binop_exp(BINOP_ADD, create_int_exp(1),
            create_binop_exp(BINOP_MUL, create_int_exp(2), create_int_exp(3))));

    Token right[] = {
        make_int_token(2), make_op_token(TOK_STAR), make_int_token(3),
        make_op_token(TOK_PLUS), make_int_token(4),
    };
    check_return_exp("2 * 3 + 4", right, COUNT_OF(right),
        create_binop_exp(BINOP_ADD,
            create_binop_exp(BINOP_MUL, create_int_exp(2), create_int_exp(3)),
            create_int_exp(4)));
}

// Same-precedence operators associate to the left.
void test_precedence_left_associativity() {
    Token sub[] = {
        make_int_token(10), make_op_token(TOK_MINUS),
        make_int_token(4), make_op_token(TOK_MINUS), make_int_token(3),
    };
    check_return_exp("10 - 4 - 3", sub, COUNT_OF(sub),
        create_binop_exp(BINOP_SUB,
            create_binop_exp(BINOP_SUB, create_int_exp(10), create_int_exp(4)),
            create_int_exp(3)));

    Token div_mod[] = {
        make_int_token(8), make_op_token(TOK_FSLASH),
        make_int_token(4), make_op_token(TOK_PERCENT), make_int_token(3),
    };
    check_return_exp("8 / 4 % 3", div_mod, COUNT_OF(div_mod),
        create_binop_exp(BINOP_MOD,
            create_binop_exp(BINOP_DIV, create_int_exp(8), create_int_exp(4)),
            create_int_exp(3)));

    Token shift[] = {
        make_int_token(1), make_op_token(TOK_LSHIFT),
        make_int_token(2), make_op_token(TOK_LSHIFT), make_int_token(3),
    };
    check_return_exp("1 << 2 << 3", shift, COUNT_OF(shift),
        create_binop_exp(BINOP_LSHIFT,
            create_binop_exp(BINOP_LSHIFT, create_int_exp(1), create_int_exp(2)),
            create_int_exp(3)));
}

// Additive binds tighter than shifts.
void test_precedence_add_over_shift() {
    Token lshift[] = {
        make_int_token(1), make_op_token(TOK_LSHIFT),
        make_int_token(2), make_op_token(TOK_PLUS), make_int_token(3),
    };
    check_return_exp("1 << 2 + 3", lshift, COUNT_OF(lshift),
        create_binop_exp(BINOP_LSHIFT, create_int_exp(1),
            create_binop_exp(BINOP_ADD, create_int_exp(2), create_int_exp(3))));

    Token rshift[] = {
        make_int_token(1), make_op_token(TOK_PLUS),
        make_int_token(2), make_op_token(TOK_RSHIFT), make_int_token(3),
    };
    check_return_exp("1 + 2 >> 3", rshift, COUNT_OF(rshift),
        create_binop_exp(BINOP_RSHIFT,
            create_binop_exp(BINOP_ADD, create_int_exp(1), create_int_exp(2)),
            create_int_exp(3)));
}

// Bitwise tiers: shift > & > ^ > |.
void test_precedence_bitwise_tiers() {
    Token shift_over_and[] = {
        make_int_token(1), make_op_token(TOK_AND),
        make_int_token(2), make_op_token(TOK_LSHIFT), make_int_token(3),
    };
    check_return_exp("1 & 2 << 3", shift_over_and, COUNT_OF(shift_over_and),
        create_binop_exp(BINOP_AND, create_int_exp(1),
            create_binop_exp(BINOP_LSHIFT, create_int_exp(2), create_int_exp(3))));

    Token and_over_xor[] = {
        make_int_token(1), make_op_token(TOK_XOR),
        make_int_token(2), make_op_token(TOK_AND), make_int_token(3),
    };
    check_return_exp("1 ^ 2 & 3", and_over_xor, COUNT_OF(and_over_xor),
        create_binop_exp(BINOP_XOR, create_int_exp(1),
            create_binop_exp(BINOP_AND, create_int_exp(2), create_int_exp(3))));

    Token xor_over_or[] = {
        make_int_token(1), make_op_token(TOK_OR),
        make_int_token(2), make_op_token(TOK_XOR), make_int_token(3),
    };
    check_return_exp("1 | 2 ^ 3", xor_over_or, COUNT_OF(xor_over_or),
        create_binop_exp(BINOP_OR, create_int_exp(1),
            create_binop_exp(BINOP_XOR, create_int_exp(2), create_int_exp(3))));
}

// One expression exercising every precedence tier at once:
// 1 | 2 ^ 3 & 4 << 5 + 6 * 7  ==  (1 | (2 ^ (3 & (4 << (5 + (6 * 7))))))
void test_precedence_full_chain() {
    Token chain[] = {
        make_int_token(1), make_op_token(TOK_OR),
        make_int_token(2), make_op_token(TOK_XOR),
        make_int_token(3), make_op_token(TOK_AND),
        make_int_token(4), make_op_token(TOK_LSHIFT),
        make_int_token(5), make_op_token(TOK_PLUS),
        make_int_token(6), make_op_token(TOK_STAR), make_int_token(7),
    };
    check_return_exp("1 | 2 ^ 3 & 4 << 5 + 6 * 7", chain, COUNT_OF(chain),
        create_binop_exp(BINOP_OR, create_int_exp(1),
            create_binop_exp(BINOP_XOR, create_int_exp(2),
                create_binop_exp(BINOP_AND, create_int_exp(3),
                    create_binop_exp(BINOP_LSHIFT, create_int_exp(4),
                        create_binop_exp(BINOP_ADD, create_int_exp(5),
                            create_binop_exp(BINOP_MUL, create_int_exp(6),
                                create_int_exp(7))))))));
}

// Parentheses override precedence.
void test_precedence_parentheses() {
    Token grouped[] = {
        make_sep_token(TOK_LPAR),
        make_int_token(1), make_op_token(TOK_OR), make_int_token(2),
        make_sep_token(TOK_RPAR),
        make_op_token(TOK_AND), make_int_token(3),
    };
    check_return_exp("(1 | 2) & 3", grouped, COUNT_OF(grouped),
        create_binop_exp(BINOP_AND,
            create_binop_exp(BINOP_OR, create_int_exp(1), create_int_exp(2)),
            create_int_exp(3)));
}

// Unary minus binds tighter than any binary operator.
void test_precedence_unary_over_binary() {
    Token negated[] = {
        make_op_token(TOK_MINUS), make_int_token(1),
        make_op_token(TOK_PLUS), make_int_token(2),
    };
    check_return_exp("-1 + 2", negated, COUNT_OF(negated),
        create_binop_exp(BINOP_ADD,
            create_unary_exp(UNOP_MINUS, create_int_exp(1)),
            create_int_exp(2)));
}

// --- Compound assignment tests ---
//
// `exp_equals` deliberately ignores `assign.op`, so these tests inspect the
// parsed assignment node directly. Each parses `int main() { x <op> 5; }` and
// checks both that the node is an assignment over `x` / `5` and that the
// operator tag is mapped correctly.
static void check_assign_op(const char* description, TokenOperator tok_op,
                            AstAssignOp expected_op) {
    TokenList tokens = token_list_create(16);
    token_list_push(&tokens, (Token){TOK_KEYWORD,    {.kw = TOK_INT},            1, 1});
    token_list_push(&tokens, (Token){TOK_IDENTIFIER, {.ident = strdup("main")}, 1, 1});
    token_list_push(&tokens, make_sep_token(TOK_LPAR));
    token_list_push(&tokens, make_sep_token(TOK_RPAR));
    token_list_push(&tokens, make_sep_token(TOK_LBRACE));
    token_list_push(&tokens, (Token){TOK_IDENTIFIER, {.ident = strdup("x")}, 1, 1});
    token_list_push(&tokens, make_op_token(tok_op));
    token_list_push(&tokens, make_int_token(5));
    token_list_push(&tokens, make_sep_token(TOK_SEMICOLON));
    token_list_push(&tokens, make_sep_token(TOK_RBRACE));

    Parser parser = parser_create(&tokens);
    AstProgram prog = parse_program(&parser);

    AstBlockItem item = prog.functions[0].body[0];
    assert(item.type == AST_STATEMENT);
    assert(item.stmt.kind == STMT_EXP);
    AstExp* exp = item.stmt.exp_stmt.exp;
    assert(exp->kind == EXP_ASSIGN);
    assert(exp->assign.lhs->kind == EXP_VAR);
    assert(strcmp(exp->assign.lhs->variable.identifier, "x") == 0);
    assert(exp->assign.rhs->kind == EXP_INT && exp->assign.rhs->int_lit.value == 5);
    if (exp->assign.op != expected_op) {
        printf("  FAIL: %s (expected assign op %d, got %d)\n",
               description, expected_op, exp->assign.op);
        exit(1);
    }

    destroy_program(&prog);
    token_list_destroy(&tokens);
    printf("  PASS: %s\n", description);
}

// Each assignment token maps to the matching AstAssignOp; plain '=' is ASSIGN_NOP.
void test_parse_compound_assign_ops() {
    check_assign_op("x = 5",   TOK_ASSIGN,    ASSIGN_NOP);
    check_assign_op("x += 5",  TOK_PLUS_EQ,   ASSIGN_ADD);
    check_assign_op("x -= 5",  TOK_MINUS_EQ,  ASSIGN_SUB);
    check_assign_op("x *= 5",  TOK_MUL_EQ,    ASSIGN_MUL);
    check_assign_op("x /= 5",  TOK_DIV_EQ,    ASSIGN_DIV);
    check_assign_op("x %= 5",  TOK_MOD_EQ,    ASSIGN_MOD);
    check_assign_op("x &= 5",  TOK_AND_EQ,    ASSIGN_AND);
    check_assign_op("x |= 5",  TOK_OR_EQ,     ASSIGN_OR);
    check_assign_op("x ^= 5",  TOK_XOR_EQ,    ASSIGN_XOR);
    check_assign_op("x >>= 5", TOK_RSHIFT_EQ, ASSIGN_RSHIFT);
    check_assign_op("x <<= 5", TOK_LSHIFT_EQ, ASSIGN_LSHIFT);
}

// Assignment is right-associative: `x += y += 5` parses as `x += (y += 5)`,
// and each node keeps its own operator tag.
void test_parse_compound_assign_right_assoc() {
    TokenList tokens = token_list_create(16);
    token_list_push(&tokens, (Token){TOK_KEYWORD,    {.kw = TOK_INT},            1, 1});
    token_list_push(&tokens, (Token){TOK_IDENTIFIER, {.ident = strdup("main")}, 1, 1});
    token_list_push(&tokens, make_sep_token(TOK_LPAR));
    token_list_push(&tokens, make_sep_token(TOK_RPAR));
    token_list_push(&tokens, make_sep_token(TOK_LBRACE));
    token_list_push(&tokens, (Token){TOK_IDENTIFIER, {.ident = strdup("x")}, 1, 1});
    token_list_push(&tokens, make_op_token(TOK_PLUS_EQ));
    token_list_push(&tokens, (Token){TOK_IDENTIFIER, {.ident = strdup("y")}, 1, 1});
    token_list_push(&tokens, make_op_token(TOK_MUL_EQ));
    token_list_push(&tokens, make_int_token(5));
    token_list_push(&tokens, make_sep_token(TOK_SEMICOLON));
    token_list_push(&tokens, make_sep_token(TOK_RBRACE));

    Parser parser = parser_create(&tokens);
    AstProgram prog = parse_program(&parser);

    AstExp* outer = prog.functions[0].body[0].stmt.exp_stmt.exp;
    assert(outer->kind == EXP_ASSIGN && outer->assign.op == ASSIGN_ADD);
    assert(outer->assign.lhs->kind == EXP_VAR);
    assert(strcmp(outer->assign.lhs->variable.identifier, "x") == 0);

    AstExp* inner = outer->assign.rhs;
    assert(inner->kind == EXP_ASSIGN && inner->assign.op == ASSIGN_MUL);
    assert(inner->assign.lhs->kind == EXP_VAR);
    assert(strcmp(inner->assign.lhs->variable.identifier, "y") == 0);
    assert(inner->assign.rhs->kind == EXP_INT && inner->assign.rhs->int_lit.value == 5);

    destroy_program(&prog);
    token_list_destroy(&tokens);
    printf("  PASS: test_parse_compound_assign_right_assoc\n");
}

int main(void) {
    printf("Running parser tests...\n");
    test_create_int_exp();
    test_create_unary_exp();
    test_create_binop_exp();
    test_create_return_stmt();
    test_create_function_decl();
    test_parse_return_2();
    test_precedence_mul_over_add();
    test_precedence_left_associativity();
    test_precedence_add_over_shift();
    test_precedence_bitwise_tiers();
    test_precedence_full_chain();
    test_precedence_parentheses();
    test_precedence_unary_over_binary();
    test_parse_compound_assign_ops();
    test_parse_compound_assign_right_assoc();
    printf("All parser tests passed!\n");
    return 0;
}
