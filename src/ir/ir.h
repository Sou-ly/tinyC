#pragma once

#include <stdlib.h>
#include <stdbool.h>

#include "../parser/ast.h"

typedef enum {
    IR_CONSTANT,
    IR_VARIABLE
} IrValKind;

typedef struct {
    IrValKind kind;
    union {
        char* name;
        int int_val;
    };
} IrVal;

typedef enum {
    IR_COMP,
    IR_NEG
} IrUnopType;

typedef enum {
    IR_ADD,
	IR_SUB,
	IR_MUL,
	IR_DIV,
	IR_MOD,
	IR_AND,
	IR_OR,
	IR_XOR,
	IR_LSHIFT,
	IR_RSHIFT
} IrBinopType;

typedef enum {
    IR_RETURN,
    IR_UNOP,
	IR_BINOP
} IrInstructionType;

typedef struct {
    IrInstructionType type;
    union {
        struct { IrVal val; } ret;
        struct { IrUnopType op; IrVal src; IrVal dst; } unary;
        struct { IrBinopType op; IrVal lhs; IrVal rhs; IrVal dst; } binop;
    };
} IrInstruction;

typedef struct {
    char* name;
    IrInstruction* instructions;
	int size;
} IrFunction;

typedef struct {
    IrFunction* functions;
    int size;
} IrProgram;

IrProgram emit_ir(const AstProgram* ast_program);
void destroy_ir(IrProgram* program);
