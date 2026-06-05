#include "parser.h"
#include <stdio.h>
#include <string.h>

// --- Utilities ---

static token* current(Parser* p) {
    return &p->tokens[p->pos];
}

static token* advance(Parser* p) {
    return &p->tokens[p->pos++];
}

static bool at_end(Parser* p) {
    return p->pos >= p->num_tokens;
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

static AstExpression* parse_expression(Parser* p) {
    token* tok = current(p);
    switch (tok->kind) {
        case TOK_INT_LITERAL: {
            int val = tok->as.int_val;
            advance(p);
            return create_int_expr(val);
        }
        case TOK_OPERATOR:
            if (tok->as.op == OP_NOT) {
                advance(p);
                return create_unary_expr(UNARY_NOT, parse_expression(p));
            } else if (tok->as.op == OP_MINUS) {
                advance(p);
                return create_unary_expr(UNARY_MINUS, parse_expression(p));
            }
            break;
        case TOK_SEPARATOR:
            if (tok->as.sep == SEP_LPAR) {
                advance(p);
                AstExpression* expr = parse_expression(p);
                expect_separator(p, SEP_RPAR);
                return expr;
            }
            break;
        default:
            break;
    }

    fprintf(stderr, "parse error at %zu:%zu: expected expression\n",
            current(p)->line, current(p)->col);
    exit(1);
}

static AstStatement* parse_statement(Parser* p) {
    if (current(p)->kind == TOK_KEYWORD && current(p)->as.kw == KW_RETURN) {
        advance(p); // consume 'return'
        AstExpression* expr = parse_expression(p);
        expect_separator(p, SEP_SEMICOLON);
        return create_return_stmt(expr);
    }

    fprintf(stderr, "parse error at %zu:%zu: expected statement\n",
            current(p)->line, current(p)->col);
    exit(1);
}

static AstDeclaration* parse_declaration(Parser* p) {
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
    AstStatement** body = malloc(sizeof(AstStatement*) * capacity);

    while (!at_end(p) && !(current(p)->kind == TOK_SEPARATOR && current(p)->as.sep == SEP_RBRACE)) {
        if (count >= capacity) {
            capacity *= 2;
            body = realloc(body, sizeof(AstStatement*) * capacity);
        }
        body[count++] = parse_statement(p);
    }

    expect_separator(p, SEP_RBRACE);

    return create_function_decl(name, body, count);
}

// --- Public API ---

Parser parser_create(token_list* tokens) {
    return (Parser){
        .tokens = tokens->items,
        .num_tokens = (int)tokens->count,
        .pos = 0
    };
}

AstProgram* parse_program(Parser* p) {
    int capacity = 4;
    int count = 0;
    AstDeclaration** decls = malloc(sizeof(AstDeclaration*) * capacity);

    while (!at_end(p)) {
        if (count >= capacity) {
            capacity *= 2;
            decls = realloc(decls, sizeof(AstDeclaration*) * capacity);
        }
        decls[count++] = parse_declaration(p);
    }

    return create_program(decls, count);
}
