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
    };
} x86_Operand;

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

typedef struct x86_Instr {
    x86_InstrKind kind;
    union {
        struct { x86_Operand dst; x86_Operand src; }					mov;
		struct { x86_Unop unop; x86_Operand operand; }					unop;
        struct { x86_Binop optype; x86_Operand rhs; x86_Operand dst; }	binop;
		struct { x86_Operand operand; }									idiv;
		// struct {}													cdq;
        struct { int size; }											alloc_stack;
        // struct {}													ret;
        struct { x86_Operand lhs; x86_Operand rhs; }                    cmp;
        struct { char* identifier; }                                    jmp;
        struct { x86_ConditionCode cond; x86_Operand op; }              setcc;
        struct { x86_ConditionCode cond; char* identifier; }            jmpcc;
        struct { char* identifier; }                                    label;
    };
    struct x86_Instr* next;
} x86_Instr;

typedef struct {
    x86_Instr* head;
    x86_Instr* tail;
} x86_InstrList;

// --- Operand constructors ---

x86_Operand x86_operand_reg(x86_Reg reg);
x86_Operand x86_operand_imm(int imm);
x86_Operand x86_operand_id(char* identifier);
x86_Operand x86_operand_stack(int offset);

// --- Instruction constructors ---

x86_Instr x86_mov(x86_Operand dst, x86_Operand src);
x86_Instr x86_ret(void);
x86_Instr x86_alloc(int size);
x86_Instr x86_unary(x86_Unop op, x86_Operand operand);
x86_Instr x86_binary(x86_Binop op, x86_Operand rhs, x86_Operand dst);
x86_Instr x86_idiv_instr(x86_Operand operand);
x86_Instr x86_cdq_instr(void);
x86_Instr x86_cmp_instr(x86_Operand lhs, x86_Operand rhs);
x86_Instr x86_jmp_instr(char* identifier);
x86_Instr x86_jmpcc_instr(x86_ConditionCode cond, char* identifier);
x86_Instr x86_setcc_instr(x86_ConditionCode cond, x86_Operand op);
x86_Instr x86_label_instr(char* identifier);

// --- Instruction list ---

x86_InstrList x86_instr_list_new(void);
x86_Instr* x86_instr_list_append(x86_InstrList* list, x86_Instr instr);
void x86_instr_list_prepend(x86_InstrList* list, x86_Instr instr);
void x86_instr_list_destroy(x86_InstrList* list);

// --- Functions ---

typedef struct {
    char* name;
    x86_InstrList instrs;
} x86_Function;

// --- Program ---

typedef struct {
    x86_Function* functions;
    int num_functions;
} x86_Program;

x86_Function make_x86_function(char* name, x86_InstrList instrs);
void destroy_x86_function(x86_Function* fn);

x86_Program make_x86_program(x86_Function* functions, int num_functions);
void destroy_x86_program(x86_Program* prog);
