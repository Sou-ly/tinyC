#pragma once

#include <stdlib.h>
#include <stdbool.h>

#include "../parser/ast.h"

typedef enum IrValKind {
    IR_CONSTANT,
    IR_VARIABLE
} IrValKind;

typedef struct IrVal {
    IrValKind kind;
    union {
        char* name;
        int int_val;
    };
} IrVal;

typedef enum IrUnopType {
    IR_COMP,
    IR_NEG,
	IR_NOT
} IrUnopType;

typedef enum IrBinopType {
    IR_ADD,
	IR_SUB,
	IR_MUL,
	IR_DIV,
	IR_MOD,
	IR_AND,
	IR_OR,
	IR_XOR,
	IR_LSHIFT,
	IR_RSHIFT,
	IR_EQ,
	IR_NEQ,
	IR_LAND,
	IR_LOR,
	IR_LESS,
	IR_GREATER,
	IR_LEQ,
	IR_GEQ
} IrBinopType;

typedef enum IrInstructionType {
    IR_RETURN,
    IR_UNOP,
	IR_BINOP,
	IR_COPY,
	IR_JUMP,
	IR_JUMP_ZERO,
	IR_JUMP_NOT_ZERO,
	IR_LABEL
} IrInstructionType;

typedef struct {
    IrInstructionType type;
    union {
        struct { IrVal val; } ret;
        struct { IrUnopType op; IrVal src; IrVal dst; }				unary;
        struct { IrBinopType op; IrVal lhs; IrVal rhs; IrVal dst; } binop;
		struct { IrVal src; IrVal dst; }							copy;
		struct { char* target; }									jump;
		struct { IrVal cond; char* target; }						jump_zero;
		struct { IrVal cond; char* target; } 						jump_not_zero;
		struct { char* identifier; }								label;
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
