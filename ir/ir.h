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
} IrUnaryOpType;

typedef enum {
    IR_RETURN,
    IR_UNARY
} IrInstructionType;

typedef struct {
    IrInstructionType type;
    union {
        struct { IrVal val; } ret;
        struct { IrUnaryOpType op; IrVal src; IrVal dst; } unary;
    };
} IrInstruction;

typedef struct {
    char* identifier;
    IrInstruction* instructions;
	int size;
} IrFunction;
    
typedef struct {
    IrFunction* functions;
    int size;
} IrProgram;

IrProgram emit_ir(const AstProgram* ast_program);
