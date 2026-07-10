#pragma once

#include <stdlib.h>
#include <stdbool.h>
#include "../common/list.h"
#include "../common/optional.h"

// --- Expressions ---

typedef enum {
    UNOP_COMP,
    UNOP_MINUS,
	UNOP_NOT,
	UNOP_POSTINC,
	UNOP_PREINC,
	UNOP_POSTDEC,
	UNOP_PREDEC
} AstUnopType;

typedef enum {
	// arithmetic
    BINOP_ADD,
    BINOP_SUB,
	BINOP_MUL,
	BINOP_DIV,
	BINOP_MOD,
	// bitwise
	BINOP_AND,
	BINOP_OR,
	BINOP_XOR,
	BINOP_LSHIFT,
	BINOP_RSHIFT,
	// logical
	BINOP_LAND,
	BINOP_LOR,
	BINOP_EQ,
	BINOP_NEQ,
	BINOP_LESS,
	BINOP_GREATER,
	BINOP_LEQ,
	BINOP_GEQ,
	// special
	BINOP_ASSIGN,
	BINOP_CONDITION
} AstBinopType;

typedef enum {
	ASSIGN_NOP,
	ASSIGN_ADD,
	ASSIGN_SUB,
	ASSIGN_MUL,
	ASSIGN_DIV,
	ASSIGN_MOD,
	ASSIGN_OR,
	ASSIGN_AND,
	ASSIGN_XOR,
	ASSIGN_RSHIFT,
	ASSIGN_LSHIFT,
} AstAssignOp;

typedef enum {
	EXP_INT,
	EXP_UNOP,
	EXP_BINOP,
	EXP_VAR,
	EXP_ASSIGN,
	EXP_CONDITIONAL
} AstExpKind;

typedef struct AstExp AstExp;

typedef struct { int value; }										AstExpInt;
typedef struct { AstUnopType op_type; AstExp* operand; }			AstExpUnary;
typedef struct { AstBinopType op_type; AstExp* lhs; AstExp* rhs; }	AstExpBinop;
typedef struct { char* identifier; }								AstExpVar;
typedef struct { AstAssignOp op; AstExp* lhs; AstExp* rhs; }		AstExpAssign;
typedef struct { AstExp* lhs; AstExp* mid; AstExp* rhs; }			AstExpConditional;
typedef struct { char* identifier; AstExp* args; }					AstExpFunctionCall;

struct AstExp {
    AstExpKind kind;
    union {
        AstExpInt			int_lit;
        AstExpUnary			unary;
        AstExpBinop			binop;
        AstExpVar			variable;
        AstExpAssign		assign;
		AstExpConditional	conditional;
    } as;
};

AstExp* ast_exp_int(int value);
AstExp* ast_exp_unary(AstUnopType op_type, AstExp* operand);
AstExp* ast_exp_binop(AstBinopType op_type, AstExp* lhs, AstExp* rhs);
AstExp* ast_exp_var(const char* identifier);
AstExp* ast_exp_assign(AstAssignOp op, AstExp* lhs, AstExp* rhs);
AstExp* ast_exp_conditional(AstExp* lhs, AstExp* mid, AstExp* rhs);
void ast_exp_destroy(AstExp* exp);

// --- Blocks ---

typedef struct AstBlockItem AstBlockItem;
typedef struct AstStatement AstStatement;

typedef LIST_OF(AstBlockItem) AstBlock;

// Prototype (absent) vs empty body (present, count 0) are different declarations.
OPTIONAL_TYPE(OptionalBlock, AstBlock);

// --- Declarations ---

typedef LIST_OF(char*) AstParamList;

typedef struct AstVariableDeclaration {
	char* identifier;
	AstExp* init;			// nullable: `int x;` has no initializer
} AstVariableDeclaration;

typedef struct AstFunctionDeclaration {
	char* identifier;
	AstParamList params;
	OptionalBlock body;		// absent for a prototype, present for a definition
} AstFunctionDeclaration;

typedef enum { DECL_FUNC, DECL_VAR } AstDeclarationKind;

typedef struct AstDeclaration {
	AstDeclarationKind kind;
	union {
		AstFunctionDeclaration	function;
		AstVariableDeclaration	variable;
	} as;
} AstDeclaration;

// Adopt identifier and the params/body/init given. Destroys free what the
// declaration owns, not the node (declarations are stored by value).
AstFunctionDeclaration ast_function_declaration(char* identifier, AstParamList params, OptionalBlock body);
AstVariableDeclaration ast_variable_declaration(char* identifier, AstExp* init);
AstDeclaration ast_declaration_function(AstFunctionDeclaration function);
AstDeclaration ast_declaration_variable(AstVariableDeclaration variable);
void ast_function_declaration_destroy(AstFunctionDeclaration* declaration);
void ast_variable_declaration_destroy(AstVariableDeclaration* declaration);
void ast_declaration_destroy(AstDeclaration* declaration);

// --- Statements ---

typedef enum {
    STMT_RETURN,
    STMT_EXP,
    STMT_IF,
	STMT_COMPOUND,
	STMT_FOR,
	STMT_WHILE,
	STMT_DO_WHILE,
	STMT_BREAK,
	STMT_CONTINUE,
	STMT_LABEL,
	STMT_GOTO,
	STMT_SWITCH,
} AstStatementKind;

typedef struct { AstExp* exp; }			AstStmtReturn;
typedef struct { AstExp* exp; }			AstStmtExp;
typedef struct { char* label; }			AstStmtContinue;
typedef struct { char* label; }			AstStmtBreak;
typedef struct { char* identifier; }	AstStmtLabel;
typedef struct { char* target; }		AstStmtGoto;

typedef struct {
	AstExp* cond;
	AstStatement* then_br;
	AstStatement* else_br;
} AstStmtIf;

typedef struct AstStmtWhile {
	char* label;
	AstExp* cond;
	AstStatement* body;
} AstStmtWhile;

typedef struct AstStmtDoWhile {
	char* label;
	AstExp* cond;
	AstStatement* body;
} AstStmtDoWhile;

typedef enum { AST_INIT_DECL, AST_INIT_EXP } AstForInitKind;

typedef struct {
	AstForInitKind kind;
	union {
		AstVariableDeclaration	decl;
		AstExp*		exp;
	} as;
} AstForInit;

typedef struct AstStmtFor {
	char*			label;
	AstForInit		init;
	AstExp*			cond;	// nullable
	AstExp*			post;	// nullable
	AstStatement*	body;
} AstStmtFor;

typedef struct AstSwitchClause {
    bool is_default;
    int value;        // valid only when !is_default
    AstBlock body;
} AstSwitchClause;

typedef LIST_OF(AstSwitchClause) AstClauseList;

typedef struct AstStmtSwitch {
    char* label;                 // assigned by the labelling pass; base for clause/break labels
    AstExp* cond;
    AstClauseList clauses;       // source order; empty when count == 0
} AstStmtSwitch;

struct AstStatement {
    AstStatementKind kind;
    union {
        AstStmtReturn	ret;
        AstStmtExp		exp_stmt;
        AstStmtIf		if_cond;
        AstBlock		compound;
		AstStmtFor		for_loop;
		AstStmtWhile	while_loop;
		AstStmtDoWhile	do_while_loop;
		AstStmtContinue	continue_stmt;
		AstStmtBreak	break_stmt;
		AstStmtLabel	label;
		AstStmtGoto		goto_stmt;
        AstStmtSwitch   switch_stmt;
    } as;
};

AstStatement* ast_stmt_return(AstExp* exp);
AstStatement* ast_stmt_exp(AstExp* exp);
AstStatement* ast_stmt_if(AstExp* cond, AstStatement* then_br, AstStatement* else_br);
AstStatement* ast_stmt_compound(AstBlock block);
AstStatement* ast_stmt_for(AstForInit init, AstExp* cond, AstExp* post, AstStatement* body);
AstStatement* ast_stmt_while(AstExp* cond, AstStatement* body);
AstStatement* ast_stmt_do_while(AstExp* cond, AstStatement* body);
AstStatement* ast_stmt_break(char* label);
AstStatement* ast_stmt_continue(char * label);
AstStatement* ast_stmt_label(char* identifier);
AstStatement* ast_stmt_goto(char* target);
AstStatement* ast_stmt_switch(AstExp* cond, AstClauseList clauses);
AstForInit ast_for_init_decl(AstVariableDeclaration decl);
AstForInit ast_for_init_exp(AstExp* exp);
void ast_stmt_destroy(AstStatement* stmt);

// --- Block items ---

struct AstBlockItem {
	enum AstBlockItemKind { AST_DECLARATION, AST_STATEMENT } kind;
	union {
		AstDeclaration	declaration;
		AstStatement*	statement;
	} as;
};

// Build blocks with (AstBlock){0} and ast_block_append -- list_push can't take a
// compound literal, its braces' commas read as extra macro arguments.
void ast_block_append(AstBlock* block, AstBlockItem block_item);
void ast_block_destroy(AstBlock* block);

// --- Program ---

typedef LIST_OF(AstFunctionDeclaration) AstProgram;

// Adopts `functions` (a heap array of `num_functions`) as backing storage.
AstProgram ast_program_create(AstFunctionDeclaration* functions, int num_functions);
void ast_program_destroy(AstProgram* program);
