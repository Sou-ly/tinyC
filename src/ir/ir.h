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

typedef struct { IrVal val; }								IrRet;
typedef struct { IrUnopType op; IrVal src; IrVal dst; }		IrUnary;
typedef struct { IrBinopType op; IrVal lhs; IrVal rhs;
				 IrVal dst; }								IrBinop;
typedef struct { IrVal src; IrVal dst; }					IrCopy;
typedef struct { char* target; }							IrJump;
typedef struct { IrVal cond; char* target; }				IrJumpZero;
typedef struct { IrVal cond; char* target; }				IrJumpNotZero;
typedef struct { char* identifier; }						IrLabel;

typedef struct {
    IrInstructionType type;
    union {
        IrRet			ret;
        IrUnary			unary;
        IrBinop			binop;
        IrCopy			copy;
        IrJump			jump;
        IrJumpZero		jump_zero;
        IrJumpNotZero	jump_not_zero;
        IrLabel			label;
    } as;
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

// --- IR constructors ---
//
// Mirror the create_*/make_* helpers in the AST: build a value or instruction
// from its parts so call sites stay readable instead of hand-writing designated
// initializers. String fields (variable names, labels, jump targets) are stored
// by pointer, NOT copied — the caller transfers ownership of an already
// allocated string (from generate_variable_name / generate_label_name / strdup).
// Storing the pointer as-is keeps a name's identity stable when it is shared
// across several instructions (e.g. a label reused by the jumps targeting it).

IrVal ir_constant(int value);
IrVal ir_variable(char* name);

IrInstruction ir_return(IrVal val);
IrInstruction ir_unary(IrUnopType op, IrVal src, IrVal dst);
IrInstruction ir_binop(IrBinopType op, IrVal lhs, IrVal rhs, IrVal dst);
IrInstruction ir_copy(IrVal src, IrVal dst);
IrInstruction ir_jump(char* target);
IrInstruction ir_jump_zero(IrVal cond, char* target);
IrInstruction ir_jump_not_zero(IrVal cond, char* target);
IrInstruction ir_label(char* identifier);

IrProgram emit_ir(const AstProgram* ast_program);
void destroy_ir(IrProgram* program);
