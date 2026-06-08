#pragma once

#include <stdlib.h>
#include <stdbool.h>

// --- Expressions ---

typedef enum {
    UNARY_NOT,
    UNARY_MINUS
} UnaryOpType;

typedef enum {
    EXPR_INT,
    EXPR_UNARY,
    EXPR_BINARY
} AstExpKind;

typedef struct AstExp AstExp;

struct AstExp {
    AstExpKind kind;
    union {
        struct { int value; } int_lit;
        struct { UnaryOpType op_type; AstExp* operand; } unary;
        struct { char op; AstExp* lhs; AstExp* rhs; } binary;
    };
};

AstExp* create_int_exp(int value);
AstExp* create_unary_exp(UnaryOpType op_type, AstExp* operand);
AstExp* create_binary_exp(char op, AstExp* lhs, AstExp* rhs);
void destroy_exp(AstExp* exp);

// --- Statements ---

typedef enum {
    STMT_RETURN,
    STMT_EXPR
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
void destroy_program(AstProgram* program);
