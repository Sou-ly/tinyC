#pragma once

#include <stdlib.h>
#include <stdbool.h>

typedef enum {
    CONSTANT,
    VARIABLE
} TacValKind;

typedef struct {
    TacValKind kind;
    union {
        int int_val;
        char * identifier;
    };
} TacVal;

typedef enum {
    TAC_COMP,
    TAC_NEG
} TacUnaryOp;

typedef enum {
    TAC_RETURN,
    TAC_UNARY
} TacInstrType;

typedef struct {
    TacInstrType type;
    union {
        struct { TacVal val; } ret;
        struct { TacUnaryOp op, TacVal src; TacVal dst } unary;
    };
} TacInstruction;

typedef struct {
    char name * identifier;
    TacInstruction* body;
} TacFunction;
    
typedef struct {
    TacFunction** func;
    int num_func;
} TacProgram;

TacProgram* emit_tacky(AstProgram);
