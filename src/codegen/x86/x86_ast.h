#pragma once

#include <stdlib.h>

// --- Operands ---

typedef enum {
    x86_AX,
    x86_R10
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
    x86_NOT
} x86_Unop;

// --- Instructions (linked list) ---

typedef enum {
    x86_MOV,
    x86_RET,
    x86_ALLOC,
    x86_UNOP
} x86_InstrKind;

typedef struct x86_Instr {
    x86_InstrKind kind;
    union {
        // struct {}                                   ret
        struct { x86_Unop unop; x86_Operand operand; } unop;
        struct { x86_Operand dst; x86_Operand src; }   mov;
        struct { int size; }                           alloc_stack;
    };
    struct x86_Instr* next;
} x86_Instr;

typedef struct {
    x86_Instr* head;
    x86_Instr* tail;
} x86_InstrList;

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
