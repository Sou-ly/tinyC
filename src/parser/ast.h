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

// --- Optional expression ---

typedef OPTIONAL_OF(AstExp*) OptionalExp;

#define some_exp(e) ((OptionalExp)SOME(e))
#define no_exp()    ((OptionalExp)NONE)



// --- Blocks ---

typedef struct AstBlockItem AstBlockItem;
typedef struct AstStatement AstStatement;

typedef LIST_OF(AstBlockItem) AstBlock;

typedef struct AstVarDecl {
	char* identifier;
	OptionalExp init;
} AstVarDecl;

typedef struct AstFuncDecl {
	char* identifier;
	LIST_OF(char*) params;
	AstBlock* body;
} AstFuncDecl;

typedef struct AstDeclaration {
	enum DeclarationKind { DECL_FUNC, DECL_VAR } kind;
	union {
		AstFuncDecl	func;
		AstVarDecl	var;
	} as;
} AstDeclaration;

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
		AstVarDecl	decl;
		AstExp*		exp;
	} as;
} AstForInit;

typedef struct AstStmtFor {
	char*			label;
	AstForInit		init;
	OptionalExp		cond;
	OptionalExp		post;
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
AstStatement* ast_stmt_for(AstForInit init, OptionalExp cond, OptionalExp post, AstStatement* body);
AstStatement* ast_stmt_while(AstExp* cond, AstStatement* body);
AstStatement* ast_stmt_do_while(AstExp* cond, AstStatement* body);
AstStatement* ast_stmt_break(char* label);
AstStatement* ast_stmt_continue(char * label);
AstStatement* ast_stmt_label(char* identifier);
AstStatement* ast_stmt_goto(char* target);
AstStatement* ast_stmt_switch(AstExp* cond, AstClauseList clauses);
AstForInit ast_for_init_decl(AstVarDecl decl);
AstForInit ast_for_init_exp(AstExp* exp);
void ast_stmt_destroy(AstStatement* stmt);

// --- Declarations & block items ---

struct AstBlockItem {
	enum AstBlockItemKind { AST_DECLARATION, AST_STATEMENT } kind;
	union {
		AstVarDecl		decl;
		AstStatement*	stmt;
	} as;
};

// Blocks are LIST_OF(AstBlockItem): build with (AstBlock){0} and list_push,
// or ast_block_append for a compound-literal item (list_push can't take one
// directly — its braces' commas would be read as extra macro arguments).
// ast_block_destroy frees each item's owned contents, then the storage.
void ast_block_append(AstBlock* block, AstBlockItem block_item);
void ast_block_destroy(AstBlock* block);

// --- Functions ---

typedef struct {
	char* identifier;
	AstBlock body;
} AstFunction;

AstFunction ast_function_create(const char* identifier, AstBlock block);
void ast_function_append(AstFunction* function, AstBlockItem block_item);
void ast_function_destroy(AstFunction* function);

// --- Program ---

typedef LIST_OF(AstFunction) AstProgram;

// Adopts `functions` (a heap array of `num_functions`) as the list's backing
// storage — no copy; the program owns and later frees it.
AstProgram ast_program_create(AstFunction* functions, int num_functions);
char* ast_program_to_string(AstProgram);
void ast_program_destroy(AstProgram* program);
