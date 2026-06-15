#pragma once

#include <stdlib.h>
#include <stdbool.h>

// --- Expressions ---

typedef enum {
    UNOP_COMP,
    UNOP_MINUS,
	UNOP_NOT
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
	BINOP_GEQ
} AstBinopType;

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
		struct { AstExp* lhs; AstExp* rhs }								assign;
    };
};

AstExp* create_int_exp(int value);
AstExp* create_unary_exp(AstUnopType op_type, AstExp* operand);
AstExp* create_binop_exp(AstBinopType op_type, AstExp* lhs, AstExp* rhs);
AstExp* create_variable_exp(const char* identifier);
AstExp* create_assign_exp(AstExp* lhs, AstExp* rhs);
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
    DECL_FUNCTION
} AstDeclarationKind;

typedef struct {
	char* name;
	AstStatement* body;
	int num_stmts;
} AstFunction;

typedef struct {
    AstDeclarationKind kind;
    union {
		AstFunction function;
    };
} AstDeclaration;

AstDeclaration make_function_decl(char* name, AstStatement* body, int num_stmts);
void destroy_decl(AstDeclaration* decl);

// --- Program ---

typedef struct {
    AstDeclaration* decls;
    int num_decls;
} AstProgram;

AstProgram make_program(AstDeclaration* decls, int num_decls);
char* to_string(AstProgram);
void destroy_program(AstProgram* program);
