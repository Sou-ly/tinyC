#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../src/parser/ast.h"
#include "../src/parser/parser.h"
#include "../src/lexer/token_list.h"

// --- AST unit tests ---

void test_create_int_exp() {
    AstExp* e = create_int_exp(42);
    assert(e != NULL);
    assert(e->kind == EXP_INT);
    assert(e->as.int_lit.value == 42);
    destroy_exp(e);
    printf("  PASS: test_create_int_exp\n");
}

void test_create_unary_exp() {
    AstExp* e = create_unary_exp(UNOP_MINUS, create_int_exp(5));
    assert(e != NULL);
    assert(e->kind == EXP_UNOP);
    assert(e->as.unary.op_type == UNOP_MINUS);
    assert(e->as.unary.operand->as.int_lit.value == 5);
    destroy_exp(e);
    printf("  PASS: test_create_unary_exp\n");
}

void test_create_binop_exp() {
    AstExp* e = create_binop_exp(BINOP_ADD, create_int_exp(3), create_int_exp(7));
    assert(e != NULL);
    assert(e->kind == EXP_BINOP);
    assert(e->as.binop.op_type == BINOP_ADD);
    assert(e->as.binop.lhs->as.int_lit.value == 3);
    assert(e->as.binop.rhs->as.int_lit.value == 7);
    destroy_exp(e);
    printf("  PASS: test_create_binop_exp\n");
}

void test_create_conditional_exp() {
    AstExp* e = create_conditional_exp(create_int_exp(1), create_int_exp(2), create_int_exp(3));
    assert(e != NULL);
    assert(e->kind == EXP_CONDITIONAL);
    assert(e->as.conditional.lhs->as.int_lit.value == 1);
    assert(e->as.conditional.mid->as.int_lit.value == 2);
    assert(e->as.conditional.rhs->as.int_lit.value == 3);
    destroy_exp(e);
    printf("  PASS: test_create_conditional_exp\n");
}

void test_create_return_stmt() {
    AstStatement* s = make_return_stmt(create_int_exp(2));
    assert(s->kind == STMT_RETURN);
    assert(s->as.ret.exp->as.int_lit.value == 2);
    destroy_stmt(s);
    printf("  PASS: test_create_return_stmt\n");
}

// make_if_stmt takes heap-owned branches; make_*_stmt already heap-allocates,
// so this is just a pass-through kept for readability at the call sites.
static AstStatement* heap_stmt(AstStatement* stmt) {
    return stmt;
}

// An if with both branches records the condition and both heap-owned branches,
// and leaves the (loop/label pass) label unset.
void test_create_if_stmt_with_else() {
    AstStatement* s = make_if_stmt(
        create_int_exp(1),
        heap_stmt(make_return_stmt(create_int_exp(2))),
        heap_stmt(make_return_stmt(create_int_exp(3))));

    assert(s->kind == STMT_IF);
    assert(s->as.if_cond.cond->kind == EXP_INT);
    assert(s->as.if_cond.cond->as.int_lit.value == 1);
    assert(s->as.if_cond.then_br->kind == STMT_RETURN);
    assert(s->as.if_cond.then_br->as.ret.exp->as.int_lit.value == 2);
    assert(s->as.if_cond.else_br != NULL);
    assert(s->as.if_cond.else_br->kind == STMT_RETURN);
    assert(s->as.if_cond.else_br->as.ret.exp->as.int_lit.value == 3);

    destroy_stmt(s);
    printf("  PASS: test_create_if_stmt_with_else\n");
}

// A plain if leaves else_br NULL; destroy_stmt must tolerate the missing branch.
void test_create_if_stmt_no_else() {
    AstStatement* s = make_if_stmt(
        create_int_exp(1),
        heap_stmt(make_return_stmt(create_int_exp(2))),
        NULL);

    assert(s->kind == STMT_IF);
    assert(s->as.if_cond.then_br->kind == STMT_RETURN);
    assert(s->as.if_cond.else_br == NULL);

    destroy_stmt(s);
    printf("  PASS: test_create_if_stmt_no_else\n");
}

void test_create_function_decl() {
    AstFunction fn = ast_function_make("main", ast_block_make(8));
    ast_function_append(&fn, (AstBlockItem){
        .type = AST_STATEMENT,
        .as.stmt = make_return_stmt(create_int_exp(0)),
    });

    assert(strcmp(fn.identifier, "main") == 0);
    assert(fn.body.size == 1);
    assert(fn.body.items[0].type == AST_STATEMENT);
    assert(fn.body.items[0].as.stmt->kind == STMT_RETURN);
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
    assert(prog.functions[0].body.size == 1);
    assert(prog.functions[0].body.items[0].type == AST_STATEMENT);
    assert(prog.functions[0].body.items[0].as.stmt->kind == STMT_RETURN);
    assert(prog.functions[0].body.items[0].as.stmt->as.ret.exp->kind == EXP_INT);
    assert(prog.functions[0].body.items[0].as.stmt->as.ret.exp->as.int_lit.value == 2);

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

static Token make_ident_token(const char* name) {
    return (Token){TOK_IDENTIFIER, {.ident = strdup(name)}, 1, 1};
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
        case BINOP_CONDITION: return "?:";
    }
    return "<unknown>";
}

static void print_exp(const AstExp* exp) {
    switch (exp->kind) {
        case EXP_INT:
            printf("%d", exp->as.int_lit.value);
            break;
        case EXP_UNOP: {
            const char* name = "?";
            switch (exp->as.unary.op_type) {
                case UNOP_COMP:    name = "!";   break;
                case UNOP_MINUS:   name = "-";   break;
                case UNOP_NOT:     name = "~";   break;
                case UNOP_PREINC:  name = "pre++";  break;
                case UNOP_PREDEC:  name = "pre--";  break;
                case UNOP_POSTINC: name = "post++"; break;
                case UNOP_POSTDEC: name = "post--"; break;
            }
            printf("(%s ", name);
            print_exp(exp->as.unary.operand);
            printf(")");
            break;
        }
        case EXP_BINOP:
            printf("(");
            print_exp(exp->as.binop.lhs);
            printf(" %s ", binop_name(exp->as.binop.op_type));
            print_exp(exp->as.binop.rhs);
            printf(")");
            break;
        case EXP_VAR:
            printf("%s", exp->as.variable.identifier);
            break;
        case EXP_ASSIGN:
            printf("(");
            print_exp(exp->as.assign.lhs);
            printf(" = ");
            print_exp(exp->as.assign.rhs);
            printf(")");
            break;
        case EXP_CONDITIONAL:
            printf("(");
            print_exp(exp->as.conditional.lhs);
            printf(" ? ");
            print_exp(exp->as.conditional.mid);
            printf(" : ");
            print_exp(exp->as.conditional.rhs);
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
            return a->as.int_lit.value == b->as.int_lit.value;
        case EXP_UNOP:
            return a->as.unary.op_type == b->as.unary.op_type
                && exp_equals(a->as.unary.operand, b->as.unary.operand);
        case EXP_BINOP:
            return a->as.binop.op_type == b->as.binop.op_type
                && exp_equals(a->as.binop.lhs, b->as.binop.lhs)
                && exp_equals(a->as.binop.rhs, b->as.binop.rhs);
        case EXP_VAR:
            return strcmp(a->as.variable.identifier, b->as.variable.identifier) == 0;
        case EXP_ASSIGN:
            return exp_equals(a->as.assign.lhs, b->as.assign.lhs)
                && exp_equals(a->as.assign.rhs, b->as.assign.rhs);
        case EXP_CONDITIONAL:
            return exp_equals(a->as.conditional.lhs, b->as.conditional.lhs)
                && exp_equals(a->as.conditional.mid, b->as.conditional.mid)
                && exp_equals(a->as.conditional.rhs, b->as.conditional.rhs);
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

    AstExp* actual = prog.functions[0].body.items[0].as.stmt->as.ret.exp;
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

    AstBlockItem item = prog.functions[0].body.items[0];
    assert(item.type == AST_STATEMENT);
    assert(item.as.stmt->kind == STMT_EXP);
    AstExp* exp = item.as.stmt->as.exp_stmt.exp;
    assert(exp->kind == EXP_ASSIGN);
    assert(exp->as.assign.lhs->kind == EXP_VAR);
    assert(strcmp(exp->as.assign.lhs->as.variable.identifier, "x") == 0);
    assert(exp->as.assign.rhs->kind == EXP_INT && exp->as.assign.rhs->as.int_lit.value == 5);
    if (exp->as.assign.op != expected_op) {
        printf("  FAIL: %s (expected assign op %d, got %d)\n",
               description, expected_op, exp->as.assign.op);
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

    AstExp* outer = prog.functions[0].body.items[0].as.stmt->as.exp_stmt.exp;
    assert(outer->kind == EXP_ASSIGN && outer->as.assign.op == ASSIGN_ADD);
    assert(outer->as.assign.lhs->kind == EXP_VAR);
    assert(strcmp(outer->as.assign.lhs->as.variable.identifier, "x") == 0);

    AstExp* inner = outer->as.assign.rhs;
    assert(inner->kind == EXP_ASSIGN && inner->as.assign.op == ASSIGN_MUL);
    assert(inner->as.assign.lhs->kind == EXP_VAR);
    assert(strcmp(inner->as.assign.lhs->as.variable.identifier, "y") == 0);
    assert(inner->as.assign.rhs->kind == EXP_INT && inner->as.assign.rhs->as.int_lit.value == 5);

    destroy_program(&prog);
    token_list_destroy(&tokens);
    printf("  PASS: test_parse_compound_assign_right_assoc\n");
}

// --- Increment / decrement tests ---
//
// Prefix `++id` / `--id` and postfix `id++` / `id--` each parse to a unary
// node over the variable, tagged with the matching pre/post op. Each case is
// parsed as `int main() { return <exp>; }`.
void test_parse_prefix_increment() {
    Token toks[] = { make_op_token(TOK_INCR), make_ident_token("x") };
    check_return_exp("++x", toks, COUNT_OF(toks),
        create_unary_exp(UNOP_PREINC, create_variable_exp("x")));
}

void test_parse_prefix_decrement() {
    Token toks[] = { make_op_token(TOK_DECR), make_ident_token("x") };
    check_return_exp("--x", toks, COUNT_OF(toks),
        create_unary_exp(UNOP_PREDEC, create_variable_exp("x")));
}

void test_parse_postfix_increment() {
    Token toks[] = { make_ident_token("x"), make_op_token(TOK_INCR) };
    check_return_exp("x++", toks, COUNT_OF(toks),
        create_unary_exp(UNOP_POSTINC, create_variable_exp("x")));
}

void test_parse_postfix_decrement() {
    Token toks[] = { make_ident_token("x"), make_op_token(TOK_DECR) };
    check_return_exp("x--", toks, COUNT_OF(toks),
        create_unary_exp(UNOP_POSTDEC, create_variable_exp("x")));
}

// --- Conditional (ternary) tests ---
//
// `cond ? then : else`. The condition binds like a normal binary operand, the
// middle is a full expression (parsed down to 0), and the operator is
// right-associative. Each case is parsed as `int main() { return <exp>; }`.

// Basic shape: `1 ? 2 : 3` -> conditional(1, 2, 3).
void test_parse_conditional_basic() {
    Token toks[] = {
        make_int_token(1), make_op_token(TOK_QUESTION_MARK),
        make_int_token(2), make_sep_token(TOK_COLON), make_int_token(3),
    };
    check_return_exp("1 ? 2 : 3", toks, COUNT_OF(toks),
        create_conditional_exp(create_int_exp(1), create_int_exp(2), create_int_exp(3)));
}

// The condition is lower precedence than arithmetic: `1 + 2 ? 3 : 4` groups as
// `(1 + 2) ? 3 : 4`.
void test_parse_conditional_below_arithmetic() {
    Token toks[] = {
        make_int_token(1), make_op_token(TOK_PLUS), make_int_token(2),
        make_op_token(TOK_QUESTION_MARK),
        make_int_token(3), make_sep_token(TOK_COLON), make_int_token(4),
    };
    check_return_exp("1 + 2 ? 3 : 4", toks, COUNT_OF(toks),
        create_conditional_exp(
            create_binop_exp(BINOP_ADD, create_int_exp(1), create_int_exp(2)),
            create_int_exp(3), create_int_exp(4)));
}

// The middle branch is a full expression, so a bare binop there stays grouped:
// `1 ? 2 + 3 : 4` -> conditional(1, (2 + 3), 4).
void test_parse_conditional_middle_is_full_exp() {
    Token toks[] = {
        make_int_token(1), make_op_token(TOK_QUESTION_MARK),
        make_int_token(2), make_op_token(TOK_PLUS), make_int_token(3),
        make_sep_token(TOK_COLON), make_int_token(4),
    };
    check_return_exp("1 ? 2 + 3 : 4", toks, COUNT_OF(toks),
        create_conditional_exp(create_int_exp(1),
            create_binop_exp(BINOP_ADD, create_int_exp(2), create_int_exp(3)),
            create_int_exp(4)));
}

// Conditionals are right-associative: `1 ? 2 : 3 ? 4 : 5` groups as
// `1 ? 2 : (3 ? 4 : 5)`.
void test_parse_conditional_right_assoc() {
    Token toks[] = {
        make_int_token(1), make_op_token(TOK_QUESTION_MARK),
        make_int_token(2), make_sep_token(TOK_COLON),
        make_int_token(3), make_op_token(TOK_QUESTION_MARK),
        make_int_token(4), make_sep_token(TOK_COLON), make_int_token(5),
    };
    check_return_exp("1 ? 2 : 3 ? 4 : 5", toks, COUNT_OF(toks),
        create_conditional_exp(create_int_exp(1), create_int_exp(2),
            create_conditional_exp(create_int_exp(3), create_int_exp(4),
                create_int_exp(5))));
}

// --- Loop parsing tests ---
//
// Each test builds a token stream for `int main() { <loop>; }`, parses it,
// and checks the resulting AST structure.

static Token make_kw_token(TokenKeyword kw) {
    return (Token){TOK_KEYWORD, {.kw = kw}, 1, 1};
}

// Parses: int main() { while (1) 2; }
void test_parse_while_loop() {
    TokenList tokens = token_list_create(16);
    token_list_push(&tokens, make_kw_token(TOK_INT));
    token_list_push(&tokens, make_ident_token("main"));
    token_list_push(&tokens, make_sep_token(TOK_LPAR));
    token_list_push(&tokens, make_sep_token(TOK_RPAR));
    token_list_push(&tokens, make_sep_token(TOK_LBRACE));
    // while (1) 2;
    token_list_push(&tokens, make_kw_token(TOK_WHILE));
    token_list_push(&tokens, make_sep_token(TOK_LPAR));
    token_list_push(&tokens, make_int_token(1));
    token_list_push(&tokens, make_sep_token(TOK_RPAR));
    token_list_push(&tokens, make_int_token(2));
    token_list_push(&tokens, make_sep_token(TOK_SEMICOLON));
    token_list_push(&tokens, make_sep_token(TOK_RBRACE));

    Parser parser = parser_create(&tokens);
    AstProgram prog = parse_program(&parser);

    AstStatement* stmt = prog.functions[0].body.items[0].as.stmt;
    assert(stmt->kind == STMT_WHILE);
    assert(stmt->as.while_loop.label == NULL);
    assert(stmt->as.while_loop.cond->as.int_lit.value == 1);
    assert(stmt->as.while_loop.body->kind == STMT_EXP);
    assert(stmt->as.while_loop.body->as.exp_stmt.exp->as.int_lit.value == 2);

    destroy_program(&prog);
    token_list_destroy(&tokens);
    printf("  PASS: test_parse_while_loop\n");
}

// Parses: int main() { do 1; while (2); }
void test_parse_do_while_loop() {
    TokenList tokens = token_list_create(16);
    token_list_push(&tokens, make_kw_token(TOK_INT));
    token_list_push(&tokens, make_ident_token("main"));
    token_list_push(&tokens, make_sep_token(TOK_LPAR));
    token_list_push(&tokens, make_sep_token(TOK_RPAR));
    token_list_push(&tokens, make_sep_token(TOK_LBRACE));
    // do 1; while (2);
    token_list_push(&tokens, make_kw_token(TOK_DO));
    token_list_push(&tokens, make_int_token(1));
    token_list_push(&tokens, make_sep_token(TOK_SEMICOLON));
    token_list_push(&tokens, make_kw_token(TOK_WHILE));
    token_list_push(&tokens, make_sep_token(TOK_LPAR));
    token_list_push(&tokens, make_int_token(2));
    token_list_push(&tokens, make_sep_token(TOK_RPAR));
    token_list_push(&tokens, make_sep_token(TOK_SEMICOLON));
    token_list_push(&tokens, make_sep_token(TOK_RBRACE));

    Parser parser = parser_create(&tokens);
    AstProgram prog = parse_program(&parser);

    AstStatement* stmt = prog.functions[0].body.items[0].as.stmt;
    assert(stmt->kind == STMT_DO_WHILE);
    assert(stmt->as.do_while_loop.label == NULL);
    assert(stmt->as.do_while_loop.cond->as.int_lit.value == 2);
    assert(stmt->as.do_while_loop.body->kind == STMT_EXP);

    destroy_program(&prog);
    token_list_destroy(&tokens);
    printf("  PASS: test_parse_do_while_loop\n");
}

// Parses: int main() { for (0; 1; 2) 3; }
void test_parse_for_loop() {
    TokenList tokens = token_list_create(16);
    token_list_push(&tokens, make_kw_token(TOK_INT));
    token_list_push(&tokens, make_ident_token("main"));
    token_list_push(&tokens, make_sep_token(TOK_LPAR));
    token_list_push(&tokens, make_sep_token(TOK_RPAR));
    token_list_push(&tokens, make_sep_token(TOK_LBRACE));
    // for (0; 1; 2) 3;
    token_list_push(&tokens, make_kw_token(TOK_FOR));
    token_list_push(&tokens, make_sep_token(TOK_LPAR));
    token_list_push(&tokens, make_int_token(0));
    token_list_push(&tokens, make_sep_token(TOK_SEMICOLON));
    token_list_push(&tokens, make_int_token(1));
    token_list_push(&tokens, make_sep_token(TOK_SEMICOLON));
    token_list_push(&tokens, make_int_token(2));
    token_list_push(&tokens, make_sep_token(TOK_RPAR));
    token_list_push(&tokens, make_int_token(3));
    token_list_push(&tokens, make_sep_token(TOK_SEMICOLON));
    token_list_push(&tokens, make_sep_token(TOK_RBRACE));

    Parser parser = parser_create(&tokens);
    AstProgram prog = parse_program(&parser);

    AstStatement* stmt = prog.functions[0].body.items[0].as.stmt;
    assert(stmt->kind == STMT_FOR);
    assert(stmt->as.for_loop.label == NULL);
    assert(stmt->as.for_loop.cond->as.int_lit.value == 1);
    assert(stmt->as.for_loop.post->as.int_lit.value == 2);
    assert(stmt->as.for_loop.body->kind == STMT_EXP);

    destroy_program(&prog);
    token_list_destroy(&tokens);
    printf("  PASS: test_parse_for_loop\n");
}

// Parses: int main() { while (1) break; }
void test_parse_break_in_loop() {
    TokenList tokens = token_list_create(16);
    token_list_push(&tokens, make_kw_token(TOK_INT));
    token_list_push(&tokens, make_ident_token("main"));
    token_list_push(&tokens, make_sep_token(TOK_LPAR));
    token_list_push(&tokens, make_sep_token(TOK_RPAR));
    token_list_push(&tokens, make_sep_token(TOK_LBRACE));
    // while (1) break;
    token_list_push(&tokens, make_kw_token(TOK_WHILE));
    token_list_push(&tokens, make_sep_token(TOK_LPAR));
    token_list_push(&tokens, make_int_token(1));
    token_list_push(&tokens, make_sep_token(TOK_RPAR));
    token_list_push(&tokens, make_kw_token(TOK_BREAK));
    token_list_push(&tokens, make_sep_token(TOK_SEMICOLON));
    token_list_push(&tokens, make_sep_token(TOK_RBRACE));

    Parser parser = parser_create(&tokens);
    AstProgram prog = parse_program(&parser);

    AstStatement* stmt = prog.functions[0].body.items[0].as.stmt;
    assert(stmt->kind == STMT_WHILE);
    assert(stmt->as.while_loop.body->kind == STMT_BREAK);
    assert(stmt->as.while_loop.body->as.break_stmt.label == NULL);

    destroy_program(&prog);
    token_list_destroy(&tokens);
    printf("  PASS: test_parse_break_in_loop\n");
}

// Parses: int main() { while (1) continue; }
void test_parse_continue_in_loop() {
    TokenList tokens = token_list_create(16);
    token_list_push(&tokens, make_kw_token(TOK_INT));
    token_list_push(&tokens, make_ident_token("main"));
    token_list_push(&tokens, make_sep_token(TOK_LPAR));
    token_list_push(&tokens, make_sep_token(TOK_RPAR));
    token_list_push(&tokens, make_sep_token(TOK_LBRACE));
    // while (1) continue;
    token_list_push(&tokens, make_kw_token(TOK_WHILE));
    token_list_push(&tokens, make_sep_token(TOK_LPAR));
    token_list_push(&tokens, make_int_token(1));
    token_list_push(&tokens, make_sep_token(TOK_RPAR));
    token_list_push(&tokens, make_kw_token(TOK_CONTINUE));
    token_list_push(&tokens, make_sep_token(TOK_SEMICOLON));
    token_list_push(&tokens, make_sep_token(TOK_RBRACE));

    Parser parser = parser_create(&tokens);
    AstProgram prog = parse_program(&parser);

    AstStatement* stmt = prog.functions[0].body.items[0].as.stmt;
    assert(stmt->kind == STMT_WHILE);
    assert(stmt->as.while_loop.body->kind == STMT_CONTINUE);
    assert(stmt->as.while_loop.body->as.continue_stmt.label == NULL);

    destroy_program(&prog);
    token_list_destroy(&tokens);
    printf("  PASS: test_parse_continue_in_loop\n");
}

// --- Label resolution tests ---
//
// resolve_labels assigns unique labels to loops and propagates them to
// break/continue statements within their bodies.

// Helper: wraps a single statement in a program with one function.
static AstProgram make_test_program(AstStatement* stmt) {
    AstBlock body = ast_block_make(1);
    ast_block_append(&body, (AstBlockItem){ .type = AST_STATEMENT, .as.stmt = stmt });
    AstFunction* fn = malloc(sizeof(AstFunction));
    *fn = ast_function_make("main", body);
    return ast_program_create(fn, 1);
}

// resolve_labels assigns a label to a while loop and its nested break.
void test_resolve_labels_while_break() {
    AstProgram prog = make_test_program(
        make_while_stmt(create_int_exp(1), make_break_stmt(NULL)));

    resolve_labels(&prog);

    AstStatement* resolved = prog.functions[0].body.items[0].as.stmt;
    assert(resolved->kind == STMT_WHILE);
    assert(resolved->as.while_loop.label != NULL);
    char* loop_label = resolved->as.while_loop.label;
    assert(resolved->as.while_loop.body->kind == STMT_BREAK);
    assert(strcmp(resolved->as.while_loop.body->as.break_stmt.label, loop_label) == 0);

    destroy_program(&prog);
    printf("  PASS: test_resolve_labels_while_break\n");
}

// resolve_labels assigns a label to a while loop and its nested continue.
void test_resolve_labels_while_continue() {
    AstProgram prog = make_test_program(
        make_while_stmt(create_int_exp(1), make_continue_stmt(NULL)));

    resolve_labels(&prog);

    AstStatement* resolved = prog.functions[0].body.items[0].as.stmt;
    assert(resolved->as.while_loop.label != NULL);
    assert(strcmp(resolved->as.while_loop.body->as.continue_stmt.label,
                 resolved->as.while_loop.label) == 0);

    destroy_program(&prog);
    printf("  PASS: test_resolve_labels_while_continue\n");
}

// resolve_labels assigns different labels to nested loops; break/continue
// in each body get the label of their enclosing loop.
void test_resolve_labels_nested_loops() {
    // inner: for (0; 1; 2) break;
    AstForInit init = make_for_init_exp(NULL);
    AstStatement* inner = make_for_stmt(init, NULL, NULL, make_break_stmt(NULL));

    // outer body: { inner_loop; continue; }
    AstBlock outer_body = ast_block_make(2);
    ast_block_append(&outer_body, (AstBlockItem){ .type = AST_STATEMENT, .as.stmt = inner });
    ast_block_append(&outer_body, (AstBlockItem){ .type = AST_STATEMENT, .as.stmt = make_continue_stmt(NULL) });

    AstProgram prog = make_test_program(
        make_while_stmt(create_int_exp(1), make_compound_stmt(outer_body)));

    resolve_labels(&prog);

    AstStatement* r_outer = prog.functions[0].body.items[0].as.stmt;
    assert(r_outer->kind == STMT_WHILE);
    char* outer_label = r_outer->as.while_loop.label;
    assert(outer_label != NULL);

    AstBlock* compound = &r_outer->as.while_loop.body->as.compound;
    AstStatement* r_inner = compound->items[0].as.stmt;
    assert(r_inner->kind == STMT_FOR);
    char* inner_label = r_inner->as.for_loop.label;
    assert(inner_label != NULL);

    // Labels must be different
    assert(strcmp(outer_label, inner_label) != 0);

    // Inner break gets inner label
    assert(strcmp(r_inner->as.for_loop.body->as.break_stmt.label, inner_label) == 0);

    // Outer continue gets outer label
    AstStatement* r_cont = compound->items[1].as.stmt;
    assert(strcmp(r_cont->as.continue_stmt.label, outer_label) == 0);

    destroy_program(&prog);
    printf("  PASS: test_resolve_labels_nested_loops\n");
}

// resolve_labels propagates through if branches inside a loop.
void test_resolve_labels_through_if() {
    // while (1) if (2) break; else continue;
    AstStatement* if_stmt = make_if_stmt(
        create_int_exp(2),
        make_break_stmt(NULL),
        make_continue_stmt(NULL));

    AstProgram prog = make_test_program(
        make_while_stmt(create_int_exp(1), if_stmt));

    resolve_labels(&prog);

    AstStatement* resolved = prog.functions[0].body.items[0].as.stmt;
    char* label = resolved->as.while_loop.label;
    assert(label != NULL);

    AstStatement* r_if = resolved->as.while_loop.body;
    assert(r_if->kind == STMT_IF);
    assert(strcmp(r_if->as.if_cond.then_br->as.break_stmt.label, label) == 0);
    assert(strcmp(r_if->as.if_cond.else_br->as.continue_stmt.label, label) == 0);

    destroy_program(&prog);
    printf("  PASS: test_resolve_labels_through_if\n");
}

// resolve_labels assigns a label to a do-while loop.
void test_resolve_labels_do_while() {
    AstProgram prog = make_test_program(
        make_do_while_stmt(create_int_exp(1), make_break_stmt(NULL)));

    resolve_labels(&prog);

    AstStatement* resolved = prog.functions[0].body.items[0].as.stmt;
    assert(resolved->kind == STMT_DO_WHILE);
    assert(resolved->as.do_while_loop.label != NULL);
    assert(strcmp(resolved->as.do_while_loop.body->as.break_stmt.label,
                 resolved->as.do_while_loop.label) == 0);

    destroy_program(&prog);
    printf("  PASS: test_resolve_labels_do_while\n");
}

int main(void) {
    printf("Running parser tests...\n");
    test_create_int_exp();
    test_create_unary_exp();
    test_create_binop_exp();
    test_create_conditional_exp();
    test_create_return_stmt();
    test_create_if_stmt_with_else();
    test_create_if_stmt_no_else();
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
    test_parse_prefix_increment();
    test_parse_prefix_decrement();
    test_parse_postfix_increment();
    test_parse_postfix_decrement();
    test_parse_conditional_basic();
    test_parse_conditional_below_arithmetic();
    test_parse_conditional_middle_is_full_exp();
    test_parse_conditional_right_assoc();
    printf("Running loop parsing tests...\n");
    test_parse_while_loop();
    test_parse_do_while_loop();
    test_parse_for_loop();
    test_parse_break_in_loop();
    test_parse_continue_in_loop();
    printf("Running label resolution tests...\n");
    test_resolve_labels_while_break();
    test_resolve_labels_while_continue();
    test_resolve_labels_nested_loops();
    test_resolve_labels_through_if();
    test_resolve_labels_do_while();
    printf("All parser tests passed!\n");
    return 0;
}
