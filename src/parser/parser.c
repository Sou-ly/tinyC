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
static AstDeclaration parse_declaration(Parser* p);

// factor ::= int | ("!" | "-") factor | "(" exp ")"  | ++id | --id | id++ | id--
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
			// TODO: should probably have some check on lvalue instead... need to check C standard
			if ((tok->op == TOK_INCR || tok->op == TOK_DECR) && current(p)->kind == TOK_IDENTIFIER) {
				AstExp* exp = create_variable_exp(current(p)->ident);
				advance(p);
				return create_unary_exp(tok->op == TOK_INCR? UNOP_PREINC : UNOP_PREDEC, exp);
			} else {
				return create_unary_exp(tok_to_unop(tok->op), parse_factor(p));
			}
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
			if (current(p)->kind == TOK_OPERATOR && (current(p)->op == TOK_INCR || current(p)->op == TOK_DECR)) {
				TokenOperator postfix = current(p)->op;
				advance(p);
				return create_unary_exp(postfix == TOK_INCR? UNOP_POSTINC : UNOP_POSTDEC, exp);
			} else {
				return exp;
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
        AstBinopType op = tok_to_binop(current(p)->op);
		int prec = precedence(tok);
		advance(p);
		AstExp* rhs;
		if (op == BINOP_ASSIGN) {
			rhs = parse_expression(p, prec);
			lhs = create_assign_exp(tok_to_assign_op(tok->op), lhs, rhs);
		} else if (op == BINOP_CONDITION) {
			AstExp* mid = parse_expression(p, 0);
			expect_separator(p, TOK_COLON);
			rhs = parse_expression(p, prec);
			lhs = create_conditional_exp(lhs, mid, rhs);
		} else {
			rhs = parse_expression(p, prec + 1);
			lhs = create_binop_exp(op, lhs, rhs);
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
            next != NULL && next->kind == TOK_SEPARATOR && next->sep == TOK_COLON) {
        char* identifier = strdup(current(p)->ident);
        advance(p); // identifier
        advance(p); // ':'
        return make_label_stmt(identifier);
    }

    if (current(p)->kind != TOK_KEYWORD) {
		AstExp* exp = parse_expression(p, 0);
    	expect_separator(p, TOK_SEMICOLON);
    	return make_exp_stmt(exp);
    }

	switch(current(p)->kw) {
		case TOK_RETURN: {
			advance(p); // consume 'return'
    		AstExp* exp = parse_expression(p, 0);
    		expect_separator(p, TOK_SEMICOLON);
    		return make_return_stmt(exp);
		}
		case TOK_IF: {
			advance(p);
			expect_separator(p, TOK_LPAR);
			AstExp* cond = parse_expression(p, 0);
			expect_separator(p, TOK_RPAR);
			AstStatement* then_br = parse_statement(p);
			AstStatement* else_br = NULL;
			if (current(p)->kind == TOK_KEYWORD && current(p)->kw == TOK_ELSE) {
				advance(p);
				else_br = parse_statement(p);
			}
			return make_if_stmt(cond, then_br, else_br);
		}
		case TOK_FOR: {
			advance(p);
			expect_separator(p, TOK_LPAR);
			AstForInit for_init;
			if (current(p)->kind == TOK_KEYWORD && current(p)->kw == TOK_INT) {
				for_init = make_for_init_decl(parse_declaration(p));
			} else {
				AstExp* exp = parse_expression(p, 0);
				expect_separator(p, TOK_SEMICOLON);
				for_init = make_for_init_exp(exp);
			}
			AstExp* cond = parse_expression(p, 0);
			expect_separator(p, TOK_SEMICOLON);
			AstExp* post = parse_expression(p, 0);
			expect_separator(p, TOK_RPAR);
			AstStatement* body = parse_statement(p);
			return make_for_stmt(for_init, cond, post, body);
		}
		case TOK_WHILE: {
			advance(p);
			expect_separator(p, TOK_LPAR);
			AstExp* cond = parse_expression(p, 0);
			expect_separator(p, TOK_RPAR);
			AstStatement* body = parse_statement(p);
			return make_while_stmt(cond, body);
		}
		case TOK_DO: {
			advance(p);
			AstStatement* body = parse_statement(p);
			expect_keyword(p, TOK_WHILE);
			expect_separator(p, TOK_LPAR);
			AstExp* cond = parse_expression(p, 0);
			expect_separator(p, TOK_RPAR);
			expect_separator(p, TOK_SEMICOLON);
			return make_do_while_stmt(cond, body);
		}
		case TOK_ELSE:
			fprintf(stderr, "Missing if statement\n");
			exit(1);
		case TOK_CONTINUE:
			advance(p);
			expect_separator(p, TOK_SEMICOLON);
			return make_continue_stmt(NULL); // label assigned by loop-labelling pass
		case TOK_BREAK:
			advance(p);
			expect_separator(p, TOK_SEMICOLON);
			return make_break_stmt(NULL); // label assigned by loop-labelling pass
		case TOK_GOTO: {
			advance(p); // consume 'goto'
			if (current(p)->kind != TOK_IDENTIFIER) {
				fprintf(stderr, "parse error at %zu:%zu: expected label after 'goto'\n",
						current(p)->line, current(p)->col);
				exit(1);
			}
			char* target = strdup(current(p)->ident);
			advance(p); // consume target
			expect_separator(p, TOK_SEMICOLON);
			return make_goto_stmt(target);
		}
		default:
			break;
	}
	fprintf(stderr, "parse error at %zu:%zu: unexpected keyword in statement\n",
			current(p)->line, current(p)->col);
	exit(1);
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
		block_item.as.decl = parse_declaration(p);
	} else {
		block_item.type = AST_STATEMENT;
		block_item.as.stmt = parse_statement(p);
	}
	return block_item;
}

static AstBlock parse_block(Parser* p) {
    expect_separator(p, TOK_LBRACE);
	AstBlock block = ast_block_make(8);
    while (!at_end(p) && !(current(p)->kind == TOK_SEPARATOR && current(p)->sep == TOK_RBRACE)) {
		AstBlockItem block_item = parse_block_item(p);
		ast_block_append(&block, block_item);
    }
    expect_separator(p, TOK_RBRACE);
    return block;
}

static AstFunction parse_function(Parser* p) {
    // expect: int <name> ( )  { block-items  }
    expect_keyword(p, TOK_INT);
    if (current(p)->kind != TOK_IDENTIFIER) {
        fprintf(stderr, "parse error at %zu:%zu: expected function name\n", current(p)->line, current(p)->col);
        exit(1);
    }
    char* name = strdup(current(p)->ident);
    advance(p);
    expect_separator(p, TOK_LPAR);
    expect_separator(p, TOK_RPAR);
	AstBlock block = parse_block(p);
	AstFunction function = ast_function_make(name, block);
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
	}
	return exp;
}

static void resolve_declaration(AstDeclaration* decl, VarMap* map) {
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

	decl->exp = resolve_expression(decl->exp, map);

	free(decl->identifier);
	decl->identifier = unique;
}

static void resolve_for_init(AstForInit* for_init, VarMap* map) {
	switch (for_init->init_type) {
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
			stmt->as.for_loop.cond = resolve_expression(stmt->as.for_loop.cond, &new_map);
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
		case STMT_BREAK:
		case STMT_CONTINUE:
		case STMT_LABEL:
		case STMT_GOTO:
			// label/goto names live in a separate namespace from variables
			break;
	}
}

static void resolve_block_item(AstBlockItem* item, VarMap* map) {
	switch (item->type) {
		case AST_DECLARATION:
			resolve_declaration(&item->as.decl, map);
			break;
		case AST_STATEMENT:
			resolve_statement(item->as.stmt, map);
			break;
	}
}

static void resolve_block(AstBlock* block, VarMap* map) {
	for (size_t i = 0; i < block->size; i++) {
		resolve_block_item(&block->items[i], map);
	}
}

static void resolve_function(AstFunction* func) {
	VarMap map = varmap_create(16);
	resolve_block(&func->body, &map);
	varmap_destroy(&map);
}

void resolve_variables(AstProgram* program) {
	RESOLVE_COUNTER = 0;
	for (int i = 0; i < program->num_functions; i++) {
		resolve_function(&program->functions[i]);
	}
}

static int LABEL_COUNTER = 0;

static char* generate_label(void) {
	int len = snprintf(NULL, 0, "loop.%d", LABEL_COUNTER);
	char* name = malloc(len + 1);
	snprintf(name, len + 1, "loop.%d", LABEL_COUNTER++);
	return name;
}

static void label_block(AstBlock* block, char* current_label);

static void label_statement(AstStatement* statement, char* current_label) {
	switch (statement->kind) {
		case STMT_FOR: {
			char* label = generate_label();
			statement->as.for_loop.label = label;
			label_statement(statement->as.for_loop.body, label);
			break;
		}
		case STMT_WHILE: {
			char* label = generate_label();
			statement->as.while_loop.label = label;
			label_statement(statement->as.while_loop.body, label);
			break;
		}
		case STMT_DO_WHILE: {
			char* label = generate_label();
			statement->as.do_while_loop.label = label;
			label_statement(statement->as.do_while_loop.body, label);
			break;
		}
		case STMT_BREAK:
			if (current_label == NULL) {
				fprintf(stderr, "break statement outside of loop\n");
				exit(1);
			}
			statement->as.break_stmt.label = strdup(current_label);
			break;
		case STMT_CONTINUE:
			if (current_label == NULL) {
				fprintf(stderr, "continue statement outside of loop\n");
				exit(1);
			}
			statement->as.continue_stmt.label = strdup(current_label);
			break;
		case STMT_IF:
			label_statement(statement->as.if_cond.then_br, current_label);
			if (statement->as.if_cond.else_br != NULL)
				label_statement(statement->as.if_cond.else_br, current_label);
			break;
		case STMT_COMPOUND:
			label_block(&statement->as.compound, current_label);
			break;
		default:
			break;
	}
}

static void label_block(AstBlock* block, char* current_label) {
	for (size_t i = 0; i < block->size; i++) {
		AstBlockItem* item = &block->items[i];
		if (item->type == AST_STATEMENT) {
			label_statement(item->as.stmt, current_label);
		}
	}
}

void resolve_labels(AstProgram* program) {
	LABEL_COUNTER = 0;
	for (int i = 0; i < program->num_functions; i++) {
		label_block(&program->functions[i].body, NULL);
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
		case STMT_RETURN:
		case STMT_EXP:
		case STMT_BREAK:
		case STMT_CONTINUE:
		case STMT_GOTO:
			break;
	}
}

static void collect_labels_block(AstBlock* block, VarMap* labels) {
	for (size_t i = 0; i < block->size; i++) {
		AstBlockItem* item = &block->items[i];
		if (item->type == AST_STATEMENT)
			collect_labels_statement(item->as.stmt, labels);
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
		case STMT_RETURN:
		case STMT_EXP:
		case STMT_BREAK:
		case STMT_CONTINUE:
			break;
	}
}

static void rewrite_gotos_block(AstBlock* block, VarMap* labels) {
	for (size_t i = 0; i < block->size; i++) {
		AstBlockItem* item = &block->items[i];
		if (item->type == AST_STATEMENT)
			rewrite_gotos_statement(item->as.stmt, labels);
	}
}

void resolve_goto_labels(AstProgram* program) {
	GOTO_LABEL_COUNTER = 0;
	for (int i = 0; i < program->num_functions; i++) {
		// one map per function: labels do not cross function boundaries, but the
		// counter keeps climbing so names stay unique across the whole program.
		VarMap labels = varmap_create(8);
		collect_labels_block(&program->functions[i].body, &labels);
		rewrite_gotos_block(&program->functions[i].body, &labels);
		varmap_destroy(&labels);
	}
}
