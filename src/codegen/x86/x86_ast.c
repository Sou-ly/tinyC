#include "x86_ast.h"
#include <string.h>

// --- Functions ---

x86_Function make_x86_function(char* name, x86_Instr* instrs, int num_instrs) {
    return (x86_Function){ .name = strdup(name), .instrs = instrs, .num_instrs = num_instrs };
}

void destroy_x86_function(x86_Function* fn) {
    free(fn->name);
    free(fn->instrs);
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
