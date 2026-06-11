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
    AstStatement* body = malloc(sizeof(AstStatement));
    body[0] = make_return_stmt(create_int_exp(0));

    AstDeclaration fn = make_function_decl("main", body, 1);
    assert(fn.kind == DECL_FUNCTION);
    assert(strcmp(fn.function.name, "main") == 0);
    assert(fn.function.num_stmts == 1);
    assert(fn.function.body[0].kind == STMT_RETURN);
    destroy_decl(&fn);
    printf("  PASS: test_create_function_decl\n");
}

// --- Parser integration test ---
// Parses: int main() { return 2; }

void test_parse_return_2() {
    token_list tokens = token_list_create(8);

    token_list_push(&tokens, (token){TOK_KEYWORD,   {.kw = KW_INT},          1, 1});
    token_list_push(&tokens, (token){TOK_IDENTIFIER, {.ident = strdup("main")}, 1, 5});
    token_list_push(&tokens, (token){TOK_SEPARATOR, {.sep = SEP_LPAR},       1, 9});
    token_list_push(&tokens, (token){TOK_SEPARATOR, {.sep = SEP_RPAR},       1, 10});
    token_list_push(&tokens, (token){TOK_SEPARATOR, {.sep = SEP_LBRACE},     1, 12});
    token_list_push(&tokens, (token){TOK_KEYWORD,   {.kw = KW_RETURN},       2, 5});
    token_list_push(&tokens, (token){TOK_INT_LITERAL, {.int_val = 2},        2, 12});
    token_list_push(&tokens, (token){TOK_SEPARATOR, {.sep = SEP_SEMICOLON},  2, 13});
    token_list_push(&tokens, (token){TOK_SEPARATOR, {.sep = SEP_RBRACE},     3, 1});

    Parser parser = parser_create(&tokens);
    AstProgram prog = parse_program(&parser);

    assert(prog.num_decls == 1);
    assert(prog.decls[0].kind == DECL_FUNCTION);
    assert(strcmp(prog.decls[0].function.name, "main") == 0);
    assert(prog.decls[0].function.num_stmts == 1);
    assert(prog.decls[0].function.body[0].kind == STMT_RETURN);
    assert(prog.decls[0].function.body[0].ret.exp->kind == EXP_INT);
    assert(prog.decls[0].function.body[0].ret.exp->int_lit.value == 2);

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

static token make_int_token(int value) {
    return (token){TOK_INT_LITERAL, {.int_val = value}, 1, 1};
}

static token make_op_token(token_operator op) {
    return (token){TOK_OPERATOR, {.op = op}, 1, 1};
}

static token make_sep_token(token_separator sep) {
    return (token){TOK_SEPARATOR, {.sep = sep}, 1, 1};
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
    }
    return "<unknown>";
}

static void print_exp(const AstExp* exp) {
    switch (exp->kind) {
        case EXP_INT:
            printf("%d", exp->int_lit.value);
            break;
        case EXP_UNOP:
            printf("(%s ", exp->unary.op_type == UNOP_NOT ? "!" : "-");
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
    }
    return false;
}

// Parses `int main() { return <exp_tokens>; }` and checks that the returned
// expression matches `expected`. Takes ownership of `expected`.
static void check_return_exp(const char* description, const token* exp_tokens,
                             size_t num_exp_tokens, AstExp* expected) {
    token_list tokens = token_list_create(num_exp_tokens + 8);

    token_list_push(&tokens, (token){TOK_KEYWORD,    {.kw = KW_INT},            1, 1});
    token_list_push(&tokens, (token){TOK_IDENTIFIER, {.ident = strdup("main")}, 1, 1});
    token_list_push(&tokens, make_sep_token(SEP_LPAR));
    token_list_push(&tokens, make_sep_token(SEP_RPAR));
    token_list_push(&tokens, make_sep_token(SEP_LBRACE));
    token_list_push(&tokens, (token){TOK_KEYWORD,    {.kw = KW_RETURN},         1, 1});
    for (size_t i = 0; i < num_exp_tokens; i++) {
        token_list_push(&tokens, exp_tokens[i]);
    }
    token_list_push(&tokens, make_sep_token(SEP_SEMICOLON));
    token_list_push(&tokens, make_sep_token(SEP_RBRACE));

    Parser parser = parser_create(&tokens);
    AstProgram prog = parse_program(&parser);

    AstExp* actual = prog.decls[0].function.body[0].ret.exp;
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
    token left[] = {
        make_int_token(1), make_op_token(OP_PLUS),
        make_int_token(2), make_op_token(OP_STAR), make_int_token(3),
    };
    check_return_exp("1 + 2 * 3;", left, COUNT_OF(left),
        create_binop_exp(BINOP_ADD, create_int_exp(1),
            create_binop_exp(BINOP_MUL, create_int_exp(2), create_int_exp(3))));

    token right[] = {
        make_int_token(2), make_op_token(OP_STAR), make_int_token(3),
        make_op_token(OP_PLUS), make_int_token(4),
    };
    check_return_exp("2 * 3 + 4", right, COUNT_OF(right),
        create_binop_exp(BINOP_ADD,
            create_binop_exp(BINOP_MUL, create_int_exp(2), create_int_exp(3)),
            create_int_exp(4)));
}

// Same-precedence operators associate to the left.
void test_precedence_left_associativity() {
    token sub[] = {
        make_int_token(10), make_op_token(OP_MINUS),
        make_int_token(4), make_op_token(OP_MINUS), make_int_token(3),
    };
    check_return_exp("10 - 4 - 3", sub, COUNT_OF(sub),
        create_binop_exp(BINOP_SUB,
            create_binop_exp(BINOP_SUB, create_int_exp(10), create_int_exp(4)),
            create_int_exp(3)));

    token div_mod[] = {
        make_int_token(8), make_op_token(OP_FSLASH),
        make_int_token(4), make_op_token(OP_PERCENT), make_int_token(3),
    };
    check_return_exp("8 / 4 % 3", div_mod, COUNT_OF(div_mod),
        create_binop_exp(BINOP_MOD,
            create_binop_exp(BINOP_DIV, create_int_exp(8), create_int_exp(4)),
            create_int_exp(3)));

    token shift[] = {
        make_int_token(1), make_op_token(OP_LSHIFT),
        make_int_token(2), make_op_token(OP_LSHIFT), make_int_token(3),
    };
    check_return_exp("1 << 2 << 3", shift, COUNT_OF(shift),
        create_binop_exp(BINOP_LSHIFT,
            create_binop_exp(BINOP_LSHIFT, create_int_exp(1), create_int_exp(2)),
            create_int_exp(3)));
}

// Additive binds tighter than shifts.
void test_precedence_add_over_shift() {
    token lshift[] = {
        make_int_token(1), make_op_token(OP_LSHIFT),
        make_int_token(2), make_op_token(OP_PLUS), make_int_token(3),
    };
    check_return_exp("1 << 2 + 3", lshift, COUNT_OF(lshift),
        create_binop_exp(BINOP_LSHIFT, create_int_exp(1),
            create_binop_exp(BINOP_ADD, create_int_exp(2), create_int_exp(3))));

    token rshift[] = {
        make_int_token(1), make_op_token(OP_PLUS),
        make_int_token(2), make_op_token(OP_RSHIFT), make_int_token(3),
    };
    check_return_exp("1 + 2 >> 3", rshift, COUNT_OF(rshift),
        create_binop_exp(BINOP_RSHIFT,
            create_binop_exp(BINOP_ADD, create_int_exp(1), create_int_exp(2)),
            create_int_exp(3)));
}

// Bitwise tiers: shift > & > ^ > |.
void test_precedence_bitwise_tiers() {
    token shift_over_and[] = {
        make_int_token(1), make_op_token(OP_AND),
        make_int_token(2), make_op_token(OP_LSHIFT), make_int_token(3),
    };
    check_return_exp("1 & 2 << 3", shift_over_and, COUNT_OF(shift_over_and),
        create_binop_exp(BINOP_AND, create_int_exp(1),
            create_binop_exp(BINOP_LSHIFT, create_int_exp(2), create_int_exp(3))));

    token and_over_xor[] = {
        make_int_token(1), make_op_token(OP_XOR),
        make_int_token(2), make_op_token(OP_AND), make_int_token(3),
    };
    check_return_exp("1 ^ 2 & 3", and_over_xor, COUNT_OF(and_over_xor),
        create_binop_exp(BINOP_XOR, create_int_exp(1),
            create_binop_exp(BINOP_AND, create_int_exp(2), create_int_exp(3))));

    token xor_over_or[] = {
        make_int_token(1), make_op_token(OP_OR),
        make_int_token(2), make_op_token(OP_XOR), make_int_token(3),
    };
    check_return_exp("1 | 2 ^ 3", xor_over_or, COUNT_OF(xor_over_or),
        create_binop_exp(BINOP_OR, create_int_exp(1),
            create_binop_exp(BINOP_XOR, create_int_exp(2), create_int_exp(3))));
}

// One expression exercising every precedence tier at once:
// 1 | 2 ^ 3 & 4 << 5 + 6 * 7  ==  (1 | (2 ^ (3 & (4 << (5 + (6 * 7))))))
void test_precedence_full_chain() {
    token chain[] = {
        make_int_token(1), make_op_token(OP_OR),
        make_int_token(2), make_op_token(OP_XOR),
        make_int_token(3), make_op_token(OP_AND),
        make_int_token(4), make_op_token(OP_LSHIFT),
        make_int_token(5), make_op_token(OP_PLUS),
        make_int_token(6), make_op_token(OP_STAR), make_int_token(7),
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
    token grouped[] = {
        make_sep_token(SEP_LPAR),
        make_int_token(1), make_op_token(OP_OR), make_int_token(2),
        make_sep_token(SEP_RPAR),
        make_op_token(OP_AND), make_int_token(3),
    };
    check_return_exp("(1 | 2) & 3", grouped, COUNT_OF(grouped),
        create_binop_exp(BINOP_AND,
            create_binop_exp(BINOP_OR, create_int_exp(1), create_int_exp(2)),
            create_int_exp(3)));
}

// Unary minus binds tighter than any binary operator.
void test_precedence_unary_over_binary() {
    token negated[] = {
        make_op_token(OP_MINUS), make_int_token(1),
        make_op_token(OP_PLUS), make_int_token(2),
    };
    check_return_exp("-1 + 2", negated, COUNT_OF(negated),
        create_binop_exp(BINOP_ADD,
            create_unary_exp(UNOP_MINUS, create_int_exp(1)),
            create_int_exp(2)));
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
    printf("All parser tests passed!\n");
    return 0;
}
