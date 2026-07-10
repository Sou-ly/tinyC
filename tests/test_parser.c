#define _GNU_SOURCE
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include "../src/parser/ast.h"
#include "../src/parser/parser.h"
#include "../src/lexer/token.h"

// --- AST unit tests ---

void test_create_int_exp() {
    AstExp* e = ast_exp_int(42);
    assert(e != NULL);
    assert(e->kind == EXP_INT);
    assert(e->as.int_lit.value == 42);
    ast_exp_destroy(e);
    printf("  PASS: test_create_int_exp\n");
}

void test_create_unary_exp() {
    AstExp* e = ast_exp_unary(UNOP_MINUS, ast_exp_int(5));
    assert(e != NULL);
    assert(e->kind == EXP_UNOP);
    assert(e->as.unary.op_type == UNOP_MINUS);
    assert(e->as.unary.operand->as.int_lit.value == 5);
    ast_exp_destroy(e);
    printf("  PASS: test_create_unary_exp\n");
}

void test_create_binop_exp() {
    AstExp* e = ast_exp_binop(BINOP_ADD, ast_exp_int(3), ast_exp_int(7));
    assert(e != NULL);
    assert(e->kind == EXP_BINOP);
    assert(e->as.binop.op_type == BINOP_ADD);
    assert(e->as.binop.lhs->as.int_lit.value == 3);
    assert(e->as.binop.rhs->as.int_lit.value == 7);
    ast_exp_destroy(e);
    printf("  PASS: test_create_binop_exp\n");
}

void test_create_conditional_exp() {
    AstExp* e = ast_exp_conditional(ast_exp_int(1), ast_exp_int(2), ast_exp_int(3));
    assert(e != NULL);
    assert(e->kind == EXP_CONDITIONAL);
    assert(e->as.conditional.lhs->as.int_lit.value == 1);
    assert(e->as.conditional.mid->as.int_lit.value == 2);
    assert(e->as.conditional.rhs->as.int_lit.value == 3);
    ast_exp_destroy(e);
    printf("  PASS: test_create_conditional_exp\n");
}

void test_create_return_stmt() {
    AstStatement* s = ast_stmt_return(ast_exp_int(2));
    assert(s->kind == STMT_RETURN);
    assert(s->as.ret.exp->as.int_lit.value == 2);
    ast_stmt_destroy(s);
    printf("  PASS: test_create_return_stmt\n");
}

// ast_stmt_if takes heap-owned branches; make_*_stmt already heap-allocates,
// so this is just a pass-through kept for readability at the call sites.
static AstStatement* heap_stmt(AstStatement* stmt) {
    return stmt;
}

// An if with both branches records the condition and both heap-owned branches,
// and leaves the (loop/label pass) label unset.
void test_create_if_stmt_with_else() {
    AstStatement* s = ast_stmt_if(
        ast_exp_int(1),
        heap_stmt(ast_stmt_return(ast_exp_int(2))),
        heap_stmt(ast_stmt_return(ast_exp_int(3))));

    assert(s->kind == STMT_IF);
    assert(s->as.if_cond.cond->kind == EXP_INT);
    assert(s->as.if_cond.cond->as.int_lit.value == 1);
    assert(s->as.if_cond.then_br->kind == STMT_RETURN);
    assert(s->as.if_cond.then_br->as.ret.exp->as.int_lit.value == 2);
    assert(s->as.if_cond.else_br != NULL);
    assert(s->as.if_cond.else_br->kind == STMT_RETURN);
    assert(s->as.if_cond.else_br->as.ret.exp->as.int_lit.value == 3);

    ast_stmt_destroy(s);
    printf("  PASS: test_create_if_stmt_with_else\n");
}

// A plain if leaves else_br NULL; ast_stmt_destroy must tolerate the missing branch.
void test_create_if_stmt_no_else() {
    AstStatement* s = ast_stmt_if(
        ast_exp_int(1),
        heap_stmt(ast_stmt_return(ast_exp_int(2))),
        NULL);

    assert(s->kind == STMT_IF);
    assert(s->as.if_cond.then_br->kind == STMT_RETURN);
    assert(s->as.if_cond.else_br == NULL);

    ast_stmt_destroy(s);
    printf("  PASS: test_create_if_stmt_no_else\n");
}

void test_create_function_decl() {
    AstBlock body = (AstBlock){0};
    ast_block_append(&body, (AstBlockItem){
        .kind = AST_STATEMENT,
        .as.statement = ast_stmt_return(ast_exp_int(0)),
    });
    AstFunctionDeclaration fn = ast_function_declaration(
        strdup("main"), (AstParamList){0}, SOME(OptionalBlock, body));

    assert(strcmp(fn.identifier, "main") == 0);
    assert(fn.body.present);
    assert(fn.body.value.count == 1);
    assert(fn.body.value.items[0].kind == AST_STATEMENT);
    assert(fn.body.value.items[0].as.statement->kind == STMT_RETURN);
    ast_function_declaration_destroy(&fn);
    printf("  PASS: test_create_function_decl\n");
}

// --- Parser integration test ---
// Parses: int main() { return 2; }

void test_parse_return_2() {
    TokenList tokens = (TokenList){0};

    list_push(&tokens, ((Token){TOK_KEYWORD,   {.keyword = TOK_INT},          1, 1}));
    list_push(&tokens, ((Token){TOK_IDENTIFIER, {.identifier = strdup("main")}, 1, 5}));
    list_push(&tokens, ((Token){TOK_SEPARATOR, {.separator = TOK_LPAR},       1, 9}));
    list_push(&tokens, ((Token){TOK_SEPARATOR, {.separator = TOK_RPAR},       1, 10}));
    list_push(&tokens, ((Token){TOK_SEPARATOR, {.separator = TOK_LBRACE},     1, 12}));
    list_push(&tokens, ((Token){TOK_KEYWORD,   {.keyword = TOK_RETURN},       2, 5}));
    list_push(&tokens, ((Token){TOK_INT_LITERAL, {.int_value = 2},        2, 12}));
    list_push(&tokens, ((Token){TOK_SEPARATOR, {.separator = TOK_SEMICOLON},  2, 13}));
    list_push(&tokens, ((Token){TOK_SEPARATOR, {.separator = TOK_RBRACE},     3, 1}));

    Parser parser = parser_create(&tokens);
    AstProgram prog = parse_program(&parser);

    assert(prog.count == 1);
    assert(strcmp(prog.items[0].identifier, "main") == 0);
    assert(prog.items[0].body.value.count == 1);
    assert(prog.items[0].body.value.items[0].kind == AST_STATEMENT);
    assert(prog.items[0].body.value.items[0].as.statement->kind == STMT_RETURN);
    assert(prog.items[0].body.value.items[0].as.statement->as.ret.exp->kind == EXP_INT);
    assert(prog.items[0].body.value.items[0].as.statement->as.ret.exp->as.int_lit.value == 2);

    ast_program_destroy(&prog);
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
    return (Token){TOK_INT_LITERAL, {.int_value = value}, 1, 1};
}

static Token make_op_token(TokenOperator op) {
    return (Token){TOK_OPERATOR, {.operator = op}, 1, 1};
}

static Token make_ident_token(const char* name) {
    return (Token){TOK_IDENTIFIER, {.identifier = strdup(name)}, 1, 1};
}

static Token make_sep_token(TokenSeparator sep) {
    return (Token){TOK_SEPARATOR, {.separator = sep}, 1, 1};
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
    TokenList tokens = (TokenList){0};

    list_push(&tokens, ((Token){TOK_KEYWORD,    {.keyword = TOK_INT},            1, 1}));
    list_push(&tokens, ((Token){TOK_IDENTIFIER, {.identifier = strdup("main")}, 1, 1}));
    list_push(&tokens, make_sep_token(TOK_LPAR));
    list_push(&tokens, make_sep_token(TOK_RPAR));
    list_push(&tokens, make_sep_token(TOK_LBRACE));
    list_push(&tokens, ((Token){TOK_KEYWORD,    {.keyword = TOK_RETURN},         1, 1}));
    for (size_t i = 0; i < num_exp_tokens; i++) {
        list_push(&tokens, exp_tokens[i]);
    }
    list_push(&tokens, make_sep_token(TOK_SEMICOLON));
    list_push(&tokens, make_sep_token(TOK_RBRACE));

    Parser parser = parser_create(&tokens);
    AstProgram prog = parse_program(&parser);

    AstExp* actual = prog.items[0].body.value.items[0].as.statement->as.ret.exp;
    if (!exp_equals(actual, expected)) {
        printf("  FAIL: %s\n    expected: ", description);
        print_exp(expected);
        printf("\n    actual:   ");
        print_exp(actual);
        printf("\n");
        exit(1);
    }

    ast_exp_destroy(expected);
    ast_program_destroy(&prog);
    free_tokens(&tokens);
    printf("  PASS: %s\n", description);
}

// '*' binds tighter than '+', on both sides.
void test_precedence_mul_over_add() {
    Token left[] = {
        make_int_token(1), make_op_token(TOK_PLUS),
        make_int_token(2), make_op_token(TOK_STAR), make_int_token(3),
    };
    check_return_exp("1 + 2 * 3;", left, COUNT_OF(left),
        ast_exp_binop(BINOP_ADD, ast_exp_int(1),
            ast_exp_binop(BINOP_MUL, ast_exp_int(2), ast_exp_int(3))));

    Token right[] = {
        make_int_token(2), make_op_token(TOK_STAR), make_int_token(3),
        make_op_token(TOK_PLUS), make_int_token(4),
    };
    check_return_exp("2 * 3 + 4", right, COUNT_OF(right),
        ast_exp_binop(BINOP_ADD,
            ast_exp_binop(BINOP_MUL, ast_exp_int(2), ast_exp_int(3)),
            ast_exp_int(4)));
}

// Same-precedence operators associate to the left.
void test_precedence_left_associativity() {
    Token sub[] = {
        make_int_token(10), make_op_token(TOK_MINUS),
        make_int_token(4), make_op_token(TOK_MINUS), make_int_token(3),
    };
    check_return_exp("10 - 4 - 3", sub, COUNT_OF(sub),
        ast_exp_binop(BINOP_SUB,
            ast_exp_binop(BINOP_SUB, ast_exp_int(10), ast_exp_int(4)),
            ast_exp_int(3)));

    Token div_mod[] = {
        make_int_token(8), make_op_token(TOK_FSLASH),
        make_int_token(4), make_op_token(TOK_PERCENT), make_int_token(3),
    };
    check_return_exp("8 / 4 % 3", div_mod, COUNT_OF(div_mod),
        ast_exp_binop(BINOP_MOD,
            ast_exp_binop(BINOP_DIV, ast_exp_int(8), ast_exp_int(4)),
            ast_exp_int(3)));

    Token shift[] = {
        make_int_token(1), make_op_token(TOK_LSHIFT),
        make_int_token(2), make_op_token(TOK_LSHIFT), make_int_token(3),
    };
    check_return_exp("1 << 2 << 3", shift, COUNT_OF(shift),
        ast_exp_binop(BINOP_LSHIFT,
            ast_exp_binop(BINOP_LSHIFT, ast_exp_int(1), ast_exp_int(2)),
            ast_exp_int(3)));
}

// Additive binds tighter than shifts.
void test_precedence_add_over_shift() {
    Token lshift[] = {
        make_int_token(1), make_op_token(TOK_LSHIFT),
        make_int_token(2), make_op_token(TOK_PLUS), make_int_token(3),
    };
    check_return_exp("1 << 2 + 3", lshift, COUNT_OF(lshift),
        ast_exp_binop(BINOP_LSHIFT, ast_exp_int(1),
            ast_exp_binop(BINOP_ADD, ast_exp_int(2), ast_exp_int(3))));

    Token rshift[] = {
        make_int_token(1), make_op_token(TOK_PLUS),
        make_int_token(2), make_op_token(TOK_RSHIFT), make_int_token(3),
    };
    check_return_exp("1 + 2 >> 3", rshift, COUNT_OF(rshift),
        ast_exp_binop(BINOP_RSHIFT,
            ast_exp_binop(BINOP_ADD, ast_exp_int(1), ast_exp_int(2)),
            ast_exp_int(3)));
}

// Bitwise tiers: shift > & > ^ > |.
void test_precedence_bitwise_tiers() {
    Token shift_over_and[] = {
        make_int_token(1), make_op_token(TOK_AND),
        make_int_token(2), make_op_token(TOK_LSHIFT), make_int_token(3),
    };
    check_return_exp("1 & 2 << 3", shift_over_and, COUNT_OF(shift_over_and),
        ast_exp_binop(BINOP_AND, ast_exp_int(1),
            ast_exp_binop(BINOP_LSHIFT, ast_exp_int(2), ast_exp_int(3))));

    Token and_over_xor[] = {
        make_int_token(1), make_op_token(TOK_XOR),
        make_int_token(2), make_op_token(TOK_AND), make_int_token(3),
    };
    check_return_exp("1 ^ 2 & 3", and_over_xor, COUNT_OF(and_over_xor),
        ast_exp_binop(BINOP_XOR, ast_exp_int(1),
            ast_exp_binop(BINOP_AND, ast_exp_int(2), ast_exp_int(3))));

    Token xor_over_or[] = {
        make_int_token(1), make_op_token(TOK_OR),
        make_int_token(2), make_op_token(TOK_XOR), make_int_token(3),
    };
    check_return_exp("1 | 2 ^ 3", xor_over_or, COUNT_OF(xor_over_or),
        ast_exp_binop(BINOP_OR, ast_exp_int(1),
            ast_exp_binop(BINOP_XOR, ast_exp_int(2), ast_exp_int(3))));
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
        ast_exp_binop(BINOP_OR, ast_exp_int(1),
            ast_exp_binop(BINOP_XOR, ast_exp_int(2),
                ast_exp_binop(BINOP_AND, ast_exp_int(3),
                    ast_exp_binop(BINOP_LSHIFT, ast_exp_int(4),
                        ast_exp_binop(BINOP_ADD, ast_exp_int(5),
                            ast_exp_binop(BINOP_MUL, ast_exp_int(6),
                                ast_exp_int(7))))))));
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
        ast_exp_binop(BINOP_AND,
            ast_exp_binop(BINOP_OR, ast_exp_int(1), ast_exp_int(2)),
            ast_exp_int(3)));
}

// Unary minus binds tighter than any binary operator.
void test_precedence_unary_over_binary() {
    Token negated[] = {
        make_op_token(TOK_MINUS), make_int_token(1),
        make_op_token(TOK_PLUS), make_int_token(2),
    };
    check_return_exp("-1 + 2", negated, COUNT_OF(negated),
        ast_exp_binop(BINOP_ADD,
            ast_exp_unary(UNOP_MINUS, ast_exp_int(1)),
            ast_exp_int(2)));
}

// --- Compound assignment tests ---
//
// `exp_equals` deliberately ignores `assign.op`, so these tests inspect the
// parsed assignment node directly. Each parses `int main() { x <op> 5; }` and
// checks both that the node is an assignment over `x` / `5` and that the
// operator tag is mapped correctly.
static void check_assign_op(const char* description, TokenOperator tok_op,
                            AstAssignOp expected_op) {
    TokenList tokens = (TokenList){0};
    list_push(&tokens, ((Token){TOK_KEYWORD,    {.keyword = TOK_INT},            1, 1}));
    list_push(&tokens, ((Token){TOK_IDENTIFIER, {.identifier = strdup("main")}, 1, 1}));
    list_push(&tokens, make_sep_token(TOK_LPAR));
    list_push(&tokens, make_sep_token(TOK_RPAR));
    list_push(&tokens, make_sep_token(TOK_LBRACE));
    list_push(&tokens, ((Token){TOK_IDENTIFIER, {.identifier = strdup("x")}, 1, 1}));
    list_push(&tokens, make_op_token(tok_op));
    list_push(&tokens, make_int_token(5));
    list_push(&tokens, make_sep_token(TOK_SEMICOLON));
    list_push(&tokens, make_sep_token(TOK_RBRACE));

    Parser parser = parser_create(&tokens);
    AstProgram prog = parse_program(&parser);

    AstBlockItem item = prog.items[0].body.value.items[0];
    assert(item.kind == AST_STATEMENT);
    assert(item.as.statement->kind == STMT_EXP);
    AstExp* exp = item.as.statement->as.exp_stmt.exp;
    assert(exp->kind == EXP_ASSIGN);
    assert(exp->as.assign.lhs->kind == EXP_VAR);
    assert(strcmp(exp->as.assign.lhs->as.variable.identifier, "x") == 0);
    assert(exp->as.assign.rhs->kind == EXP_INT && exp->as.assign.rhs->as.int_lit.value == 5);
    if (exp->as.assign.op != expected_op) {
        printf("  FAIL: %s (expected assign op %d, got %d)\n",
               description, expected_op, exp->as.assign.op);
        exit(1);
    }

    ast_program_destroy(&prog);
    free_tokens(&tokens);
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
    TokenList tokens = (TokenList){0};
    list_push(&tokens, ((Token){TOK_KEYWORD,    {.keyword = TOK_INT},            1, 1}));
    list_push(&tokens, ((Token){TOK_IDENTIFIER, {.identifier = strdup("main")}, 1, 1}));
    list_push(&tokens, make_sep_token(TOK_LPAR));
    list_push(&tokens, make_sep_token(TOK_RPAR));
    list_push(&tokens, make_sep_token(TOK_LBRACE));
    list_push(&tokens, ((Token){TOK_IDENTIFIER, {.identifier = strdup("x")}, 1, 1}));
    list_push(&tokens, make_op_token(TOK_PLUS_EQ));
    list_push(&tokens, ((Token){TOK_IDENTIFIER, {.identifier = strdup("y")}, 1, 1}));
    list_push(&tokens, make_op_token(TOK_MUL_EQ));
    list_push(&tokens, make_int_token(5));
    list_push(&tokens, make_sep_token(TOK_SEMICOLON));
    list_push(&tokens, make_sep_token(TOK_RBRACE));

    Parser parser = parser_create(&tokens);
    AstProgram prog = parse_program(&parser);

    AstExp* outer = prog.items[0].body.value.items[0].as.statement->as.exp_stmt.exp;
    assert(outer->kind == EXP_ASSIGN && outer->as.assign.op == ASSIGN_ADD);
    assert(outer->as.assign.lhs->kind == EXP_VAR);
    assert(strcmp(outer->as.assign.lhs->as.variable.identifier, "x") == 0);

    AstExp* inner = outer->as.assign.rhs;
    assert(inner->kind == EXP_ASSIGN && inner->as.assign.op == ASSIGN_MUL);
    assert(inner->as.assign.lhs->kind == EXP_VAR);
    assert(strcmp(inner->as.assign.lhs->as.variable.identifier, "y") == 0);
    assert(inner->as.assign.rhs->kind == EXP_INT && inner->as.assign.rhs->as.int_lit.value == 5);

    ast_program_destroy(&prog);
    free_tokens(&tokens);
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
        ast_exp_unary(UNOP_PREINC, ast_exp_var("x")));
}

void test_parse_prefix_decrement() {
    Token toks[] = { make_op_token(TOK_DECR), make_ident_token("x") };
    check_return_exp("--x", toks, COUNT_OF(toks),
        ast_exp_unary(UNOP_PREDEC, ast_exp_var("x")));
}

void test_parse_postfix_increment() {
    Token toks[] = { make_ident_token("x"), make_op_token(TOK_INCR) };
    check_return_exp("x++", toks, COUNT_OF(toks),
        ast_exp_unary(UNOP_POSTINC, ast_exp_var("x")));
}

void test_parse_postfix_decrement() {
    Token toks[] = { make_ident_token("x"), make_op_token(TOK_DECR) };
    check_return_exp("x--", toks, COUNT_OF(toks),
        ast_exp_unary(UNOP_POSTDEC, ast_exp_var("x")));
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
        ast_exp_conditional(ast_exp_int(1), ast_exp_int(2), ast_exp_int(3)));
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
        ast_exp_conditional(
            ast_exp_binop(BINOP_ADD, ast_exp_int(1), ast_exp_int(2)),
            ast_exp_int(3), ast_exp_int(4)));
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
        ast_exp_conditional(ast_exp_int(1),
            ast_exp_binop(BINOP_ADD, ast_exp_int(2), ast_exp_int(3)),
            ast_exp_int(4)));
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
        ast_exp_conditional(ast_exp_int(1), ast_exp_int(2),
            ast_exp_conditional(ast_exp_int(3), ast_exp_int(4),
                ast_exp_int(5))));
}

// --- Loop parsing tests ---
//
// Each test builds a token stream for `int main() { <loop>; }`, parses it,
// and checks the resulting AST structure.

static Token make_kw_token(TokenKeyword kw) {
    return (Token){TOK_KEYWORD, {.keyword = kw}, 1, 1};
}

// Parses: int main() { while (1) 2; }
void test_parse_while_loop() {
    TokenList tokens = (TokenList){0};
    list_push(&tokens, make_kw_token(TOK_INT));
    list_push(&tokens, make_ident_token("main"));
    list_push(&tokens, make_sep_token(TOK_LPAR));
    list_push(&tokens, make_sep_token(TOK_RPAR));
    list_push(&tokens, make_sep_token(TOK_LBRACE));
    // while (1) 2;
    list_push(&tokens, make_kw_token(TOK_WHILE));
    list_push(&tokens, make_sep_token(TOK_LPAR));
    list_push(&tokens, make_int_token(1));
    list_push(&tokens, make_sep_token(TOK_RPAR));
    list_push(&tokens, make_int_token(2));
    list_push(&tokens, make_sep_token(TOK_SEMICOLON));
    list_push(&tokens, make_sep_token(TOK_RBRACE));

    Parser parser = parser_create(&tokens);
    AstProgram prog = parse_program(&parser);

    AstStatement* stmt = prog.items[0].body.value.items[0].as.statement;
    assert(stmt->kind == STMT_WHILE);
    assert(stmt->as.while_loop.label == NULL);
    assert(stmt->as.while_loop.cond->as.int_lit.value == 1);
    assert(stmt->as.while_loop.body->kind == STMT_EXP);
    assert(stmt->as.while_loop.body->as.exp_stmt.exp->as.int_lit.value == 2);

    ast_program_destroy(&prog);
    free_tokens(&tokens);
    printf("  PASS: test_parse_while_loop\n");
}

// Parses: int main() { do 1; while (2); }
void test_parse_do_while_loop() {
    TokenList tokens = (TokenList){0};
    list_push(&tokens, make_kw_token(TOK_INT));
    list_push(&tokens, make_ident_token("main"));
    list_push(&tokens, make_sep_token(TOK_LPAR));
    list_push(&tokens, make_sep_token(TOK_RPAR));
    list_push(&tokens, make_sep_token(TOK_LBRACE));
    // do 1; while (2);
    list_push(&tokens, make_kw_token(TOK_DO));
    list_push(&tokens, make_int_token(1));
    list_push(&tokens, make_sep_token(TOK_SEMICOLON));
    list_push(&tokens, make_kw_token(TOK_WHILE));
    list_push(&tokens, make_sep_token(TOK_LPAR));
    list_push(&tokens, make_int_token(2));
    list_push(&tokens, make_sep_token(TOK_RPAR));
    list_push(&tokens, make_sep_token(TOK_SEMICOLON));
    list_push(&tokens, make_sep_token(TOK_RBRACE));

    Parser parser = parser_create(&tokens);
    AstProgram prog = parse_program(&parser);

    AstStatement* stmt = prog.items[0].body.value.items[0].as.statement;
    assert(stmt->kind == STMT_DO_WHILE);
    assert(stmt->as.do_while_loop.label == NULL);
    assert(stmt->as.do_while_loop.cond->as.int_lit.value == 2);
    assert(stmt->as.do_while_loop.body->kind == STMT_EXP);

    ast_program_destroy(&prog);
    free_tokens(&tokens);
    printf("  PASS: test_parse_do_while_loop\n");
}

// Parses: int main() { for (0; 1; 2) 3; }
void test_parse_for_loop() {
    TokenList tokens = (TokenList){0};
    list_push(&tokens, make_kw_token(TOK_INT));
    list_push(&tokens, make_ident_token("main"));
    list_push(&tokens, make_sep_token(TOK_LPAR));
    list_push(&tokens, make_sep_token(TOK_RPAR));
    list_push(&tokens, make_sep_token(TOK_LBRACE));
    // for (0; 1; 2) 3;
    list_push(&tokens, make_kw_token(TOK_FOR));
    list_push(&tokens, make_sep_token(TOK_LPAR));
    list_push(&tokens, make_int_token(0));
    list_push(&tokens, make_sep_token(TOK_SEMICOLON));
    list_push(&tokens, make_int_token(1));
    list_push(&tokens, make_sep_token(TOK_SEMICOLON));
    list_push(&tokens, make_int_token(2));
    list_push(&tokens, make_sep_token(TOK_RPAR));
    list_push(&tokens, make_int_token(3));
    list_push(&tokens, make_sep_token(TOK_SEMICOLON));
    list_push(&tokens, make_sep_token(TOK_RBRACE));

    Parser parser = parser_create(&tokens);
    AstProgram prog = parse_program(&parser);

    AstStatement* stmt = prog.items[0].body.value.items[0].as.statement;
    assert(stmt->kind == STMT_FOR);
    assert(stmt->as.for_loop.label == NULL);
    assert(stmt->as.for_loop.cond);
    assert(stmt->as.for_loop.cond->as.int_lit.value == 1);
    assert(stmt->as.for_loop.post);
    assert(stmt->as.for_loop.post->as.int_lit.value == 2);
    assert(stmt->as.for_loop.body->kind == STMT_EXP);

    ast_program_destroy(&prog);
    free_tokens(&tokens);
    printf("  PASS: test_parse_for_loop\n");
}

// Parses: int main() { while (1) break; }
void test_parse_break_in_loop() {
    TokenList tokens = (TokenList){0};
    list_push(&tokens, make_kw_token(TOK_INT));
    list_push(&tokens, make_ident_token("main"));
    list_push(&tokens, make_sep_token(TOK_LPAR));
    list_push(&tokens, make_sep_token(TOK_RPAR));
    list_push(&tokens, make_sep_token(TOK_LBRACE));
    // while (1) break;
    list_push(&tokens, make_kw_token(TOK_WHILE));
    list_push(&tokens, make_sep_token(TOK_LPAR));
    list_push(&tokens, make_int_token(1));
    list_push(&tokens, make_sep_token(TOK_RPAR));
    list_push(&tokens, make_kw_token(TOK_BREAK));
    list_push(&tokens, make_sep_token(TOK_SEMICOLON));
    list_push(&tokens, make_sep_token(TOK_RBRACE));

    Parser parser = parser_create(&tokens);
    AstProgram prog = parse_program(&parser);

    AstStatement* stmt = prog.items[0].body.value.items[0].as.statement;
    assert(stmt->kind == STMT_WHILE);
    assert(stmt->as.while_loop.body->kind == STMT_BREAK);
    assert(stmt->as.while_loop.body->as.break_stmt.label == NULL);

    ast_program_destroy(&prog);
    free_tokens(&tokens);
    printf("  PASS: test_parse_break_in_loop\n");
}

// Parses: int main() { while (1) continue; }
void test_parse_continue_in_loop() {
    TokenList tokens = (TokenList){0};
    list_push(&tokens, make_kw_token(TOK_INT));
    list_push(&tokens, make_ident_token("main"));
    list_push(&tokens, make_sep_token(TOK_LPAR));
    list_push(&tokens, make_sep_token(TOK_RPAR));
    list_push(&tokens, make_sep_token(TOK_LBRACE));
    // while (1) continue;
    list_push(&tokens, make_kw_token(TOK_WHILE));
    list_push(&tokens, make_sep_token(TOK_LPAR));
    list_push(&tokens, make_int_token(1));
    list_push(&tokens, make_sep_token(TOK_RPAR));
    list_push(&tokens, make_kw_token(TOK_CONTINUE));
    list_push(&tokens, make_sep_token(TOK_SEMICOLON));
    list_push(&tokens, make_sep_token(TOK_RBRACE));

    Parser parser = parser_create(&tokens);
    AstProgram prog = parse_program(&parser);

    AstStatement* stmt = prog.items[0].body.value.items[0].as.statement;
    assert(stmt->kind == STMT_WHILE);
    assert(stmt->as.while_loop.body->kind == STMT_CONTINUE);
    assert(stmt->as.while_loop.body->as.continue_stmt.label == NULL);

    ast_program_destroy(&prog);
    free_tokens(&tokens);
    printf("  PASS: test_parse_continue_in_loop\n");
}

// --- Label resolution tests ---
//
// resolve_labels assigns unique labels to loops and propagates them to
// break/continue statements within their bodies.

// Helper: wraps a single statement in a program with one function.
static AstProgram make_test_program(AstStatement* stmt) {
    AstBlock body = (AstBlock){0};
    ast_block_append(&body, (AstBlockItem){ .kind = AST_STATEMENT, .as.statement = stmt });
    AstFunctionDeclaration* fn = malloc(sizeof(AstFunctionDeclaration));
    *fn = ast_function_declaration(strdup("main"), (AstParamList){0}, SOME(OptionalBlock, body));
    return ast_program_create(fn, 1);
}

// resolve_labels assigns a label to a while loop and its nested break.
void test_resolve_labels_while_break() {
    AstProgram prog = make_test_program(
        ast_stmt_while(ast_exp_int(1), ast_stmt_break(NULL)));

    resolve_labels(&prog);

    AstStatement* resolved = prog.items[0].body.value.items[0].as.statement;
    assert(resolved->kind == STMT_WHILE);
    assert(resolved->as.while_loop.label != NULL);
    char* loop_label = resolved->as.while_loop.label;
    assert(resolved->as.while_loop.body->kind == STMT_BREAK);
    assert(strcmp(resolved->as.while_loop.body->as.break_stmt.label, loop_label) == 0);

    ast_program_destroy(&prog);
    printf("  PASS: test_resolve_labels_while_break\n");
}

// resolve_labels assigns a label to a while loop and its nested continue.
void test_resolve_labels_while_continue() {
    AstProgram prog = make_test_program(
        ast_stmt_while(ast_exp_int(1), ast_stmt_continue(NULL)));

    resolve_labels(&prog);

    AstStatement* resolved = prog.items[0].body.value.items[0].as.statement;
    assert(resolved->as.while_loop.label != NULL);
    assert(strcmp(resolved->as.while_loop.body->as.continue_stmt.label,
                 resolved->as.while_loop.label) == 0);

    ast_program_destroy(&prog);
    printf("  PASS: test_resolve_labels_while_continue\n");
}

// resolve_labels assigns different labels to nested loops; break/continue
// in each body get the label of their enclosing loop.
void test_resolve_labels_nested_loops() {
    // inner: for (0; 1; 2) break;
    AstForInit init = ast_for_init_exp(NULL);
    AstStatement* inner = ast_stmt_for(init, NULL, NULL, ast_stmt_break(NULL));

    // outer body: { inner_loop; continue; }
    AstBlock outer_body = (AstBlock){0};
    ast_block_append(&outer_body, (AstBlockItem){ .kind = AST_STATEMENT, .as.statement = inner });
    ast_block_append(&outer_body, (AstBlockItem){ .kind = AST_STATEMENT, .as.statement = ast_stmt_continue(NULL) });

    AstProgram prog = make_test_program(
        ast_stmt_while(ast_exp_int(1), ast_stmt_compound(outer_body)));

    resolve_labels(&prog);

    AstStatement* r_outer = prog.items[0].body.value.items[0].as.statement;
    assert(r_outer->kind == STMT_WHILE);
    char* outer_label = r_outer->as.while_loop.label;
    assert(outer_label != NULL);

    AstBlock* compound = &r_outer->as.while_loop.body->as.compound;
    AstStatement* r_inner = compound->items[0].as.statement;
    assert(r_inner->kind == STMT_FOR);
    char* inner_label = r_inner->as.for_loop.label;
    assert(inner_label != NULL);

    // Labels must be different
    assert(strcmp(outer_label, inner_label) != 0);

    // Inner break gets inner label
    assert(strcmp(r_inner->as.for_loop.body->as.break_stmt.label, inner_label) == 0);

    // Outer continue gets outer label
    AstStatement* r_cont = compound->items[1].as.statement;
    assert(strcmp(r_cont->as.continue_stmt.label, outer_label) == 0);

    ast_program_destroy(&prog);
    printf("  PASS: test_resolve_labels_nested_loops\n");
}

// resolve_labels propagates through if branches inside a loop.
void test_resolve_labels_through_if() {
    // while (1) if (2) break; else continue;
    AstStatement* if_stmt = ast_stmt_if(
        ast_exp_int(2),
        ast_stmt_break(NULL),
        ast_stmt_continue(NULL));

    AstProgram prog = make_test_program(
        ast_stmt_while(ast_exp_int(1), if_stmt));

    resolve_labels(&prog);

    AstStatement* resolved = prog.items[0].body.value.items[0].as.statement;
    char* label = resolved->as.while_loop.label;
    assert(label != NULL);

    AstStatement* r_if = resolved->as.while_loop.body;
    assert(r_if->kind == STMT_IF);
    assert(strcmp(r_if->as.if_cond.then_br->as.break_stmt.label, label) == 0);
    assert(strcmp(r_if->as.if_cond.else_br->as.continue_stmt.label, label) == 0);

    ast_program_destroy(&prog);
    printf("  PASS: test_resolve_labels_through_if\n");
}

// resolve_labels assigns a label to a do-while loop.
void test_resolve_labels_do_while() {
    AstProgram prog = make_test_program(
        ast_stmt_do_while(ast_exp_int(1), ast_stmt_break(NULL)));

    resolve_labels(&prog);

    AstStatement* resolved = prog.items[0].body.value.items[0].as.statement;
    assert(resolved->kind == STMT_DO_WHILE);
    assert(resolved->as.do_while_loop.label != NULL);
    assert(strcmp(resolved->as.do_while_loop.body->as.break_stmt.label,
                 resolved->as.do_while_loop.label) == 0);

    ast_program_destroy(&prog);
    printf("  PASS: test_resolve_labels_do_while\n");
}

// --- goto / label parsing tests ---
//
// A labeled statement is `IDENT :` on its own; a goto is `goto IDENT ;`. These
// go through tokenize() -> parse_program (the parser copies identifiers, so the
// token list can be freed right after) and then inspect the resulting AST.

static AstProgram parse_src(const char* src) {
    TokenList tokens = (TokenList){0};
    assert(tokenize(src, &tokens) == ERR_OK);
    Parser parser = parser_create(&tokens);
    AstProgram prog = parse_program(&parser);
    free_tokens(&tokens);
    return prog;
}

static AstStatement* nth_stmt(AstProgram* prog, size_t i) {
    assert(prog->count == 1);
    assert(i < prog->items[0].body.value.count);
    AstBlockItem item = prog->items[0].body.value.items[i];
    assert(item.kind == AST_STATEMENT);
    return item.as.statement;
}

// `start:` is a standalone STMT_LABEL; the following `return 1;` is a separate
// statement, so the body holds two items.
void test_parse_label() {
    AstProgram prog = parse_src("int main() { start: return 1; }");
    assert(prog.items[0].body.value.count == 2);
    AstStatement* label = nth_stmt(&prog, 0);
    assert(label->kind == STMT_LABEL);
    assert(strcmp(label->as.label.identifier, "start") == 0);
    assert(nth_stmt(&prog, 1)->kind == STMT_RETURN);
    ast_program_destroy(&prog);
    printf("  PASS: test_parse_label\n");
}

// `goto end;` parses to STMT_GOTO carrying the target name.
void test_parse_goto() {
    AstProgram prog = parse_src("int main() { goto end; }");
    assert(prog.items[0].body.value.count == 1);
    AstStatement* g = nth_stmt(&prog, 0);
    assert(g->kind == STMT_GOTO);
    assert(strcmp(g->as.goto_stmt.target, "end") == 0);
    ast_program_destroy(&prog);
    printf("  PASS: test_parse_goto\n");
}

// A leading identifier NOT followed by ':' is an expression statement -- the
// label lookahead must not swallow `x = 5;`.
void test_parse_assignment_not_label() {
    AstProgram prog = parse_src("int main() { x = 5; }");
    AstStatement* s = nth_stmt(&prog, 0);
    assert(s->kind == STMT_EXP);
    assert(s->as.exp_stmt.exp->kind == EXP_ASSIGN);
    ast_program_destroy(&prog);
    printf("  PASS: test_parse_assignment_not_label\n");
}

// label followed by a goto back to it: two statements, matching names.
void test_parse_label_and_goto() {
    AstProgram prog = parse_src("int main() { loop: goto loop; }");
    assert(prog.items[0].body.value.count == 2);
    AstStatement* label = nth_stmt(&prog, 0);
    AstStatement* g = nth_stmt(&prog, 1);
    assert(label->kind == STMT_LABEL && strcmp(label->as.label.identifier, "loop") == 0);
    assert(g->kind == STMT_GOTO && strcmp(g->as.goto_stmt.target, "loop") == 0);
    ast_program_destroy(&prog);
    printf("  PASS: test_parse_label_and_goto\n");
}

// Runs parse_src in a forked child (stderr silenced) and asserts it exits
// non-zero -- i.e. the parser rejected the program. Parse errors call exit(1),
// so this must fork like the goto-error checks do.
static void expect_parse_error(const char* description, const char* src) {
    fflush(stdout);
    pid_t pid = fork();
    assert(pid >= 0);
    if (pid == 0) {
        freopen("/dev/null", "w", stderr);
        AstProgram prog = parse_src(src);  // expected to exit(1) before returning
        ast_program_destroy(&prog);
        _exit(0);                          // reached only if it wrongly succeeded
    }
    int status = 0;
    assert(waitpid(pid, &status, 0) == pid);
    if (!(WIFEXITED(status) && WEXITSTATUS(status) != 0)) {
        printf("  FAIL: %s (expected parse error, none occurred)\n", description);
        exit(1);
    }
    printf("  PASS: %s\n", description);
}

// --- switch parsing tests ---
//
// A switch parses to an ordered list of clauses (case/default) each owning the
// block-items after its colon. Order is preserved and default may sit anywhere.

// Two cases and a default, in source order: three clauses, values recorded, the
// third flagged default, and each body holds the statements up to the next label.
void test_parse_switch_basic() {
    AstProgram prog = parse_src(
        "int main() { switch (x) { case 1: y = 1; case 2: y = 2; y = 3; default: y = 9; } }");
    AstStatement* s = nth_stmt(&prog, 0);
    assert(s->kind == STMT_SWITCH);
    assert(s->as.switch_stmt.cond->kind == EXP_VAR);
    assert(s->as.switch_stmt.clauses.count == 3);

    AstSwitchClause* c = s->as.switch_stmt.clauses.items;
    assert(!c[0].is_default && c[0].value == 1 && c[0].body.count == 1);
    assert(!c[1].is_default && c[1].value == 2 && c[1].body.count == 2); // two stmts
    assert(c[2].is_default && c[2].body.count == 1);

    ast_program_destroy(&prog);
    printf("  PASS: test_parse_switch_basic\n");
}

// An empty switch body: zero clauses, NULL clause array.
void test_parse_switch_empty() {
    AstProgram prog = parse_src("int main() { switch (x) { } }");
    AstStatement* s = nth_stmt(&prog, 0);
    assert(s->kind == STMT_SWITCH);
    assert(s->as.switch_stmt.clauses.count == 0);
    assert(s->as.switch_stmt.clauses.items == NULL);
    ast_program_destroy(&prog);
    printf("  PASS: test_parse_switch_empty\n");
}

// default need not be last; its source position is preserved among the clauses.
void test_parse_switch_default_in_middle() {
    AstProgram prog = parse_src(
        "int main() { switch (x) { case 1: y = 1; default: y = 9; case 2: y = 2; } }");
    AstStatement* s = nth_stmt(&prog, 0);
    assert(s->as.switch_stmt.clauses.count == 3);
    AstSwitchClause* c = s->as.switch_stmt.clauses.items;
    assert(!c[0].is_default && c[0].value == 1);
    assert(c[1].is_default);                       // default sits in the middle
    assert(!c[2].is_default && c[2].value == 2);
    ast_program_destroy(&prog);
    printf("  PASS: test_parse_switch_default_in_middle\n");
}

// The labelling pass gives the switch a label and points a nested break at it
// (its "_break" target), while a nested continue still refers to the enclosing
// loop -- so a switch inside a while gets two different labels.
void test_resolve_labels_switch_break() {
    AstProgram prog = parse_src(
        "int main() { while (1) { switch (x) { case 1: break; default: continue; } } }");
    resolve_labels(&prog);

    AstStatement* loop = nth_stmt(&prog, 0);
    assert(loop->kind == STMT_WHILE);
    char* loop_label = loop->as.while_loop.label;

    // while body is a compound { switch ... }
    AstStatement* sw = loop->as.while_loop.body->as.compound.items[0].as.statement;
    assert(sw->kind == STMT_SWITCH);
    char* switch_label = sw->as.switch_stmt.label;
    assert(switch_label != NULL);
    assert(strcmp(switch_label, loop_label) != 0);   // distinct labels

    // break -> switch label; continue -> enclosing loop label
    AstSwitchClause* c = sw->as.switch_stmt.clauses.items;
    AstStatement* brk = c[0].body.items[0].as.statement;
    AstStatement* cont = c[1].body.items[0].as.statement;
    assert(brk->kind == STMT_BREAK && strcmp(brk->as.break_stmt.label, switch_label) == 0);
    assert(cont->kind == STMT_CONTINUE && strcmp(cont->as.continue_stmt.label, loop_label) == 0);

    ast_program_destroy(&prog);
    printf("  PASS: test_resolve_labels_switch_break\n");
}

// Malformed switches are rejected by the parser (checked in a forked child, as
// parse errors call exit(1)).
void test_parse_switch_errors() {
    expect_parse_error("case without colon",   "int main() { switch (x) { case 1 y = 1; } }");
    expect_parse_error("non-integer case",      "int main() { switch (x) { case y: y = 1; } }");
    expect_parse_error("two defaults",          "int main() { switch (x) { default: ; default: ; } }");
    expect_parse_error("stray token in body",   "int main() { switch (x) { y = 1; } }");
}

// --- goto / label resolution tests ---
//
// resolve_goto_labels renames each source label to a program-unique name and
// repoints each goto at its target's unique name, rejecting undefined targets
// and duplicate labels (checked in a forked child, like the scoping tests).

// A label and a goto to it: after resolution both carry the same unique name,
// which differs from the source name and uses the ".L" infix.
void test_resolve_goto_label_basic() {
    AstProgram prog = parse_src("int main() { start: goto start; }");
    resolve_goto_labels(&prog);
    AstStatement* label = nth_stmt(&prog, 0);
    AstStatement* g = nth_stmt(&prog, 1);
    assert(label->kind == STMT_LABEL);
    assert(g->kind == STMT_GOTO);
    assert(strcmp(label->as.label.identifier, "start") != 0);       // renamed
    assert(strstr(label->as.label.identifier, ".L") != NULL);       // unique form
    assert(strcmp(label->as.label.identifier, g->as.goto_stmt.target) == 0); // consistent
    ast_program_destroy(&prog);
    printf("  PASS: test_resolve_goto_label_basic\n");
}

// A goto may reference a label declared later in the function (forward jump).
void test_resolve_goto_forward_reference() {
    AstProgram prog = parse_src("int main() { goto end; end: return 0; }");
    resolve_goto_labels(&prog);
    AstStatement* g = nth_stmt(&prog, 0);
    AstStatement* label = nth_stmt(&prog, 1);
    assert(g->kind == STMT_GOTO && label->kind == STMT_LABEL);
    assert(strcmp(g->as.goto_stmt.target, label->as.label.identifier) == 0);
    ast_program_destroy(&prog);
    printf("  PASS: test_resolve_goto_forward_reference\n");
}

// Labels have function scope: a label inside an if-branch is collected and a
// goto elsewhere in the function resolves to it.
void test_resolve_goto_nested_in_if() {
    AstProgram prog = parse_src("int main() { goto target; if (1) target: return 0; }");
    resolve_goto_labels(&prog);
    AstStatement* g = nth_stmt(&prog, 0);
    AstStatement* if_stmt = nth_stmt(&prog, 1);
    assert(g->kind == STMT_GOTO && if_stmt->kind == STMT_IF);
    AstStatement* label = if_stmt->as.if_cond.then_br;
    assert(label->kind == STMT_LABEL);
    assert(strcmp(g->as.goto_stmt.target, label->as.label.identifier) == 0);
    ast_program_destroy(&prog);
    printf("  PASS: test_resolve_goto_nested_in_if\n");
}

// The same source label name in two functions must get distinct unique names,
// so the emitted labels do not collide.
void test_resolve_labels_unique_across_functions() {
    AstProgram prog = parse_src("int f() { done: return 0; } int g() { done: return 1; }");
    resolve_goto_labels(&prog);
    assert(prog.count == 2);
    AstStatement* l0 = prog.items[0].body.value.items[0].as.statement;
    AstStatement* l1 = prog.items[1].body.value.items[0].as.statement;
    assert(l0->kind == STMT_LABEL && l1->kind == STMT_LABEL);
    assert(strcmp(l0->as.label.identifier, l1->as.label.identifier) != 0);
    ast_program_destroy(&prog);
    printf("  PASS: test_resolve_labels_unique_across_functions\n");
}

// Runs parse + resolve_goto_labels in a forked child (stderr silenced) and
// asserts it exits non-zero, i.e. the pass rejected the program.
static void expect_goto_error(const char* description, const char* src) {
    fflush(stdout);
    pid_t pid = fork();
    assert(pid >= 0);
    if (pid == 0) {
        freopen("/dev/null", "w", stderr);
        AstProgram prog = parse_src(src);
        resolve_goto_labels(&prog);  // expected to exit(1) before returning
        ast_program_destroy(&prog);
        _exit(0);                    // reached only if it wrongly succeeded
    }
    int status = 0;
    assert(waitpid(pid, &status, 0) == pid);
    if (!(WIFEXITED(status) && WEXITSTATUS(status) != 0)) {
        printf("  FAIL: %s (expected error, none occurred)\n", description);
        exit(1);
    }
    printf("  PASS: %s\n", description);
}

void test_resolve_goto_errors() {
    expect_goto_error("goto to undefined label", "int main() { goto nowhere; }");
    expect_goto_error("duplicate label in function", "int main() { dup: dup: return 0; }");
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
    printf("Running goto/label parsing tests...\n");
    test_parse_label();
    test_parse_goto();
    test_parse_assignment_not_label();
    test_parse_label_and_goto();
    printf("Running switch parsing tests...\n");
    test_parse_switch_basic();
    test_parse_switch_empty();
    test_parse_switch_default_in_middle();
    test_resolve_labels_switch_break();
    test_parse_switch_errors();
    printf("Running goto/label resolution tests...\n");
    test_resolve_goto_label_basic();
    test_resolve_goto_forward_reference();
    test_resolve_goto_nested_in_if();
    test_resolve_labels_unique_across_functions();
    test_resolve_goto_errors();
    printf("All parser tests passed!\n");
    return 0;
}
