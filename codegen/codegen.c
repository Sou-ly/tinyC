#include "codegen.h"
#include <stdio.h>

static AsmInstr* codegen_stmt(AstStatement* stmt, int* num_instrs) {
    switch (stmt->kind) {
        case STMT_RETURN: {
            int val = stmt->ret.expr->int_lit.value;
            AsmInstr* instrs = malloc(sizeof(AsmInstr) * 2);
            instrs[0] = asm_mov(asm_reg("w0"), asm_imm(val));
            instrs[1] = asm_ret();
            *num_instrs = 2;
            return instrs;
        }
        default:
            fprintf(stderr, "codegen: unsupported statement kind\n");
            exit(1);
    }
}

static AsmFunction* codegen_decl(AstDeclaration* decl) {
    switch (decl->kind) {
        case DECL_FUNCTION: {
            // For now: codegen only the first statement
            int num_instrs = 0;
            AsmInstr* instrs = codegen_stmt(decl->function.body[0], &num_instrs);
            return create_asm_function(decl->function.name, instrs, num_instrs);
        }
        default:
            fprintf(stderr, "codegen: unsupported declaration kind\n");
            exit(1);
    }
}

AsmProgram* codegen(AstProgram* program) {
    AsmFunction** functions = malloc(sizeof(AsmFunction*) * program->num_decls);
    for (int i = 0; i < program->num_decls; i++) {
        functions[i] = codegen_decl(program->decls[i]);
    }
    return create_asm_program(functions, program->num_decls);
}
