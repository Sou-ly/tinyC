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
} ExprKind;

typedef struct Expr Expr;

struct Expr {
    ExprKind kind;
    union {
        struct { int value; } int_lit;
        struct { UnaryOpType op_type; Expr* operand; } unary;
        struct { char op; Expr* lhs; Expr* rhs; } binary;
    };
};

Expr* create_int_expr(int value);
Expr* create_unary_expr(UnaryOpType op_type, Expr* operand);
Expr* create_binary_expr(char op, Expr* lhs, Expr* rhs);
void destroy_expr(Expr* expr);

// --- Statements ---

typedef enum {
    STMT_RETURN,
    STMT_EXPR
} StmtKind;

typedef struct Stmt Stmt;

struct Stmt {
    StmtKind kind;
    union {
        struct { Expr* expr; } ret;
        struct { Expr* expr; } expr_stmt;
    };
};

Stmt* create_return_stmt(Expr* expr);
Stmt* create_expr_stmt(Expr* expr);
void destroy_stmt(Stmt* stmt);

// --- Declarations ---

typedef enum {
    DECL_FUNCTION
} DeclKind;

typedef struct Decl Decl;

struct Decl {
    DeclKind kind;
    union {
        struct {
            char* name;
            Stmt** body;
            int num_stmts;
        } function;
    };
};

Decl* create_function_decl(char* name, Stmt** body, int num_stmts);
void destroy_decl(Decl* decl);

// --- Program ---

typedef struct {
    Decl** decls;
    int num_decls;
} Program;

Program* create_program(Decl** decls, int num_decls);
void destroy_program(Program* program);
