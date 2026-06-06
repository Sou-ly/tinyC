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
} AstExpressionKind;

typedef struct AstExpression AstExpression;

struct AstExpression {
    AstExpressionKind kind;
    union {
        struct { int value; } int_lit;
        struct { UnaryOpType op_type; AstExpression* operand; } unary;
        struct { char op; AstExpression* lhs; AstExpression* rhs; } binary;
    };
};

AstExpression* create_int_expr(int value);
AstExpression* create_unary_expr(UnaryOpType op_type, AstExpression* operand);
AstExpression* create_binary_expr(char op, AstExpression* lhs, AstExpression* rhs);
void destroy_expr(AstExpression* expr);

// --- Statements ---

typedef enum {
    STMT_RETURN,
    STMT_EXPR
} AstStatementKind;

typedef struct {
    AstStatementKind kind;
    union {
        struct { AstExpression* expr; } ret;
        struct { AstExpression* expr; } expr_stmt;
    };
} AstStatement;

AstStatement make_return_stmt(AstExpression* expr);
AstStatement make_expr_stmt(AstExpression* expr);
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
