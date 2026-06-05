#include "codegen.h"
#include <stdio.h>

static x86_Instr* codegen_stmt(AstStatement* stmt, int* num_instrs) {
    switch (stmt->kind) {
        case STMT_RETURN: {
            int val = stmt->ret.expr->int_lit.value;
            x86_Instr* instrs = malloc(sizeof(x86_Instr) * 2);
            instrs[0] = (x86_Instr){.kind = x86_MOV, .mov = {
                .dst = (x86_Operand){.kind = x86_REG, .reg = x86_AX},
                .src = (x86_Operand){.kind = x86_IMM, .imm = val}
            }};
            instrs[1] = (x86_Instr){.kind = x86_RET};
            *num_instrs = 2;
            return instrs;
        }
        default:
            fprintf(stderr, "codegen: unsupported statement kind\n");
            exit(1);
    }
}

static x86_Function* codegen_decl(AstDeclaration* decl) {
    switch (decl->kind) {
        case DECL_FUNCTION: {
            int num_instrs = 0;
            x86_Instr* instrs = codegen_stmt(decl->function.body[0], &num_instrs);
            return create_x86_function(decl->function.name, instrs, num_instrs);
        }
        default:
            fprintf(stderr, "codegen: unsupported declaration kind\n");
            exit(1);
    }
}

x86_Program* codegen(AstProgram* program) {
    x86_Function** functions = malloc(sizeof(x86_Function*) * program->num_decls);
    for (int i = 0; i < program->num_decls; i++) {
        functions[i] = codegen_decl(program->decls[i]);
    }
    return create_x86_program(functions, program->num_decls);
}
