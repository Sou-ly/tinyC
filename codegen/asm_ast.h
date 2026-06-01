#pragma once

#include <stdlib.h>

// --- Operands ---

typedef enum {
    OPERAND_REG,
    OPERAND_IMM
} OperandKind;

typedef struct {
    OperandKind kind;
    union {
        char* reg;   // e.g. "w0", "x0"
        int imm;     // e.g. 2
    };
} AsmOperand;

AsmOperand asm_reg(const char* reg);
AsmOperand asm_imm(int value);
void destroy_operand(AsmOperand* op);

// --- Instructions ---

typedef enum {
    ASM_MOV,
    ASM_RET
} AsmInstrKind;

typedef struct {
    AsmInstrKind kind;
    AsmOperand* operands;
    int num_operands;
} AsmInstr;

AsmInstr asm_mov(AsmOperand dst, AsmOperand src);
AsmInstr asm_ret(void);
void destroy_instr(AsmInstr* instr);

// --- Functions ---

typedef struct {
    char* name;
    AsmInstr* instrs;
    int num_instrs;
} AsmFunction;

AsmFunction* create_asm_function(char* name, AsmInstr* instrs, int num_instrs);
void destroy_asm_function(AsmFunction* fn);

// --- Program ---

typedef struct {
    AsmFunction** functions;
    int num_functions;
} AsmProgram;

AsmProgram* create_asm_program(AsmFunction** functions, int num_functions);
void destroy_asm_program(AsmProgram* prog);
