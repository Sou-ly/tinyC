#pragma once

#include <stdlib.h>
#include <stdbool.h>

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

AstExp* create_int_exp(int value);
AstExp* create_unary_exp(AstUnopType op_type, AstExp* operand);
AstExp* create_binop_exp(AstBinopType op_type, AstExp* lhs, AstExp* rhs);
AstExp* create_variable_exp(const char* identifier);
AstExp* create_assign_exp(AstAssignOp op, AstExp* lhs, AstExp* rhs);
AstExp* create_conditional_exp(AstExp* lhs, AstExp* mid, AstExp* rhs);
void destroy_exp(AstExp* exp);



// --- Blocks ---
//
// A block is a list of block items; each item is a statement or a
// declaration. A compound statement (STMT_COMPOUND) is just a block, so
// blocks nest through statements. The mutual recursion forces this
// ordering: AstBlock holds AstBlockItem* (pointer, so a forward decl is
// enough), AstStatement embeds AstBlock by value, and AstBlockItem embeds
// AstStatement by value — so statements must be fully defined before block
// items.

typedef struct AstStatement AstStatement;
typedef struct AstBlockItem AstBlockItem;

typedef struct {
	size_t capacity;
	size_t size;
	AstBlockItem* items;
} AstBlock;

// --- Statements ---

typedef enum {
    STMT_RETURN,
    STMT_EXP,
    STMT_IF,
	STMT_COMPOUND
} AstStatementKind;

typedef struct { AstExp* exp; }													AstStmtReturn;
typedef struct { AstExp* exp; }													AstStmtExp;
typedef struct { AstExp* cond; AstStatement* then_br; AstStatement* else_br; }	AstStmtIf;

struct AstStatement {
    AstStatementKind kind;
    union {
        AstStmtReturn	ret;
        AstStmtExp		exp_stmt;
        AstStmtIf		if_cond;
        AstBlock		compound;
    } as;
};

AstStatement make_return_stmt(AstExp* exp);
AstStatement make_exp_stmt(AstExp* exp);
AstStatement make_if_stmt(AstExp* cond, AstStatement* then_br, AstStatement* else_br);
AstStatement make_compound_stmt(AstBlock block);
void destroy_stmt(AstStatement* stmt);

// --- Declarations & block items ---

typedef enum {
    AST_DECLARATION,
    AST_STATEMENT
} AstBlockItemType;

typedef struct {
	char* identifier;
	AstExp* exp; // nullable
} AstDeclaration;

struct AstBlockItem {
	AstBlockItemType type;
	union {
		AstDeclaration	decl;
		AstStatement	stmt;
	} as;
};

AstBlock ast_block_make(size_t capacity);
void ast_block_append(AstBlock* block, AstBlockItem block_item);
void ast_block_destroy(AstBlock* block);

// --- Functions ---

typedef struct {
	char* identifier;
	AstBlock body;
} AstFunction;

AstFunction ast_function_make(const char* name, AstBlock block);
void ast_function_append(AstFunction* function, AstBlockItem block_item);
void ast_function_destroy(AstFunction* function);

// --- Program ---

typedef struct {
    AstFunction* functions;
    int num_functions;
} AstProgram;

AstProgram ast_program_create(AstFunction* functions, int num_functions);
char* to_string(AstProgram);
void destroy_program(AstProgram* program);
