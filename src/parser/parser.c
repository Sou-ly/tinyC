#include "parser.h"
#include <stdio.h>
#include <string.h>

// --- Utilities ---

static token* current(Parser* p) {
    return &p->tokens->items[p->pos];
}

static token* advance(Parser* p) {
    return &p->tokens->items[p->pos++];
}

static bool at_end(Parser* p) {
    return (size_t)p->pos >= p->tokens->count;
}

static void expect_keyword(Parser* p, token_keyword kw) {
    if (at_end(p) || current(p)->kind != TOK_KEYWORD || current(p)->as.kw != kw) {
        fprintf(stderr, "parse error at %zu:%zu: expected keyword '%s'\n",
                current(p)->line, current(p)->col, keyword_name(kw));
        exit(1);
    }
    advance(p);
}

static void expect_separator(Parser* p, token_separator sep) {
    if (at_end(p) || current(p)->kind != TOK_SEPARATOR || current(p)->as.sep != sep) {
        fprintf(stderr, "parse error at %zu:%zu: expected '%s'\n",
                current(p)->line, current(p)->col, separator_name(sep));
        exit(1);
    }
    advance(p);
}

// --- Parsing ---

static AstExp* parse_exp(Parser* p);

// factor ::= int | ("!" | "-") factor | "(" exp ")"
static AstExp* parse_factor(Parser* p) {
    token* tok = current(p);
    switch (tok->kind) {
        case TOK_INT_LITERAL: {
            int val = tok->as.int_val;
            advance(p);
            return create_int_exp(val);
        }
        case TOK_OPERATOR:
            if (tok->as.op == OP_NOT) {
                advance(p);
                return create_unary_exp(UNOP_NOT, parse_factor(p));
            } else if (tok->as.op == OP_MINUS) {
                advance(p);
                return create_unary_exp(UNOP_MINUS, parse_factor(p));
            }
            break;
        case TOK_SEPARATOR:
            if (tok->as.sep == SEP_LPAR) {
                advance(p);
                AstExp* exp = parse_exp(p);
                expect_separator(p, SEP_RPAR);
                return exp;
            }
            break;
        default:
            break;
    }

    fprintf(stderr, "parse error at %zu:%zu: expected factor\n",
            current(p)->line, current(p)->col);
    exit(1);
}

// term ::= factor { ("*" | "/") factor }
static AstExp* parse_term(Parser* p) {
    AstExp* lhs = parse_factor(p);
    while (!at_end(p) && current(p)->kind == TOK_OPERATOR &&
           (current(p)->as.op == OP_STAR || current(p)->as.op == OP_FSLASH)) {
        AstBinopType op = current(p)->as.op == OP_STAR ? BINOP_MUL : BINOP_DIV;
        advance(p);
        AstExp* rhs = parse_factor(p);
        lhs = create_binop_exp(op, lhs, rhs);
    }
    return lhs;
}

// exp ::= term { ("+" | "-") term }
static AstExp* parse_exp(Parser* p) {
    AstExp* lhs = parse_term(p);
    while (!at_end(p) && current(p)->kind == TOK_OPERATOR &&
           (current(p)->as.op == OP_PLUS || current(p)->as.op == OP_MINUS)) {
        AstBinopType op = current(p)->as.op == OP_PLUS ? BINOP_ADD : BINOP_SUB;
        advance(p);
        AstExp* rhs = parse_term(p);
        lhs = create_binop_exp(op, lhs, rhs);
    }
    return lhs;
}

static AstStatement parse_statement(Parser* p) {
    if (current(p)->kind == TOK_KEYWORD && current(p)->as.kw == KW_RETURN) {
        advance(p); // consume 'return'
        AstExp* exp = parse_exp(p);
        expect_separator(p, SEP_SEMICOLON);
        return make_return_stmt(exp);
    }

    fprintf(stderr, "parse error at %zu:%zu: expected statement\n",
            current(p)->line, current(p)->col);
    exit(1);
}

static AstDeclaration parse_declaration(Parser* p) {
    // expect: int <name> ( )  { ... }
    expect_keyword(p, KW_INT);

    if (current(p)->kind != TOK_IDENTIFIER) {
        fprintf(stderr, "parse error at %zu:%zu: expected function name\n",
                current(p)->line, current(p)->col);
        exit(1);
    }
    char* name = current(p)->as.ident;
    advance(p);

    expect_separator(p, SEP_LPAR);
    expect_separator(p, SEP_RPAR);
    expect_separator(p, SEP_LBRACE);

    // parse body statements
    int capacity = 8;
    int count = 0;
    AstStatement* body = malloc(sizeof(AstStatement) * capacity);

    while (!at_end(p) && !(current(p)->kind == TOK_SEPARATOR && current(p)->as.sep == SEP_RBRACE)) {
        if (count >= capacity) {
            capacity *= 2;
            body = realloc(body, sizeof(AstStatement) * capacity);
        }
        body[count++] = parse_statement(p);
    }

    expect_separator(p, SEP_RBRACE);

    return make_function_decl(name, body, count);
}

// --- Public API ---

Parser parser_create(token_list* tokens) {
    return (Parser){
        .tokens = tokens,
        .pos = 0
    };
}

AstProgram parse_program(Parser* p) {
    int capacity = 4;
    int count = 0;
    AstDeclaration* decls = malloc(sizeof(AstDeclaration) * capacity);

    while (!at_end(p)) {
        if (count >= capacity) {
            capacity *= 2;
            decls = realloc(decls, sizeof(AstDeclaration) * capacity);
        }
        decls[count++] = parse_declaration(p);
    }

    return make_program(decls, count);
}
