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
	BINOP_ASSIGN
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
	EXP_ASSIGN
} AstExpKind;

typedef struct AstExp AstExp;

struct AstExp {
    AstExpKind kind;
    union {
        struct { int value; }											int_lit;
        struct { AstUnopType op_type; AstExp* operand; }				unary;
        struct { AstBinopType op_type; AstExp* lhs; AstExp* rhs; }		binop;
		struct { char* identifier; }									variable;
		struct { AstAssignOp op; AstExp* lhs; AstExp* rhs; }			assign;
    };
};

AstExp* create_int_exp(int value);
AstExp* create_unary_exp(AstUnopType op_type, AstExp* operand);
AstExp* create_binop_exp(AstBinopType op_type, AstExp* lhs, AstExp* rhs);
AstExp* create_variable_exp(const char* identifier);
AstExp* create_assign_exp(AstAssignOp op, AstExp* lhs, AstExp* rhs);
void destroy_exp(AstExp* exp);

// --- Statements ---

typedef enum {
    STMT_RETURN,
    STMT_EXP
} AstStatementKind;

typedef struct {
    AstStatementKind kind;
    union {
        struct { AstExp* exp; } ret;
        struct { AstExp* exp; } exp_stmt;
    };
} AstStatement;

AstStatement make_return_stmt(AstExp* exp);
AstStatement make_exp_stmt(AstExp* exp);
void destroy_stmt(AstStatement* stmt);

// --- Declarations ---

typedef enum {
    AST_DECLARATION,
    AST_STATEMENT
} AstBlockItemType;

typedef struct {
	char* identifier;
	AstExp* exp; // nullable
} AstDeclaration;

typedef struct {
	AstBlockItemType type;
	union {
		AstDeclaration	decl;
		AstStatement	stmt;
	};
} AstBlockItem;

typedef struct {
	char* identifier;
	size_t size;
	size_t capacity;
	AstBlockItem* body;
} AstFunction;

AstFunction ast_function_make(const char* name, size_t capacity);
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
