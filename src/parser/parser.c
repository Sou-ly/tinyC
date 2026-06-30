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

static AstBlock resolve_block(AstBlock block, VarMap* map);

static AstStatement resolve_statement(AstStatement stmt, VarMap* map) {
	switch (stmt.kind) {
		case STMT_RETURN:
			stmt.as.ret.exp = resolve_expression(stmt.as.ret.exp, map);
			break;
		case STMT_EXP:
			stmt.as.exp_stmt.exp = resolve_expression(stmt.as.exp_stmt.exp, map);
			break;
		case STMT_IF:
			stmt.as.if_cond.cond = resolve_expression(stmt.as.if_cond.cond, map);
			*stmt.as.if_cond.then_br = resolve_statement(*stmt.as.if_cond.then_br, map);
			if (stmt.as.if_cond.else_br != NULL) {
				*stmt.as.if_cond.else_br = resolve_statement(*stmt.as.if_cond.else_br, map);
			}
			break;
		case STMT_COMPOUND: {
			// the inner block resolves against a copy, so declarations there
			// don't leak into the enclosing scope
			VarMap new_map = varmap_copy(*map);
			AstStatement resolved =
				make_compound_stmt(resolve_block(stmt.as.compound, &new_map));
			varmap_destroy(&new_map);
			return resolved;
		}
	}
	return stmt;
}

static AstDeclaration resolve_declaration(AstDeclaration decl, VarMap* map) {
	// check for duplicate in current scope
	// (linear scan is fine — we only check exact key matches at the top level)
	for (int i = 0; i < map->size; i++) {
		if (map->entries[i].is_cur_scope &&
				strcmp(map->entries[i].key, decl.identifier) == 0) {
			fprintf(stderr, "error: duplicate variable declaration '%s'\n",
					decl.identifier);
			exit(1);
		}
	}
	char* unique = make_unique_name(decl.identifier);
	// the map owns its own key/val copies: decl.identifier is freed below and
	// reassigned to `unique`, which the AST owns — aliasing either here would
	// leave the map with a dangling key or double-free `unique` at cleanup.
	varmap_put(map, (VarMapEntry) { .key=strdup(decl.identifier), .val=strdup(unique), .is_cur_scope=true });

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
			item.as.decl = resolve_declaration(item.as.decl, map);
			break;
		case AST_STATEMENT:
			item.as.stmt = resolve_statement(item.as.stmt, map);
			break;
	}
	return item;
}

static AstBlock resolve_block(AstBlock block, VarMap* map) {
	for (size_t i = 0; i < block.size; i++) {
		block.items[i] = resolve_block_item(block.items[i], map);
	}
	return block;
}

static AstFunction resolve_function(AstFunction func) {
	VarMap map = varmap_create(16);
	func.body = resolve_block(func.body, &map);
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
