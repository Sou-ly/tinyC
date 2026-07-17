#include "parser.h"
#include "../common/ice.h"
#include <stdio.h>
#include <string.h>

// --- Utilities ---
//
static bool is_binop(Token* tok) {
	if (tok == NULL || tok->kind != TOK_OPERATOR) {
		return false;
	}
	switch (tok->as.operator) {
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
		case TOK_LAND:
		case TOK_LOR:
		case TOK_EQ:
		case TOK_NEQ:
		case TOK_LESS:
		case TOK_GREATER:
		case TOK_LEQ:
		case TOK_GEQ:
		case TOK_ASSIGN:
		case TOK_PLUS_EQ:
		case TOK_MINUS_EQ:
		case TOK_MUL_EQ:
		case TOK_DIV_EQ:
		case TOK_MOD_EQ:
		case TOK_AND_EQ:
		case TOK_OR_EQ:
		case TOK_XOR_EQ:
		case TOK_RSHIFT_EQ:
		case TOK_LSHIFT_EQ:
		case TOK_QUESTION_MARK:
			return true;
		default:
			return false;
	}
}

static int precedence(Token* tok) {
	switch (tok->as.operator) {
		case TOK_STAR:		return 50;
		case TOK_FSLASH:	return 50;
		case TOK_PERCENT:	return 50;
		case TOK_PLUS:		return 45;
		case TOK_MINUS:		return 45;
		case TOK_LSHIFT:	return 40;
		case TOK_RSHIFT:	return 40;
		case TOK_LESS:		return 35;
		case TOK_GREATER:	return 35;
		case TOK_LEQ:		return 35;
		case TOK_GEQ:		return 35;
		case TOK_EQ:		return 30;
		case TOK_NEQ:		return 30;
		case TOK_AND:		return 25;
		case TOK_XOR:		return 20;
		case TOK_OR:		return 15;
		case TOK_LAND:		return 10;
		case TOK_LOR:		return 5;
		case TOK_ASSIGN:	return 1;
		case TOK_PLUS_EQ:	return 1;
		case TOK_MINUS_EQ:	return 1;
		case TOK_MUL_EQ:	return 1;
		case TOK_DIV_EQ:	return 1;
		case TOK_MOD_EQ:	return 1;
		case TOK_AND_EQ:	return 1;
		case TOK_OR_EQ:		return 1;
		case TOK_XOR_EQ:	return 1;
		case TOK_RSHIFT_EQ:	return 1;
		case TOK_LSHIFT_EQ:	return 1;
		case TOK_QUESTION_MARK:	return 3;
		// not binary operators: no precedence. Listed explicitly (rather than a
		// `default`) so -Wswitch flags any new operator left without one.
		case TOK_DECR:
		case TOK_INCR:
		case TOK_NOT:
		case TOK_LNOT:
			break;
	}
	ICE("precedence: unrecognized token operator");
}

static AstBinopType tok_to_binop(TokenOperator op) {
	switch (op) {
		case TOK_PLUS:		return BINOP_ADD;
		case TOK_MINUS:		return BINOP_SUB;
    	case TOK_STAR:		return BINOP_MUL;
    	case TOK_FSLASH:	return BINOP_DIV;
    	case TOK_PERCENT:	return BINOP_MOD;
		case TOK_AND:		return BINOP_AND;
		case TOK_OR:		return BINOP_OR;
		case TOK_XOR:		return BINOP_XOR;
		case TOK_LSHIFT:	return BINOP_LSHIFT;
		case TOK_RSHIFT:	return BINOP_RSHIFT;
		case TOK_LAND:		return BINOP_LAND;
		case TOK_LOR:		return BINOP_LOR;
		case TOK_EQ:		return BINOP_EQ;
		case TOK_NEQ:		return BINOP_NEQ;
		case TOK_LESS:		return BINOP_LESS;
		case TOK_GREATER:	return BINOP_GREATER;
		case TOK_LEQ:		return BINOP_LEQ;
		case TOK_GEQ:		return BINOP_GEQ;
		case TOK_ASSIGN:	return BINOP_ASSIGN;
		case TOK_PLUS_EQ:	return BINOP_ASSIGN;
		case TOK_MINUS_EQ:	return BINOP_ASSIGN;
		case TOK_MUL_EQ:	return BINOP_ASSIGN;
		case TOK_DIV_EQ:	return BINOP_ASSIGN;
		case TOK_MOD_EQ:	return BINOP_ASSIGN;
		case TOK_AND_EQ:	return BINOP_ASSIGN;
		case TOK_OR_EQ:		return BINOP_ASSIGN;
		case TOK_XOR_EQ:	return BINOP_ASSIGN;
		case TOK_RSHIFT_EQ:	return BINOP_ASSIGN;
		case TOK_LSHIFT_EQ:	return BINOP_ASSIGN;
		case TOK_QUESTION_MARK:	return BINOP_CONDITION;
		// not binary operators. Listed explicitly (rather than a `default`) so
		// -Wswitch flags any new operator left without a mapping.
		case TOK_DECR:
		case TOK_INCR:
		case TOK_NOT:
		case TOK_LNOT:
			break;
	}
	ICE("binop: unrecognized token operator");
}

static AstUnopType tok_to_unop(TokenOperator op) {
	switch (op) {
		case TOK_NOT:		return UNOP_COMP;
		case TOK_MINUS:		return UNOP_MINUS;
		case TOK_LNOT:		return UNOP_NOT;
		default:			break;
	}
	ICE("unop: unrecognized token operator");
}

static Token* current(Parser* p) {
    return &p->tokens->items[p->pos];
}

// Look ahead `offset` tokens without consuming; NULL past the end. Used to
// distinguish a labeled statement (IDENT ':') from an expression statement.
static Token* peek(Parser* p, int offset) {
    size_t idx = (size_t)(p->pos + offset);
    if (idx >= p->tokens->count) return NULL;
    return &p->tokens->items[idx];
}

static Token* advance(Parser* p) {
    return &p->tokens->items[p->pos++];
}

static bool at_end(Parser* p) {
    return (size_t)p->pos >= p->tokens->count;
}

static void expect_keyword(Parser* p, TokenKeyword kw) {
    if (at_end(p) || current(p)->kind != TOK_KEYWORD || current(p)->as.keyword != kw) {
        fprintf(stderr, "parse error at %zu:%zu: expected keyword '%s'\n",
                current(p)->line, current(p)->col, keyword_name(kw));
        exit(1);
    }
    advance(p);
}

static void expect_separator(Parser* p, TokenSeparator sep) {
    if (at_end(p) || current(p)->kind != TOK_SEPARATOR || current(p)->as.separator != sep) {
        fprintf(stderr, "parse error at %zu:%zu: expected '%s'\n",
                current(p)->line, current(p)->col, separator_name(sep));
        exit(1);
    }
    advance(p);
}

// --- Parsing ---

static AstExp* parse_expression(Parser* p, int min_prec);
static AstDeclaration parse_declaration(Parser* p);
static AstVariableDeclaration parse_variable_declaration(Parser* p);
static AstFunctionDeclaration parse_function_declaration_tail(Parser* p, char* identifier);
static AstBlockItem parse_block_item(Parser* p);
static AstBlock parse_block(Parser* p);

// True when the cursor sits on a token that ends a switch clause body: the next
// `case`/`default` label, or the closing `}` of the switch.
static bool at_switch_clause_end(Parser* p) {
	if (at_end(p)) return true;
	Token* tok = current(p);
	if (tok->kind == TOK_SEPARATOR && tok->as.separator == TOK_RBRACE) return true;
	if (tok->kind == TOK_KEYWORD && (tok->as.keyword == TOK_CASE || tok->as.keyword == TOK_DEFAULT)) return true;
	return false;
}

// Parses the block items that make up one case/default clause body: everything
// up to the next label or the switch's closing brace. No braces of its own.
static AstBlock parse_switch_clause_body(Parser* p) {
	AstBlock block = (AstBlock){0};
	while (!at_switch_clause_end(p)) {
		ast_block_append(&block, parse_block_item(p));
	}
	return block;
}

// factor ::= int | ("!" | "-") factor | "(" exp ")"  | "++"id | "--"id | id"++" | id"--" | id"("")"
static AstExp* parse_factor(Parser* p) {
    Token* tok = current(p);
    switch (tok->kind) {
        case TOK_INT_LITERAL: {
            int val = tok->as.int_value;
            advance(p);
            return ast_exp_int(val);
        }
        case TOK_OPERATOR:
            advance(p);
			// TODO: should probably have some check on lvalue instead... need to check C standard
			if ((tok->as.operator == TOK_INCR || tok->as.operator == TOK_DECR) && current(p)->kind == TOK_IDENTIFIER) {
				AstExp* exp = ast_exp_var(current(p)->as.identifier);
				advance(p);
				return ast_exp_unary(tok->as.operator == TOK_INCR? UNOP_PREINC : UNOP_PREDEC, exp);
			} else {
				return ast_exp_unary(tok_to_unop(tok->as.operator), parse_factor(p));
			}
		case TOK_SEPARATOR:
            if (tok->as.separator == TOK_LPAR) {
                advance(p);
                AstExp* exp = parse_expression(p, 0);
                expect_separator(p, TOK_RPAR);
                return exp;
            }
            break;
		case TOK_IDENTIFIER: {
            char* identifier = strdup(current(p)->as.identifier);
			advance(p);
			if (current(p)->kind == TOK_OPERATOR && (current(p)->as.operator == TOK_INCR || current(p)->as.operator == TOK_DECR)) {
				TokenOperator postfix = current(p)->as.operator;
				advance(p);
				return ast_exp_unary(postfix == TOK_INCR? UNOP_POSTINC : UNOP_POSTDEC, ast_exp_var(identifier));
			} else if (current(p)->kind == TOK_SEPARATOR && current(p)->as.separator == TOK_LPAR) {
                advance(p);
                AstArgList args = {0};
                if (!(current(p)->kind == TOK_SEPARATOR && current(p)->as.separator == TOK_RPAR)) {
                    AstExp* exp = parse_expression(p, 0);
                    list_push(&args, *exp);
                    free(exp);
                    while (current(p)->kind == TOK_SEPARATOR && current(p)->as.separator == TOK_COMMA) {
                        advance(p);
                        exp = parse_expression(p, 0);
                        list_push(&args, *exp);
                        free(exp);
                    }
                }
                expect_separator(p, TOK_RPAR);
                return ast_exp_function_call(identifier, args);
            } else {
				return ast_exp_var(identifier);
			}
		}
        default:
            break;
    }

    fprintf(stderr, "parse error at %zu:%zu: expected factor\n",
            current(p)->line, current(p)->col);
    exit(1);
}

static  AstAssignOp tok_to_assign_op(TokenOperator tok) {
	switch (tok) {
		case TOK_ASSIGN:	return ASSIGN_NOP;
		case TOK_PLUS_EQ:	return ASSIGN_ADD;
		case TOK_MINUS_EQ:	return ASSIGN_SUB;
		case TOK_MUL_EQ:	return ASSIGN_MUL;
		case TOK_DIV_EQ:	return ASSIGN_DIV;
		case TOK_MOD_EQ:	return ASSIGN_MOD;
		case TOK_AND_EQ:	return ASSIGN_AND;
		case TOK_OR_EQ:		return ASSIGN_OR;
		case TOK_XOR_EQ:	return ASSIGN_XOR;
		case TOK_RSHIFT_EQ:	return ASSIGN_RSHIFT;
		case TOK_LSHIFT_EQ:	return ASSIGN_LSHIFT;
		default:			break;
	}
    fprintf(stderr, "parse error expected assignment token\n");
    exit(1);
}

static AstExp* parse_expression(Parser* p, int min_prec) {
    AstExp* lhs = parse_factor(p);
	Token* tok = current(p);
    while (is_binop(tok) && precedence(tok) >= min_prec) {
        AstBinopType op = tok_to_binop(current(p)->as.operator);
		int prec = precedence(tok);
		advance(p);
		AstExp* rhs;
		if (op == BINOP_ASSIGN) {
			rhs = parse_expression(p, prec);
			lhs = ast_exp_assign(tok_to_assign_op(tok->as.operator), lhs, rhs);
		} else if (op == BINOP_CONDITION) {
			AstExp* mid = parse_expression(p, 0);
			expect_separator(p, TOK_COLON);
			rhs = parse_expression(p, prec);
			lhs = ast_exp_conditional(lhs, mid, rhs);
		} else {
			rhs = parse_expression(p, prec + 1);
			lhs = ast_exp_binop(op, lhs, rhs);
		}
		tok = current(p);
    }
    return lhs;
}

static AstStatement* parse_statement(Parser* p) {
    // labeled statement: IDENT ':'. Two-token lookahead separates it from an
    // expression statement that merely starts with an identifier (e.g. `x = 1;`).
    Token* next = peek(p, 1);
    if (current(p)->kind == TOK_IDENTIFIER &&
            next != NULL && next->kind == TOK_SEPARATOR && next->as.separator == TOK_COLON) {
        char* identifier = strdup(current(p)->as.identifier);
        advance(p); // identifier
        advance(p); // ':'
        return ast_stmt_label(identifier);
    }

    // compound statement: a `{ ... }` block used where a statement is expected
    // (e.g. a loop/if body with several statements). parse_block owns the braces.
    if (current(p)->kind == TOK_SEPARATOR && current(p)->as.separator == TOK_LBRACE) {
        return ast_stmt_compound(parse_block(p));
    }

    if (current(p)->kind != TOK_KEYWORD) {
		AstExp* exp = parse_expression(p, 0);
    	expect_separator(p, TOK_SEMICOLON);
    	return ast_stmt_exp(exp);
    }

	switch(current(p)->as.keyword) {
		case TOK_RETURN: {
			advance(p); // consume 'return'
    		AstExp* exp = parse_expression(p, 0);
    		expect_separator(p, TOK_SEMICOLON);
    		return ast_stmt_return(exp);
		}
		case TOK_IF: {
			advance(p);
			expect_separator(p, TOK_LPAR);
			AstExp* cond = parse_expression(p, 0);
			expect_separator(p, TOK_RPAR);
			AstStatement* then_br = parse_statement(p);
			AstStatement* else_br = NULL;
			if (current(p)->kind == TOK_KEYWORD && current(p)->as.keyword == TOK_ELSE) {
				advance(p);
				else_br = parse_statement(p);
			}
			return ast_stmt_if(cond, then_br, else_br);
		}
		case TOK_FOR: {
			advance(p);
			expect_separator(p, TOK_LPAR);
			AstForInit for_init;
			if (current(p)->kind == TOK_KEYWORD && current(p)->as.keyword == TOK_INT) {
				for_init = ast_for_init_decl(parse_variable_declaration(p));
			} else if (current(p)->kind == TOK_SEPARATOR && current(p)->as.separator == TOK_SEMICOLON) {
				advance(p);	// `for (;` -- no initializer
				for_init = ast_for_init_exp(NULL);
			} else {
				AstExp* exp = parse_expression(p, 0);
				expect_separator(p, TOK_SEMICOLON);
				for_init = ast_for_init_exp(exp);
			}
			AstExp* cond = NULL;
			if (!(current(p)->kind == TOK_SEPARATOR && current(p)->as.separator == TOK_SEMICOLON))
				cond = parse_expression(p, 0);
			expect_separator(p, TOK_SEMICOLON);
			AstExp* post = NULL;
			if (!(current(p)->kind == TOK_SEPARATOR && current(p)->as.separator == TOK_RPAR))
				post = parse_expression(p, 0);
			expect_separator(p, TOK_RPAR);
			AstStatement* body = parse_statement(p);
			return ast_stmt_for(for_init, cond, post, body);
		}
		case TOK_WHILE: {
			advance(p);
			expect_separator(p, TOK_LPAR);
			AstExp* cond = parse_expression(p, 0);
			expect_separator(p, TOK_RPAR);
			AstStatement* body = parse_statement(p);
			return ast_stmt_while(cond, body);
		}
		case TOK_DO: {
			advance(p);
			AstStatement* body = parse_statement(p);
			expect_keyword(p, TOK_WHILE);
			expect_separator(p, TOK_LPAR);
			AstExp* cond = parse_expression(p, 0);
			expect_separator(p, TOK_RPAR);
			expect_separator(p, TOK_SEMICOLON);
			return ast_stmt_do_while(cond, body);
		}
		case TOK_ELSE:
			fprintf(stderr, "Missing if statement\n");
			exit(1);
		case TOK_CONTINUE:
			advance(p);
			expect_separator(p, TOK_SEMICOLON);
			return ast_stmt_continue(NULL); // label assigned by loop-labelling pass
		case TOK_BREAK:
			advance(p);
			expect_separator(p, TOK_SEMICOLON);
			return ast_stmt_break(NULL); // label assigned by loop-labelling pass
		case TOK_GOTO: {
			advance(p); // consume 'goto'
			if (current(p)->kind != TOK_IDENTIFIER) {
				fprintf(stderr, "parse error at %zu:%zu: expected label after 'goto'\n",
						current(p)->line, current(p)->col);
				exit(1);
			}
			char* target = strdup(current(p)->as.identifier);
			advance(p); // consume target
			expect_separator(p, TOK_SEMICOLON);
			return ast_stmt_goto(target);
		}
		case TOK_SWITCH: {
			advance(p); // consume 'switch'
			expect_separator(p, TOK_LPAR);
			AstExp* cond = parse_expression(p, 0);
			expect_separator(p, TOK_RPAR);
			expect_separator(p, TOK_LBRACE);

			// Clauses are kept in source order so codegen can lay their bodies
			// out contiguously (C fallthrough) and place `default` wherever it
			// appears. A second `default` is a parse error.
			AstClauseList clauses = {0};
			bool have_default = false;

			while (!at_end(p) && !(current(p)->kind == TOK_SEPARATOR && current(p)->as.separator == TOK_RBRACE)) {
				AstSwitchClause clause = { .is_default = false, .value = 0 };
				if (current(p)->kind == TOK_KEYWORD && current(p)->as.keyword == TOK_CASE) {
					advance(p); // consume 'case'
					if (current(p)->kind != TOK_INT_LITERAL) {
						fprintf(stderr, "parse error at %zu:%zu: expected integer literal after 'case'\n",
								current(p)->line, current(p)->col);
						exit(1);
					}
					clause.value = current(p)->as.int_value;
					advance(p); // consume the literal
				} else if (current(p)->kind == TOK_KEYWORD && current(p)->as.keyword == TOK_DEFAULT) {
					advance(p); // consume 'default'
					if (have_default) {
						fprintf(stderr, "parse error at %zu:%zu: multiple 'default' labels in one switch\n",
								current(p)->line, current(p)->col);
						exit(1);
					}
					clause.is_default = true;
					have_default = true;
				} else {
					fprintf(stderr, "parse error at %zu:%zu: expected 'case', 'default', or '}' in switch body\n",
							current(p)->line, current(p)->col);
					exit(1);
				}
				expect_separator(p, TOK_COLON);
				clause.body = parse_switch_clause_body(p);
				list_push(&clauses, clause);
			}
			expect_separator(p, TOK_RBRACE);
			return ast_stmt_switch(cond, clauses);
		}
		default:
			break;
	}
	fprintf(stderr, "parse error at %zu:%zu: unexpected keyword in statement\n",
			current(p)->line, current(p)->col);
	exit(1);
}

// Consumes `int <name>` and dispatches on the next token: '(' starts a function
// declaration, everything else is a variable declaration.
static AstDeclaration parse_declaration(Parser* p) {
	if (!(current(p)->kind == TOK_KEYWORD && current(p)->as.keyword == TOK_INT)) {
		fprintf(stderr, "parse error at %zu:%zu: expected declaration\n",
				current(p)->line, current(p)->col);
		exit(1);
	}
	advance(p); // consume int
	if (current(p)->kind != TOK_IDENTIFIER) {
		fprintf(stderr, "parse error at %zu:%zu: expected declared name\n",
				current(p)->line, current(p)->col);
		exit(1);
	}
	char* identifier = strdup(current(p)->as.identifier);
	advance(p);

	if (current(p)->kind == TOK_SEPARATOR && current(p)->as.separator == TOK_LPAR)
		return ast_declaration_function(parse_function_declaration_tail(p, identifier));

	AstExp* init = NULL;
	if (current(p)->kind == TOK_OPERATOR && current(p)->as.operator == TOK_ASSIGN) {
		advance(p); // consume '='
		init = parse_expression(p, 0);
	}
	expect_separator(p, TOK_SEMICOLON);
	return ast_declaration_variable(ast_variable_declaration(identifier, init));
}

// for-init admits a variable declaration only: `for (int f(void); ...)` is not C.
static AstVariableDeclaration parse_variable_declaration(Parser* p) {
	AstDeclaration declaration = parse_declaration(p);
	if (declaration.kind != DECL_VAR) {
		fprintf(stderr, "parse error at %zu:%zu: expected variable declaration\n",
				current(p)->line, current(p)->col);
		exit(1);
	}
	return declaration.as.variable;
}

static AstBlockItem parse_block_item(Parser* p) {
	AstBlockItem block_item;
	if (current(p)->kind == TOK_KEYWORD && current(p)->as.keyword == TOK_INT) {
		block_item.kind = AST_DECLARATION;
		block_item.as.declaration = parse_declaration(p);
	} else {
		block_item.kind = AST_STATEMENT;
		block_item.as.statement = parse_statement(p);
	}
	return block_item;
}

static AstBlock parse_block(Parser* p) {
    expect_separator(p, TOK_LBRACE);
	AstBlock block = (AstBlock){0};
    while (!at_end(p) && !(current(p)->kind == TOK_SEPARATOR && current(p)->as.separator == TOK_RBRACE)) {
		AstBlockItem block_item = parse_block_item(p);
		ast_block_append(&block, block_item);
    }
    expect_separator(p, TOK_RBRACE);
    return block;
}

// Picks up after `int <name>`, at '('. 
static AstFunctionDeclaration parse_function_declaration_tail(Parser* p, char* identifier) {
	expect_separator(p, TOK_LPAR);
	AstParamList params = {0};
	// A parameter may go unnamed in a prototype but not in a definition, so the
	// first one is remembered here and reported once the body is known.
	bool has_unnamed = false;
	size_t unnamed_line = 0, unnamed_col = 0;
	while (!at_end(p) && current(p)->kind == TOK_KEYWORD) {
		if (current(p)->as.keyword == TOK_VOID) {
			advance(p);
			break;
		}
		advance(p);
		if (current(p)->kind == TOK_IDENTIFIER) {
			list_push(&params, strdup(current(p)->as.identifier));
			advance(p);
		} else {
			list_push(&params, NULL);
			if (!has_unnamed) {
				has_unnamed = true;
				unnamed_line = current(p)->line;
				unnamed_col = current(p)->col;
			}
		}
		if (!(current(p)->kind == TOK_SEPARATOR && current(p)->as.separator == TOK_COMMA)) {
			break;
		}
		expect_separator(p, TOK_COMMA);
		// a ',' commits to another parameter: `int f(int a,)` is not C.
		if (at_end(p)) {
			fprintf(stderr, "parse error: unexpected end of input after ','\n");
			exit(1);
		}
		if (current(p)->kind != TOK_KEYWORD) {
			fprintf(stderr, "parse error at %zu:%zu: expected parameter after ','\n",
					current(p)->line, current(p)->col);
			exit(1);
		}
	}
	expect_separator(p, TOK_RPAR);

	if (current(p)->kind == TOK_SEPARATOR && current(p)->as.separator == TOK_SEMICOLON) {
		advance(p);
		return ast_function_declaration(identifier, params, NONE(OptionalBlock));
	}
	if (has_unnamed) {
		fprintf(stderr, "parse error at %zu:%zu: unnamed parameter in definition of '%s'\n",
				unnamed_line, unnamed_col, identifier);
		exit(1);
	}
	return ast_function_declaration(identifier, params, SOME(OptionalBlock, parse_block(p)));
}

static AstFunctionDeclaration parse_function(Parser* p) {
	AstDeclaration declaration = parse_declaration(p);
	if (declaration.kind != DECL_FUNC) {
		fprintf(stderr, "parse error at %zu:%zu: expected function declaration\n",
				current(p)->line, current(p)->col);
		exit(1);
	}
	return declaration.as.function;
}

// --- Public API ---

Parser parser_create(TokenList* tokens) {
    return (Parser){
        .tokens = tokens,
        .pos = 0
    };
}

AstProgram parse_program(Parser* p) {
    AstProgram program = {0};
    while (!at_end(p)) {
        list_push(&program, parse_function(p));
    }
    return program;
}

// --- Variable resolution ---
//
// After parsing, every variable still carries its original source name.
// The resolution pass renames each declaration to a unique name (var.0,
// var.1, …) and rewrites every reference to use the new name.  It also
// catches duplicate declarations and undeclared variables.

static int RESOLVE_COUNTER = 0;

static char* make_unique_name(const char* original) {
	int len = snprintf(NULL, 0, "%s.%d", original, RESOLVE_COUNTER);
	char* name = malloc(len + 1);
	snprintf(name, len + 1, "%s.%d", original, RESOLVE_COUNTER++);
	return name;
}

VarMap varmap_create(int capacity) {
	return (VarMap){
		.entries = malloc(sizeof(VarMapEntry) * capacity),
		.size = 0,
		.capacity = capacity,
	};
}

VarMap varmap_copy(VarMap map) {
	VarMap copy = varmap_create(map.capacity);
	for (int i = 0; i < map.size; i++) {
		// entries inherited from an enclosing scope: a declaration with
		// the same name in the new (inner) scope is shadowing, not a dup
		copy.entries[i] = (VarMapEntry){
			.key = strdup(map.entries[i].key),
			.val = strdup(map.entries[i].val),
			.is_cur_scope = false,
		};
	}
	copy.size = map.size;
	return copy;
}

void varmap_destroy(VarMap* map) {
	for (int i = 0; i < map->size; i++) {
		free(map->entries[i].key);
		free(map->entries[i].val);
	}
	free(map->entries);
}

VarMapEntry* varmap_get(VarMap* map, const char* key) {
	// search backwards so later entries shadow earlier ones
	for (int i = map->size - 1; i >= 0; i--) {
		if (strcmp(map->entries[i].key, key) == 0) {
			return &map->entries[i];
		}
	}
	return NULL;
}

void varmap_put(VarMap* map, VarMapEntry entry) {
	if (map->size == map->capacity) {
		map->capacity *= 2;
		map->entries = realloc(map->entries, sizeof(VarMapEntry) * map->capacity);
	}
	map->entries[map->size] = entry;
	map->size++;
}

static AstExp* resolve_expression(AstExp* exp, VarMap* map) {
	if (!exp) return NULL;
	switch (exp->kind) {
		case EXP_INT:
			return exp;
		case EXP_VAR: {
			VarMapEntry* resolved = varmap_get(map, exp->as.variable.identifier);
			if (!resolved) {
				fprintf(stderr, "error: undeclared variable '%s'\n",
						exp->as.variable.identifier);
				exit(1);
			}
			free(exp->as.variable.identifier);
			exp->as.variable.identifier = strdup(resolved->val);
			return exp;
		}
		case EXP_ASSIGN: {
			if (exp->as.assign.lhs->kind != EXP_VAR) {
				fprintf(stderr, "error: invalid lvalue in assignment\n");
				exit(1);
			}
			exp->as.assign.lhs = resolve_expression(exp->as.assign.lhs, map);
			exp->as.assign.rhs = resolve_expression(exp->as.assign.rhs, map);
			return exp;
		}
		case EXP_UNOP:
			exp->as.unary.operand = resolve_expression(exp->as.unary.operand, map);
			return exp;
		case EXP_BINOP:
			exp->as.binop.lhs = resolve_expression(exp->as.binop.lhs, map);
			exp->as.binop.rhs = resolve_expression(exp->as.binop.rhs, map);
			return exp;
		case EXP_CONDITIONAL:
			exp->as.conditional.lhs = resolve_expression(exp->as.conditional.lhs, map);
			exp->as.conditional.mid = resolve_expression(exp->as.conditional.mid, map);
			exp->as.conditional.rhs = resolve_expression(exp->as.conditional.rhs, map);
			return exp;
		case EXP_FUNCTION_CALL: {
			// The callee resolves against the same map as variables; a top-level
			// function is present there mapped to its own name (external linkage,
			// so it is not renamed).
			VarMapEntry* resolved = varmap_get(map, exp->as.funcall.identifier);
			if (!resolved) {
				fprintf(stderr, "error: undeclared function '%s'\n",
						exp->as.funcall.identifier);
				exit(1);
			}
			free(exp->as.funcall.identifier);
			exp->as.funcall.identifier = strdup(resolved->val);
			for (size_t i = 0; i < exp->as.funcall.args.count; i++)
				resolve_expression(&exp->as.funcall.args.items[i], map);
			return exp;
		}
	}
	return exp;
}

static void resolve_declaration(AstVariableDeclaration* decl, VarMap* map) {
	for (int i = 0; i < map->size; i++) {
		if (map->entries[i].is_cur_scope &&
				strcmp(map->entries[i].key, decl->identifier) == 0) {
			fprintf(stderr, "error: duplicate variable declaration '%s'\n",
					decl->identifier);
			exit(1);
		}
	}
	char* unique = make_unique_name(decl->identifier);
	varmap_put(map, (VarMapEntry) { .key=strdup(decl->identifier), .val=strdup(unique), .is_cur_scope=true });

	if (decl->init)
		decl->init = resolve_expression(decl->init, map);

	free(decl->identifier);
	decl->identifier = unique;
}

static void resolve_for_init(AstForInit* for_init, VarMap* map) {
	switch (for_init->kind) {
		case AST_INIT_DECL:
			resolve_declaration(&for_init->as.decl, map);
			break;
		case AST_INIT_EXP:
			for_init->as.exp = resolve_expression(for_init->as.exp, map);
			break;
	}
}

static void resolve_block(AstBlock* block, VarMap* map);

static void resolve_statement(AstStatement* stmt, VarMap* map) {
	switch (stmt->kind) {
		case STMT_RETURN:
			stmt->as.ret.exp = resolve_expression(stmt->as.ret.exp, map);
			break;
		case STMT_EXP:
			stmt->as.exp_stmt.exp = resolve_expression(stmt->as.exp_stmt.exp, map);
			break;
		case STMT_IF:
			stmt->as.if_cond.cond = resolve_expression(stmt->as.if_cond.cond, map);
			resolve_statement(stmt->as.if_cond.then_br, map);
			if (stmt->as.if_cond.else_br != NULL) {
				resolve_statement(stmt->as.if_cond.else_br, map);
			}
			break;
		case STMT_COMPOUND: {
			VarMap new_map = varmap_copy(*map);
			resolve_block(&stmt->as.compound, &new_map);
			varmap_destroy(&new_map);
			break;
		}
		case STMT_FOR: {
			VarMap new_map = varmap_copy(*map);
			resolve_for_init(&stmt->as.for_loop.init, &new_map);
			if (stmt->as.for_loop.cond)
				stmt->as.for_loop.cond = resolve_expression(stmt->as.for_loop.cond, &new_map);
			if (stmt->as.for_loop.post)
				stmt->as.for_loop.post = resolve_expression(stmt->as.for_loop.post, &new_map);
			resolve_statement(stmt->as.for_loop.body, &new_map);
			varmap_destroy(&new_map);
			break;
		}
		case STMT_WHILE: {
			VarMap new_map = varmap_copy(*map);
			stmt->as.while_loop.cond = resolve_expression(stmt->as.while_loop.cond, &new_map);
			resolve_statement(stmt->as.while_loop.body, &new_map);
			varmap_destroy(&new_map);
			break;
		}
		case STMT_DO_WHILE: {
			VarMap new_map = varmap_copy(*map);
			stmt->as.do_while_loop.cond = resolve_expression(stmt->as.do_while_loop.cond, &new_map);
			resolve_statement(stmt->as.do_while_loop.body, &new_map);
			varmap_destroy(&new_map);
			break;
		}
		case STMT_SWITCH: {
			stmt->as.switch_stmt.cond = resolve_expression(stmt->as.switch_stmt.cond, map);
			VarMap new_map = varmap_copy(*map);
			for (size_t i = 0; i < stmt->as.switch_stmt.clauses.count; i++)
				resolve_block(&stmt->as.switch_stmt.clauses.items[i].body, &new_map);
			varmap_destroy(&new_map);
			break;
		}
		case STMT_BREAK:
		case STMT_CONTINUE:
		case STMT_LABEL:
		case STMT_GOTO:
			// label/goto names live in a separate namespace from variables
			break;
	}
}

static void resolve_block_item(AstBlockItem* item, VarMap* map) {
	switch (item->kind) {
		case AST_DECLARATION:
			switch (item->as.declaration.kind) {
				case DECL_VAR:
					resolve_declaration(&item->as.declaration.as.variable, map);
					break;
				case DECL_FUNC:
					// a block-scope function declaration is a prototype; it declares no
					// variable, and a nested definition is not C.
					if (item->as.declaration.as.function.body.present) {
						fprintf(stderr, "error: nested function definition '%s'\n",
								item->as.declaration.as.function.identifier);
						exit(1);
					}
					break;
			}
			break;
		case AST_STATEMENT:
			resolve_statement(item->as.statement, map);
			break;
	}
}

static void resolve_block(AstBlock* block, VarMap* map) {
	for (size_t i = 0; i < block->count; i++) {
		resolve_block_item(&block->items[i], map);
	}
}

// True if `name` is already bound in the current (innermost) scope. Used for
// duplicate-parameter and duplicate-declaration checks, which must ignore
// same-named bindings inherited from enclosing scopes (those are shadowing).
static bool bound_in_current_scope(VarMap* map, const char* name) {
	for (int i = 0; i < map->size; i++) {
		if (map->entries[i].is_cur_scope && strcmp(map->entries[i].key, name) == 0)
			return true;
	}
	return false;
}

// Resolves one function against `globals` (the program's function names). A
// prototype only checks its parameters for duplicates; a definition renames its
// parameters and resolves its body. Parameters and the body's outermost block
// share one scope, so a top-level local may not reuse a parameter's name.
static void resolve_function(AstFunctionDeclaration* func, VarMap* globals) {
	if (!func->body.present) {
		// A prototype's parameters may be unnamed, and are never renamed (no body
		// refers to them); still reject two named parameters that collide.
		VarMap seen = varmap_create(8);
		for (size_t i = 0; i < func->params.count; i++) {
			char* param = func->params.items[i];
			if (param == NULL) continue;
			if (bound_in_current_scope(&seen, param)) {
				fprintf(stderr, "error: duplicate parameter '%s' in function '%s'\n",
						param, func->identifier);
				exit(1);
			}
			varmap_put(&seen, (VarMapEntry){ .key=strdup(param), .val=strdup(param), .is_cur_scope=true });
		}
		varmap_destroy(&seen);
		return;
	}

	// Start from the function names (demoted to an enclosing scope by the copy)
	// so calls resolve; parameters go on top in the current scope.
	VarMap map = varmap_copy(*globals);
	for (size_t i = 0; i < func->params.count; i++) {
		char* param = func->params.items[i];
		if (bound_in_current_scope(&map, param)) {
			fprintf(stderr, "error: duplicate parameter '%s' in function '%s'\n",
					param, func->identifier);
			exit(1);
		}
		char* unique = make_unique_name(param);
		varmap_put(&map, (VarMapEntry){ .key=strdup(param), .val=strdup(unique), .is_cur_scope=true });
		free(func->params.items[i]);
		func->params.items[i] = unique;
	}
	resolve_block(&func->body.value, &map);
	varmap_destroy(&map);
}

void resolve_variables(AstProgram* program) {
	RESOLVE_COUNTER = 0;
	// Top-level functions live in one program-wide scope and keep their source
	// names (external linkage), so each maps to itself; a call looks the callee
	// up here. A redeclaration (prototype plus definition) is bound once.
	VarMap globals = varmap_create(16);
	for (size_t i = 0; i < program->count; i++) {
		char* name = program->items[i].identifier;
		if (varmap_get(&globals, name) == NULL)
			varmap_put(&globals, (VarMapEntry){ .key=strdup(name), .val=strdup(name), .is_cur_scope=true });
	}
	for (size_t i = 0; i < program->count; i++) {
		resolve_function(&program->items[i], &globals);
	}
	varmap_destroy(&globals);
}

// --- Typechecking ---
//
// Runs after resolve_variables: variables carry program-unique names and
// functions their source names, so one flat SymbolTable covers the whole
// program — no per-scope copies like the VarMap needs.

SymbolTable symtab_create(int capacity) {
	return (SymbolTable){
		.entries = malloc(sizeof(Symbol) * capacity),
		.size = 0,
		.capacity = capacity,
	};
}

void symtab_destroy(SymbolTable* table) {
	for (int i = 0; i < table->size; i++)
		free(table->entries[i].key);
	free(table->entries);
}

Symbol* symtab_get(SymbolTable* table, const char* key) {
	for (int i = 0; i < table->size; i++) {
		if (strcmp(table->entries[i].key, key) == 0)
			return &table->entries[i];
	}
	return NULL;
}

void symtab_put(SymbolTable* table, Symbol symbol) {
	if (table->size == table->capacity) {
		table->capacity *= 2;
		table->entries = realloc(table->entries, sizeof(Symbol) * table->capacity);
	}
	table->entries[table->size] = symbol;
	table->size++;
}

static int LABEL_COUNTER = 0;

static char* generate_label(void) {
	int len = snprintf(NULL, 0, "loop.%d", LABEL_COUNTER);
	char* name = malloc(len + 1);
	snprintf(name, len + 1, "loop.%d", LABEL_COUNTER++);
	return name;
}

static void label_block(AstBlock* block, char* break_label, char* continue_label);

// break and continue have distinct targets: a loop is the target of both, but a
// switch is a target for break only -- `continue` inside a switch still refers
// to the enclosing loop. So the two labels are threaded separately, and a switch
// overrides break_label while leaving continue_label untouched. NULL means "no
// enclosing target", which makes a stray break/continue an error.
static void label_statement(AstStatement* statement, char* break_label, char* continue_label) {
	switch (statement->kind) {
		case STMT_FOR: {
			char* label = generate_label();
			statement->as.for_loop.label = label;
			label_statement(statement->as.for_loop.body, label, label);
			break;
		}
		case STMT_WHILE: {
			char* label = generate_label();
			statement->as.while_loop.label = label;
			label_statement(statement->as.while_loop.body, label, label);
			break;
		}
		case STMT_DO_WHILE: {
			char* label = generate_label();
			statement->as.do_while_loop.label = label;
			label_statement(statement->as.do_while_loop.body, label, label);
			break;
		}
		case STMT_SWITCH: {
			char* label = generate_label();
			statement->as.switch_stmt.label = label;
			// break exits the switch; continue passes through to the enclosing loop.
			for (size_t i = 0; i < statement->as.switch_stmt.clauses.count; i++)
				label_block(&statement->as.switch_stmt.clauses.items[i].body, label, continue_label);
			break;
		}
		case STMT_BREAK:
			if (break_label == NULL) {
				fprintf(stderr, "break statement outside of loop or switch\n");
				exit(1);
			}
			statement->as.break_stmt.label = strdup(break_label);
			break;
		case STMT_CONTINUE:
			if (continue_label == NULL) {
				fprintf(stderr, "continue statement outside of loop\n");
				exit(1);
			}
			statement->as.continue_stmt.label = strdup(continue_label);
			break;
		case STMT_IF:
			label_statement(statement->as.if_cond.then_br, break_label, continue_label);
			if (statement->as.if_cond.else_br != NULL)
				label_statement(statement->as.if_cond.else_br, break_label, continue_label);
			break;
		case STMT_COMPOUND:
			label_block(&statement->as.compound, break_label, continue_label);
			break;
		default:
			break;
	}
}

static void label_block(AstBlock* block, char* break_label, char* continue_label) {
	for (size_t i = 0; i < block->count; i++) {
		AstBlockItem* item = &block->items[i];
		if (item->kind == AST_STATEMENT) {
			label_statement(item->as.statement, break_label, continue_label);
		}
	}
}

void resolve_labels(AstProgram* program) {
	LABEL_COUNTER = 0;
	for (size_t i = 0; i < program->count; i++) {
		if (program->items[i].body.present)
			label_block(&program->items[i].body.value, NULL, NULL);
	}
}

// --- goto / label resolution ---
//
// Source labels have function scope: a label is visible throughout the whole
// function (a goto may jump forward to a label declared later), and each label
// lives in a namespace separate from variables. This pass, per function:
//   1. collects every label definition, assigning each a program-unique name;
//   2. rewrites each label to its unique name and each goto to the unique name
//      of its target -- erroring if a goto has no matching label.
// It also rejects two labels sharing a name within one function (a C error).
//
// Uniqueness matters because the same source label may appear in several
// functions (or files); emitting the raw name would collide in the assembler.
// Generated names contain '.', which a source identifier never can, so they
// also can't clash with user names; the ".L" infix keeps them distinct from
// variable names ("name.N") and loop labels ("loop.N_start").

static int GOTO_LABEL_COUNTER = 0;

static char* generate_goto_label(const char* original) {
	int len = snprintf(NULL, 0, "%s.L%d", original, GOTO_LABEL_COUNTER);
	char* name = malloc(len + 1);
	snprintf(name, len + 1, "%s.L%d", original, GOTO_LABEL_COUNTER++);
	return name;
}

static void collect_labels_block(AstBlock* block, VarMap* labels);

// Phase 1: register every label defined anywhere in the statement subtree.
static void collect_labels_statement(AstStatement* stmt, VarMap* labels) {
	switch (stmt->kind) {
		case STMT_LABEL:
			if (varmap_get(labels, stmt->as.label.identifier) != NULL) {
				fprintf(stderr, "error: duplicate label '%s'\n", stmt->as.label.identifier);
				exit(1);
			}
			varmap_put(labels, (VarMapEntry) {
				.key = strdup(stmt->as.label.identifier),
				.val = generate_goto_label(stmt->as.label.identifier),
				.is_cur_scope = true,
			});
			break;
		case STMT_IF:
			collect_labels_statement(stmt->as.if_cond.then_br, labels);
			if (stmt->as.if_cond.else_br != NULL)
				collect_labels_statement(stmt->as.if_cond.else_br, labels);
			break;
		case STMT_COMPOUND:
			collect_labels_block(&stmt->as.compound, labels);
			break;
		case STMT_FOR:
			collect_labels_statement(stmt->as.for_loop.body, labels);
			break;
		case STMT_WHILE:
			collect_labels_statement(stmt->as.while_loop.body, labels);
			break;
		case STMT_DO_WHILE:
			collect_labels_statement(stmt->as.do_while_loop.body, labels);
			break;
		case STMT_SWITCH:
			for (size_t i = 0; i < stmt->as.switch_stmt.clauses.count; i++)
				collect_labels_block(&stmt->as.switch_stmt.clauses.items[i].body, labels);
			break;
		case STMT_RETURN:
		case STMT_EXP:
		case STMT_BREAK:
		case STMT_CONTINUE:
		case STMT_GOTO:
			break;
	}
}

static void collect_labels_block(AstBlock* block, VarMap* labels) {
	for (size_t i = 0; i < block->count; i++) {
		AstBlockItem* item = &block->items[i];
		if (item->kind == AST_STATEMENT)
			collect_labels_statement(item->as.statement, labels);
	}
}

static void rewrite_gotos_block(AstBlock* block, VarMap* labels);

// Phase 2: rename each label and point each goto at the matching label's
// unique name. Every label is guaranteed present (collected in phase 1); a
// goto whose target is absent has no matching label and is an error.
static void rewrite_gotos_statement(AstStatement* stmt, VarMap* labels) {
	switch (stmt->kind) {
		case STMT_LABEL: {
			VarMapEntry* entry = varmap_get(labels, stmt->as.label.identifier);
			free(stmt->as.label.identifier);
			stmt->as.label.identifier = strdup(entry->val);
			break;
		}
		case STMT_GOTO: {
			VarMapEntry* entry = varmap_get(labels, stmt->as.goto_stmt.target);
			if (entry == NULL) {
				fprintf(stderr, "error: goto to undefined label '%s'\n", stmt->as.goto_stmt.target);
				exit(1);
			}
			free(stmt->as.goto_stmt.target);
			stmt->as.goto_stmt.target = strdup(entry->val);
			break;
		}
		case STMT_IF:
			rewrite_gotos_statement(stmt->as.if_cond.then_br, labels);
			if (stmt->as.if_cond.else_br != NULL)
				rewrite_gotos_statement(stmt->as.if_cond.else_br, labels);
			break;
		case STMT_COMPOUND:
			rewrite_gotos_block(&stmt->as.compound, labels);
			break;
		case STMT_FOR:
			rewrite_gotos_statement(stmt->as.for_loop.body, labels);
			break;
		case STMT_WHILE:
			rewrite_gotos_statement(stmt->as.while_loop.body, labels);
			break;
		case STMT_DO_WHILE:
			rewrite_gotos_statement(stmt->as.do_while_loop.body, labels);
			break;
		case STMT_SWITCH:
			for (size_t i = 0; i < stmt->as.switch_stmt.clauses.count; i++)
				rewrite_gotos_block(&stmt->as.switch_stmt.clauses.items[i].body, labels);
			break;
		case STMT_RETURN:
		case STMT_EXP:
		case STMT_BREAK:
		case STMT_CONTINUE:
			break;
	}
}

static void rewrite_gotos_block(AstBlock* block, VarMap* labels) {
	for (size_t i = 0; i < block->count; i++) {
		AstBlockItem* item = &block->items[i];
		if (item->kind == AST_STATEMENT)
			rewrite_gotos_statement(item->as.statement, labels);
	}
}

void resolve_goto_labels(AstProgram* program) {
	GOTO_LABEL_COUNTER = 0;
	for (size_t i = 0; i < program->count; i++) {
		if (!program->items[i].body.present) continue;
		// one map per function: labels do not cross function boundaries, but the
		// counter keeps climbing so names stay unique across the whole program.
		VarMap labels = varmap_create(8);
		collect_labels_block(&program->items[i].body.value, &labels);
		rewrite_gotos_block(&program->items[i].body.value, &labels);
		varmap_destroy(&labels);
	}
}

static void typecheck_exp(AstExp* exp, SymbolTable* symtab) {
	if (exp == NULL) return;	// nullable expressions: decl init, for cond/post
	switch (exp->kind) {
		case EXP_FUNCTION_CALL: {
			Symbol* symbol = symtab_get(symtab, exp->as.funcall.identifier);
			if (symbol->kind == SYM_INT) {
				fprintf(stderr, "Variable used as function name");
				exit(1);
			}
			if (symbol->param_count != exp->as.funcall.args.count) {
			   	fprintf(stderr, "Function called with the wrong number of arguments");
				exit(1);
			}
			for (size_t i = 0; i < exp->as.funcall.args.count; i++)
				typecheck_exp(&exp->as.funcall.args.items[i], symtab);
			return;
		}
		case EXP_VAR: {
			Symbol* symbol = symtab_get(symtab, exp->as.variable.identifier);
			if (symbol->kind == SYM_FUNCTION) {
				fprintf(stderr, "Function name used as a variable");
				exit(1);
			}
			return;
		}
		case EXP_INT:
			return;
		case EXP_UNOP:
			typecheck_exp(exp->as.unary.operand, symtab);
			return;
		case EXP_BINOP:
			typecheck_exp(exp->as.binop.lhs, symtab);
			typecheck_exp(exp->as.binop.rhs, symtab);
			return;
		case EXP_ASSIGN:
			typecheck_exp(exp->as.assign.lhs, symtab);
			typecheck_exp(exp->as.assign.rhs, symtab);
			return;
		case EXP_CONDITIONAL:
			typecheck_exp(exp->as.conditional.lhs, symtab);
			typecheck_exp(exp->as.conditional.mid, symtab);
			typecheck_exp(exp->as.conditional.rhs, symtab);
			return;
	}
}

static void typecheck_block(AstBlock* block, SymbolTable* symtab) {
	for (size_t i = 0; i < block->count; i++) {
		switch (block->items[i].kind) {
			case AST_DECLARATION:
				break;
			case AST_STATEMENT:
				break;
		}
	}	
}

static void typecheck_function_declaration(AstFunctionDeclaration* decl, SymbolTable* symtab) {
	Symbol* old_sym = symtab_get(symtab, decl->identifier);
	bool already_defined = false;
	if (old_sym != NULL) {
		if (old_sym->kind != SYM_FUNCTION) exit(1);
		already_defined = old_sym->defined;
		if (already_defined) exit(1);	
	}

	symtab_put(symtab, (Symbol){.key=strdup(decl->identifier), .kind=SYM_FUNCTION, .param_count=decl->params.count, .defined=already_defined || decl->body.present});

	if (decl->body.present) {
		for (size_t i = 0; i < decl->params.count; i++) {
			symtab_put(symtab, (Symbol){.key=strdup(decl->params.items[i]), .kind=SYM_INT});
		}
		typecheck_block(&decl->body.value, symtab);
	}
}

void typecheck(AstProgram* program) {
	SymbolTable symtab = symtab_create(16);
	for (size_t i = 0; i < program->count; i++) {
		typecheck_function_declaration(&program->items[i], &symtab);
	}	
	symtab_destroy(&symtab);
}
