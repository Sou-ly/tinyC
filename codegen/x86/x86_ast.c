#include "x86_ast.h"
#include <string.h>

// --- Functions ---

x86_Function* create_x86_function(char* name, x86_Instr* instrs, int num_instrs) {
    x86_Function* fn = malloc(sizeof(x86_Function));
    fn->name = strdup(name);
    fn->instrs = instrs;
    fn->num_instrs = num_instrs;
    return fn;
}

void destroy_x86_function(x86_Function* fn) {
    if (!fn) return;
    free(fn->name);
    free(fn->instrs);
    free(fn);
}

// --- Program ---

x86_Program* create_x86_program(x86_Function** functions, int num_functions) {
    x86_Program* prog = malloc(sizeof(x86_Program));
    prog->functions = functions;
    prog->num_functions = num_functions;
    return prog;
}

void destroy_x86_program(x86_Program* prog) {
    if (!prog) return;
    for (int i = 0; i < prog->num_functions; i++) {
        destroy_x86_function(prog->functions[i]);
    }
    free(prog->functions);
    free(prog);
}
