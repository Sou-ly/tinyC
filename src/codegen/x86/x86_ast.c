#include "x86_ast.h"
#include <string.h>

// --- Operand constructors ---

x86_Operand x86_operand_reg(x86_Reg reg) {
    return (x86_Operand){ .kind = x86_REG, .as.reg = reg };
}

x86_Operand x86_operand_imm(int imm) {
    return (x86_Operand){ .kind = x86_IMM, .as.imm = imm };
}

x86_Operand x86_operand_id(char* identifier) {
    return (x86_Operand){ .kind = x86_ID, .as.identifier = identifier };
}

x86_Operand x86_operand_stack(int offset) {
    return (x86_Operand){ .kind = x86_STACK, .as.stack = offset };
}

// --- Instruction constructors ---

x86_Instr x86_mov(x86_Operand dst, x86_Operand src) {
    return (x86_Instr){ .kind = x86_MOV, .as.mov = { .dst = dst, .src = src } };
}

x86_Instr x86_ret(void) {
    return (x86_Instr){ .kind = x86_RET };
}

x86_Instr x86_alloc(int size) {
    return (x86_Instr){ .kind = x86_ALLOC, .as.alloc_stack = { .size = size } };
}

x86_Instr x86_unary(x86_Unop op, x86_Operand operand) {
    return (x86_Instr){ .kind = x86_UNOP, .as.unop = { .unop = op, .operand = operand } };
}

x86_Instr x86_binary(x86_Binop op, x86_Operand rhs, x86_Operand dst) {
    return (x86_Instr){ .kind = x86_BINOP, .as.binop = { .optype = op, .rhs = rhs, .dst = dst } };
}

x86_Instr x86_idiv_instr(x86_Operand operand) {
    return (x86_Instr){ .kind = x86_IDIV, .as.idiv = { .operand = operand } };
}

x86_Instr x86_cdq_instr(void) {
    return (x86_Instr){ .kind = x86_CDQ };
}

x86_Instr x86_cmp_instr(x86_Operand lhs, x86_Operand rhs) {
    return (x86_Instr){ .kind = x86_CMP, .as.cmp = { .lhs = lhs, .rhs = rhs } };
}

x86_Instr x86_jmp_instr(char* identifier) {
    return (x86_Instr){ .kind = x86_JMP, .as.jmp = { .identifier = identifier } };
}

x86_Instr x86_jmpcc_instr(x86_ConditionCode cond, char* identifier) {
    return (x86_Instr){ .kind = x86_JMPCC, .as.jmpcc = { .cond = cond, .identifier = identifier } };
}

x86_Instr x86_setcc_instr(x86_ConditionCode cond, x86_Operand op) {
    return (x86_Instr){ .kind = x86_SETCC, .as.setcc = { .cond = cond, .op = op } };
}

x86_Instr x86_label_instr(char* identifier) {
    return (x86_Instr){ .kind = x86_LABEL, .as.label = { .identifier = identifier } };
}

// --- Instruction List ---

x86_InstrList x86_instr_list_new(void) {
    return (x86_InstrList){ .head = NULL, .tail = NULL };
}

x86_Instr* x86_instr_list_append(x86_InstrList* list, x86_Instr instr) {
    x86_Instr* node = malloc(sizeof(x86_Instr));
    *node = instr;
    node->next = NULL;
    if (list->tail) {
        list->tail->next = node;
    } else {
        list->head = node;
    }
    list->tail = node;
    return node;
}

void x86_instr_list_prepend(x86_InstrList* list, x86_Instr instr) {
    x86_Instr* node = malloc(sizeof(x86_Instr));
    *node = instr;
    node->next = list->head;
    list->head = node;
    if (!list->tail) {
        list->tail = node;
    }
}

void x86_instr_list_destroy(x86_InstrList* list) {
    x86_Instr* cur = list->head;
    while (cur) {
        x86_Instr* next = cur->next;
        free(cur);
        cur = next;
    }
    list->head = NULL;
    list->tail = NULL;
}

// --- Functions ---

x86_Function make_x86_function(char* name, x86_InstrList instrs) {
    return (x86_Function){ .name = strdup(name), .instrs = instrs };
}

void destroy_x86_function(x86_Function* fn) {
    free(fn->name);
    x86_instr_list_destroy(&fn->instrs);
}

// --- Program ---

x86_Program make_x86_program(x86_Function* functions, int num_functions) {
    return (x86_Program){ .functions = functions, .num_functions = num_functions };
}

void destroy_x86_program(x86_Program* prog) {
    for (int i = 0; i < prog->num_functions; i++) {
        destroy_x86_function(&prog->functions[i]);
    }
    free(prog->functions);
}
