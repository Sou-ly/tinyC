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
		case TOK_LAND:
		case TOK_LOR:
		case TOK_EQ:
		case TOK_NEQ:
		case TOK_LESS:
		case TOK_GREATER:
		case TOK_LEQ:
		case TOK_GEQ:
		case TOK_ASSIGN:
			return true;
		default:
			return false;
	}
}

static int precedence(Token* tok) {
	switch (tok->op) {
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
		default:			break;
	}
	fprintf(stderr, "binop: unrecognized token operator");
	exit(1);
}

static AstUnopType tok_to_unop(TokenOperator op) {
	switch (op) {
		case TOK_NOT:		return UNOP_COMP;
		case TOK_MINUS:		return UNOP_MINUS;
		case TOK_LNOT:		return UNOP_NOT;
		default:			break;
	}
	fprintf(stderr, "unop: unrecognized token operator");
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

static AstExp* parse_expression(Parser* p, int min_prec);

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
            advance(p);
            return create_unary_exp(tok_to_unop(tok->op), parse_factor(p));
        case TOK_SEPARATOR:
            if (tok->sep == TOK_LPAR) {
                advance(p);
                AstExp* exp = parse_expression(p, 0);
                expect_separator(p, TOK_RPAR);
                return exp;
            }
            break;
		case TOK_IDENTIFIER: {
			AstExp* exp = create_variable_exp(current(p)->ident);
			advance(p);
			return exp;
		}
        default:
            break;
    }

    fprintf(stderr, "parse error at %zu:%zu: expected factor\n",
            current(p)->line, current(p)->col);
    exit(1);
}

static AstExp* parse_expression(Parser* p, int min_prec) {
    AstExp* lhs = parse_factor(p);
	Token* tok = current(p);
    while (is_binop(tok) && precedence(tok) >= min_prec) {
        AstBinopType op = tok_to_binop(current(p)->op);
		int prec = precedence(tok);
		advance(p);
		// assign operator is right associative
		// everything else is left associative
		AstExp* rhs;
		if (op == BINOP_ASSIGN) {
			rhs = parse_expression(p, prec);
			lhs = create_assign_exp(lhs, rhs);
		} else {
			rhs = parse_expression(p, prec + 1);
			lhs = create_binop_exp(op, lhs, rhs);
		}
		tok = current(p);
    }
    return lhs;
}

static AstStatement parse_statement(Parser* p) {
    if (current(p)->kind == TOK_KEYWORD && current(p)->kw == TOK_RETURN) {
        advance(p); // consume 'return'
        AstExp* exp = parse_expression(p, 0);
        expect_separator(p, TOK_SEMICOLON);
        return make_return_stmt(exp);
    }

    // expression statement: exp ;
    AstExp* exp = parse_expression(p, 0);
    expect_separator(p, TOK_SEMICOLON);
    return make_exp_stmt(exp);
}

static AstDeclaration parse_declaration(Parser* p) {
	AstDeclaration declaration;
	if (current(p)->kind == TOK_KEYWORD && current(p)->kw == TOK_INT) {
		advance(p); // consume int
		if (current(p)->kind != TOK_IDENTIFIER) {
    	    fprintf(stderr, "parse error at %zu:%zu: expected variable name\n", current(p)->line, current(p)->col);
    	    exit(1);
    	}
    	declaration.identifier = strdup(current(p)->ident);
		advance(p);
		if (current(p)->kind == TOK_OPERATOR && current(p)->op == TOK_ASSIGN) {
			advance(p); // consume '='
			declaration.exp = parse_expression(p, 0);
		} else {
			declaration.exp = NULL;
		}
		expect_separator(p, TOK_SEMICOLON);
		return declaration;
	}
    fprintf(stderr, "parse error at %zu:%zu: expected declaration\n",
            current(p)->line, current(p)->col);
    exit(1);
}

static AstBlockItem parse_block_item(Parser* p) {
	AstBlockItem block_item;
	if (current(p)->kind == TOK_KEYWORD && current(p)->kw == TOK_INT) {
		block_item.type = AST_DECLARATION;
		block_item.decl = parse_declaration(p);
	} else {
		block_item.type = AST_STATEMENT;
		block_item.stmt = parse_statement(p);
	}
	return block_item;
}

static AstFunction parse_function(Parser* p) {
    // expect: int <name> ( )  { block-items  }
    expect_keyword(p, TOK_INT);
    if (current(p)->kind != TOK_IDENTIFIER) {
        fprintf(stderr, "parse error at %zu:%zu: expected function name\n", current(p)->line, current(p)->col);
        exit(1);
    }
    char* name = current(p)->ident;
    advance(p);
    expect_separator(p, TOK_LPAR);
    expect_separator(p, TOK_RPAR);
    expect_separator(p, TOK_LBRACE);
    // parse body
	AstFunction function = ast_function_make(name, 8);
    while (!at_end(p) && !(current(p)->kind == TOK_SEPARATOR && current(p)->sep == TOK_RBRACE)) {
		AstBlockItem block_item = parse_block_item(p);
		ast_function_append(&function, block_item);
    }
    expect_separator(p, TOK_RBRACE);
    return function;
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
    AstFunction* functions = malloc(sizeof(AstFunction) * capacity);

    while (!at_end(p)) {
        if (count >= capacity) {
            capacity *= 2;
            functions = realloc(functions, sizeof(AstFunction) * capacity);
        }
        functions[count++] = parse_function(p);
    }

    return ast_program_create(functions, count);
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

void varmap_destroy(VarMap* map) {
	for (int i = 0; i < map->size; i++) {
		free(map->entries[i].key);
		free(map->entries[i].val);
	}
	free(map->entries);
}

char* varmap_get(VarMap* map, const char* key) {
	// search backwards so later entries shadow earlier ones
	for (int i = map->size - 1; i >= 0; i--) {
		if (strcmp(map->entries[i].key, key) == 0) {
			return map->entries[i].val;
		}
	}
	return NULL;
}

void varmap_put(VarMap* map, const char* key, const char* val) {
	if (map->size == map->capacity) {
		map->capacity *= 2;
		map->entries = realloc(map->entries, sizeof(VarMapEntry) * map->capacity);
	}
	map->entries[map->size] = (VarMapEntry){
		.key = strdup(key),
		.val = strdup(val),
	};
	map->size++;
}

static AstExp* resolve_expression(AstExp* exp, VarMap* map) {
	if (!exp) return NULL;
	switch (exp->kind) {
		case EXP_INT:
			return exp;
		case EXP_VAR: {
			char* resolved = varmap_get(map, exp->variable.identifier);
			if (!resolved) {
				fprintf(stderr, "error: undeclared variable '%s'\n",
						exp->variable.identifier);
				exit(1);
			}
			free(exp->variable.identifier);
			exp->variable.identifier = strdup(resolved);
			return exp;
		}
		case EXP_ASSIGN: {
			if (exp->assign.lhs->kind != EXP_VAR) {
				fprintf(stderr, "error: invalid lvalue in assignment\n");
				exit(1);
			}
			exp->assign.lhs = resolve_expression(exp->assign.lhs, map);
			exp->assign.rhs = resolve_expression(exp->assign.rhs, map);
			return exp;
		}
		case EXP_UNOP:
			exp->unary.operand = resolve_expression(exp->unary.operand, map);
			return exp;
		case EXP_BINOP:
			exp->binop.lhs = resolve_expression(exp->binop.lhs, map);
			exp->binop.rhs = resolve_expression(exp->binop.rhs, map);
			return exp;
	}
	return exp;
}

static AstStatement resolve_statement(AstStatement stmt, VarMap* map) {
	switch (stmt.kind) {
		case STMT_RETURN:
			stmt.ret.exp = resolve_expression(stmt.ret.exp, map);
			break;
		case STMT_EXP:
			stmt.exp_stmt.exp = resolve_expression(stmt.exp_stmt.exp, map);
			break;
	}
	return stmt;
}

static AstDeclaration resolve_declaration(AstDeclaration decl, VarMap* map) {
	// check for duplicate in current scope
	// (linear scan is fine — we only check exact key matches at the top level)
	for (int i = 0; i < map->size; i++) {
		if (strcmp(map->entries[i].key, decl.identifier) == 0) {
			fprintf(stderr, "error: duplicate variable declaration '%s'\n",
					decl.identifier);
			exit(1);
		}
	}
	char* unique = make_unique_name(decl.identifier);
	varmap_put(map, decl.identifier, unique);

	// resolve the initializer (if any) AFTER recording the mapping
	// so `int a = a;` is technically allowed (C behavior)
	decl.exp = resolve_expression(decl.exp, map);

	// rename the declaration itself
	free(decl.identifier);
	decl.identifier = unique;
	return decl;
}

static AstBlockItem resolve_block_item(AstBlockItem item, VarMap* map) {
	switch (item.type) {
		case AST_DECLARATION:
			item.decl = resolve_declaration(item.decl, map);
			break;
		case AST_STATEMENT:
			item.stmt = resolve_statement(item.stmt, map);
			break;
	}
	return item;
}

static AstFunction resolve_function(AstFunction func) {
	VarMap map = varmap_create(16);
	for (size_t i = 0; i < func.size; i++) {
		func.body[i] = resolve_block_item(func.body[i], &map);
	}
	varmap_destroy(&map);
	return func;
}

AstProgram resolve_variables(AstProgram program) {
	RESOLVE_COUNTER = 0;
	for (int i = 0; i < program.num_functions; i++) {
		program.functions[i] = resolve_function(program.functions[i]);
	}
	return program;
}
