#include "x86_ast.h"
#include <string.h>

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
