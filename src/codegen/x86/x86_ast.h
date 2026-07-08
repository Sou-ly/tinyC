#pragma once

#include <stdlib.h>

// --- Operands ---

typedef enum {
    x86_AX,
	x86_DX,
    x86_R10,
    x86_R11
} x86_Reg;

typedef enum {
    x86_REG,
    x86_IMM,
    x86_ID,
    x86_STACK
} x86_OperandKind;

typedef struct {
    x86_OperandKind kind;
    union {
        int      imm;
        x86_Reg  reg;
        char*    identifier;
        int      stack;
    } as;
} x86_Operand;

x86_Operand x86_operand_reg(x86_Reg reg);
x86_Operand x86_operand_imm(int imm);
x86_Operand x86_operand_id(char* identifier);
x86_Operand x86_operand_stack(int offset);

// --- Operators ---

typedef enum {
    x86_NEG,
    x86_COMP,
} x86_Unop;

typedef enum {
    x86_ADD,
    x86_SUB,
	x86_MUL,
	x86_DIV,
	x86_MOD,
	x86_AND,
	x86_OR,
	x86_XOR,
	x86_LSHIFT,
	x86_RSHIFT,
} x86_Binop;

typedef enum x86_ConditionCode {
    x86_E,
    x86_NE,
    x86_L,
    x86_LE,
    x86_G,
    x86_GE
} x86_ConditionCode;

// --- Instructions (linked list) ---

typedef enum {
    x86_MOV,
    x86_RET,
    x86_ALLOC,
    x86_UNOP,
    x86_BINOP,
	x86_IDIV,
	x86_CDQ,
    x86_CMP,
    x86_JMP,
    x86_JMPCC,
    x86_SETCC,
    x86_LABEL
} x86_InstrKind;

typedef struct { x86_Operand dst; x86_Operand src; }				x86_Mov;
typedef struct { x86_Unop unop; x86_Operand operand; }				x86_UnopInstr;
typedef struct { x86_Binop optype; x86_Operand rhs;
				 x86_Operand dst; }									x86_BinopInstr;
typedef struct { x86_Operand operand; }								x86_Idiv;
typedef struct { int size; }										x86_AllocStack;
typedef struct { x86_Operand lhs; x86_Operand rhs; }				x86_Cmp;
typedef struct { char* identifier; }								x86_Jmp;
typedef struct { x86_ConditionCode cond; x86_Operand op; }			x86_Setcc;
typedef struct { x86_ConditionCode cond; char* identifier; }		x86_Jmpcc;
typedef struct { char* identifier; }								x86_Label;

typedef struct x86_Instr {
    x86_InstrKind kind;
    union {
        x86_Mov			mov;
        x86_UnopInstr	unop;
        x86_BinopInstr	binop;
        x86_Idiv		idiv;
        // cdq has no payload
        x86_AllocStack	alloc_stack;
        // ret has no payload
        x86_Cmp			cmp;
        x86_Jmp			jmp;
        x86_Setcc		setcc;
        x86_Jmpcc		jmpcc;
        x86_Label		label;
    } as;
    struct x86_Instr* next;
} x86_Instr;

x86_Instr x86_instr_mov(x86_Operand dst, x86_Operand src);
x86_Instr x86_instr_ret(void);
x86_Instr x86_instr_alloc(int size);
x86_Instr x86_instr_unary(x86_Unop op, x86_Operand operand);
x86_Instr x86_instr_binary(x86_Binop op, x86_Operand rhs, x86_Operand dst);
x86_Instr x86_instr_idiv(x86_Operand operand);
x86_Instr x86_instr_cdq(void);
x86_Instr x86_instr_cmp(x86_Operand lhs, x86_Operand rhs);
x86_Instr x86_instr_jmp(char* identifier);
x86_Instr x86_instr_jmpcc(x86_ConditionCode cond, char* identifier);
x86_Instr x86_instr_setcc(x86_ConditionCode cond, x86_Operand op);
x86_Instr x86_instr_label(char* identifier);

// --- Instruction list ---

typedef struct {
    x86_Instr* head;
    x86_Instr* tail;
} x86_InstrList;

x86_InstrList x86_instr_list_create(void);
x86_Instr* x86_instr_list_append(x86_InstrList* list, x86_Instr instr);
void x86_instr_list_prepend(x86_InstrList* list, x86_Instr instr);
void x86_instr_list_destroy(x86_InstrList* list);

// --- Functions ---

typedef struct {
    char* identifier;
    x86_InstrList instrs;
} x86_Function;

x86_Function x86_function_create(char* identifier, x86_InstrList instrs);
void x86_function_destroy(x86_Function* fn);

// --- Program ---

typedef struct {
    x86_Function* functions;
    int num_functions;
} x86_Program;

x86_Program x86_program_create(x86_Function* functions, int num_functions);
void x86_program_destroy(x86_Program* prog);
