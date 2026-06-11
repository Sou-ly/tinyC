#include "parser.h"
#include <stdio.h>
#include <string.h>

// --- Utilities ---
//
static bool is_binop(Token* tok) {
	if (tok == NULL || tok->kind != TOK_OPERATOR) {
		return false;
	}
	switch (tok->op) {
		case TOK_PLUS:
		case TOK_MINUS:
    	case TOK_STAR:
    	case TOK_FSLASH:
    	case TOK_PERCENT:
		case TOK_AND:
		case TOK_OR:
		case TOK_XOR:
		case TOK_LSHIFT:
		case TOK_RSHIFT:
			return true;
		default:
			return false;
	}
}

static int precedence(Token* tok) {
	switch (tok->op) {
		case TOK_STAR:		return 50;
		case TOK_FSLASH:		return 50;
		case TOK_PERCENT:	return 50;
		case TOK_PLUS:		return 45;
		case TOK_MINUS:		return 45;
		case TOK_LSHIFT:		return 40;
		case TOK_RSHIFT:		return 40;
		case TOK_AND:		return 30;
		case TOK_XOR:		return 25;
		case TOK_OR:			return 20;
		default:			break;
	}
	fprintf(stderr, "precedence: unrecognized token operator");
	exit(1);
}

static AstBinopType tok_to_binop(TokenOperator op) {
	switch (op) {
		case TOK_PLUS:		return BINOP_ADD;
		case TOK_MINUS:		return BINOP_SUB;
    	case TOK_STAR:		return BINOP_MUL;
    	case TOK_FSLASH:		return BINOP_DIV;
    	case TOK_PERCENT:	return BINOP_MOD;
		case TOK_AND:		return BINOP_AND;
		case TOK_OR:			return BINOP_OR;
		case TOK_XOR:		return BINOP_XOR;
		case TOK_LSHIFT:		return BINOP_LSHIFT;
		case TOK_RSHIFT:		return BINOP_RSHIFT;
		default:			break;
	}
	fprintf(stderr, "binop: unrecognized token operator");
	exit(1);
}

static Token* current(Parser* p) {
    return &p->tokens->items[p->pos];
}

static Token* advance(Parser* p) {
    return &p->tokens->items[p->pos++];
}

static bool at_end(Parser* p) {
    return (size_t)p->pos >= p->tokens->count;
}

static void expect_keyword(Parser* p, TokenKeyword kw) {
    if (at_end(p) || current(p)->kind != TOK_KEYWORD || current(p)->kw != kw) {
        fprintf(stderr, "parse error at %zu:%zu: expected keyword '%s'\n",
                current(p)->line, current(p)->col, keyword_name(kw));
        exit(1);
    }
    advance(p);
}

static void expect_separator(Parser* p, TokenSeparator sep) {
    if (at_end(p) || current(p)->kind != TOK_SEPARATOR || current(p)->sep != sep) {
        fprintf(stderr, "parse error at %zu:%zu: expected '%s'\n",
                current(p)->line, current(p)->col, separator_name(sep));
        exit(1);
    }
    advance(p);
}

// --- Parsing ---

static AstExp* parse_exp(Parser* p, int min_prec);

// factor ::= int | ("!" | "-") factor | "(" exp ")"
static AstExp* parse_factor(Parser* p) {
    Token* tok = current(p);
    switch (tok->kind) {
        case TOK_INT_LITERAL: {
            int val = tok->int_val;
            advance(p);
            return create_int_exp(val);
        }
        case TOK_OPERATOR:
            if (tok->op == TOK_NOT) {
                advance(p);
                return create_unary_exp(UNOP_NOT, parse_factor(p));
            } else if (tok->op == TOK_MINUS) {
                advance(p);
                return create_unary_exp(UNOP_MINUS, parse_factor(p));
            }
            break;
        case TOK_SEPARATOR:
            if (tok->sep == TOK_LPAR) {
                advance(p);
                AstExp* exp = parse_exp(p, 0);
                expect_separator(p, TOK_RPAR);
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


static AstExp* parse_exp(Parser* p, int min_prec) {
    AstExp* lhs = parse_factor(p);
	Token* tok = current(p);
    while (is_binop(tok) && precedence(tok) >= min_prec) {
        AstBinopType op = tok_to_binop(current(p)->op);
		int prec = precedence(tok);
		advance(p);
        AstExp* rhs = parse_exp(p, prec + 1);
        lhs = create_binop_exp(op, lhs, rhs);
		tok = current(p);
    }
    return lhs;
}

static AstStatement parse_statement(Parser* p) {
    if (current(p)->kind == TOK_KEYWORD && current(p)->kw == TOK_RETURN) {
        advance(p); // consume 'return'
        AstExp* exp = parse_exp(p, 0);
        expect_separator(p, TOK_SEMICOLON);
        return make_return_stmt(exp);
    }

    fprintf(stderr, "parse error at %zu:%zu: expected statement\n",
            current(p)->line, current(p)->col);
    exit(1);
}

static AstDeclaration parse_declaration(Parser* p) {
    // expect: int <name> ( )  { ... }
    expect_keyword(p, TOK_INT);

    if (current(p)->kind != TOK_IDENTIFIER) {
        fprintf(stderr, "parse error at %zu:%zu: expected function name\n",
                current(p)->line, current(p)->col);
        exit(1);
    }
    char* name = current(p)->ident;
    advance(p);

    expect_separator(p, TOK_LPAR);
    expect_separator(p, TOK_RPAR);
    expect_separator(p, TOK_LBRACE);

    // parse body statements
    int capacity = 8;
    int count = 0;
    AstStatement* body = malloc(sizeof(AstStatement) * capacity);

    while (!at_end(p) && !(current(p)->kind == TOK_SEPARATOR && current(p)->sep == TOK_RBRACE)) {
        if (count >= capacity) {
            capacity *= 2;
            body = realloc(body, sizeof(AstStatement) * capacity);
        }
        body[count++] = parse_statement(p);
    }

    expect_separator(p, TOK_RBRACE);

    return make_function_decl(name, body, count);
}

// --- Public API ---

Parser parser_create(TokenList* tokens) {
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
