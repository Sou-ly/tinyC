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
        char* identifier;
        int int_val;
    } as;
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

typedef enum IrInstructionKind {
    IR_RETURN,
    IR_UNOP,
	IR_BINOP,
	IR_COPY,
	IR_JUMP,
	IR_JUMP_ZERO,
	IR_JUMP_NOT_ZERO,
	IR_LABEL,
	IR_FUNCALL
} IrInstructionKind;

typedef LIST_OF(IrVal) IrValList;

typedef struct { IrVal val; }										IrRet;
typedef struct { IrUnopType op; IrVal src; IrVal dst; }				IrUnary;
typedef struct { IrBinopType op; IrVal lhs; IrVal rhs; IrVal dst; } IrBinop;
typedef struct { IrVal src; IrVal dst; }							IrCopy;
typedef struct { char* target; }									IrJump;
typedef struct { IrVal cond; char* target; }						IrJumpZero;
typedef struct { IrVal cond; char* target; }						IrJumpNotZero;
typedef struct { char* identifier; }								IrLabel;
typedef struct { char* identifier; IrValList args; IrVal dst; }		IrFunctionCall;

typedef struct {
    IrInstructionKind kind;
    union {
        IrRet			ret;
        IrUnary			unary;
        IrBinop			binop;
        IrCopy			copy;
        IrJump			jump;
        IrJumpZero		jump_zero;
        IrJumpNotZero	jump_not_zero;
        IrLabel			label;
        IrFunctionCall	funcall;
    } as;
} IrInstruction;

typedef struct {
    char*					identifier;
	LIST_OF(char*)			params; 
    LIST_OF(IrInstruction)	instructions;
} IrFunction;

typedef LIST_OF(IrFunction) IrProgram;

// --- IR constructors ---

IrVal ir_val_constant(int value);
IrVal ir_val_variable(char* identifier);

IrInstruction ir_instr_return(IrVal val);
IrInstruction ir_instr_unary(IrUnopType op, IrVal src, IrVal dst);
IrInstruction ir_instr_binop(IrBinopType op, IrVal lhs, IrVal rhs, IrVal dst);
IrInstruction ir_instr_copy(IrVal src, IrVal dst);
IrInstruction ir_instr_jump(char* target);
IrInstruction ir_instr_jump_zero(IrVal cond, char* target);
IrInstruction ir_instr_jump_not_zero(IrVal cond, char* target);
IrInstruction ir_instr_label(char* identifier);
IrInstruction ir_instr_function_call(char* identifier, IrValList args, IrVal dst);

IrProgram emit_ir(const AstProgram* ast_program);
void ir_program_destroy(IrProgram* program);
