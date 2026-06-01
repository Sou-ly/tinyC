#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "lexer/token.h"
#include "list.h"
#include "parser/parser.h"
#include "codegen/codegen.h"
#include "codegen/emit.h"

#define C_FILE_EXTENSION ".c"

typedef enum {
    STAGE_FULL,
    STAGE_LEX,
    STAGE_PARSE,
    STAGE_CODEGEN
} CompileStage;

static void display_help(const char* prog_name) {
    fprintf(stderr, "Usage: %s [options] <source.c>\n", prog_name);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  --lex       Stop after lexing\n");
    fprintf(stderr, "  --parse     Stop after parsing\n");
    fprintf(stderr, "  --codegen   Stop after codegen (emit .s file)\n");
    fprintf(stderr, "  --help      Show this help\n");
}

int main(int argc, char* argv[]) {
    CompileStage stage = STAGE_FULL;
    char* source_path = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--lex") == 0) {
            stage = STAGE_LEX;
        } else if (strcmp(argv[i], "--parse") == 0) {
            stage = STAGE_PARSE;
        } else if (strcmp(argv[i], "--codegen") == 0) {
            stage = STAGE_CODEGEN;
        } else if (strcmp(argv[i], "--help") == 0) {
            display_help(argv[0]);
            return 0;
        } else if (source_path == NULL) {
            size_t len = strlen(argv[i]);
            if (len > 2 && strcmp(&argv[i][len - 2], C_FILE_EXTENSION) == 0) {
                source_path = argv[i];
            } else {
                fprintf(stderr, "Error: input file must end in .c\n");
                return 2;
            }
        } else {
            fprintf(stderr, "Error: unrecognized argument: %s\n", argv[i]);
            return 2;
        }
    }

    if (source_path == NULL) {
        fprintf(stderr, "Error: no input file\n");
        display_help(argv[0]);
        return 2;
    }

    // --- Lex ---

    FILE* src = fopen(source_path, "r");
    if (src == NULL) {
        fprintf(stderr, "Error: cannot open %s\n", source_path);
        return 2;
    }

    token_list tokens = token_list_create(32);
    if (tokenize_file(src, &tokens) != ERR_OK) {
        fprintf(stderr, "Error: failed to tokenize %s\n", source_path);
        fclose(src);
        token_list_destroy(&tokens);
        return 1;
    }
    fclose(src);

    if (stage == STAGE_LEX) {
        token_list_destroy(&tokens);
        return 0;
    }

    // --- Parse ---

    Parser parser = parser_create(&tokens);
    Program* program = parse_program(&parser);

    if (stage == STAGE_PARSE) {
        destroy_program(program);
        token_list_destroy(&tokens);
        return 0;
    }

    // --- Codegen ---

    AsmProgram* asm_prog = codegen(program);

    // Build the .s output path from the source path
    size_t src_len = strlen(source_path);
    char* asm_path = malloc(src_len + 1); // .c -> .s (same length)
    memcpy(asm_path, source_path, src_len - 2);
    asm_path[src_len - 2] = '.';
    asm_path[src_len - 1] = 's';
    asm_path[src_len] = '\0';

    FILE* asm_out = fopen(asm_path, "w");
    if (asm_out == NULL) {
        fprintf(stderr, "Error: cannot open %s for writing\n", asm_path);
        free(asm_path);
        destroy_asm_program(asm_prog);
        destroy_program(program);
        token_list_destroy(&tokens);
        return 2;
    }

    emit_arm64(asm_prog, asm_out);
    fclose(asm_out);

    if (stage == STAGE_CODEGEN) {
        free(asm_path);
        destroy_asm_program(asm_prog);
        destroy_program(program);
        token_list_destroy(&tokens);
        return 0;
    }

    // --- Assemble & Link ---

    char* exe_path = malloc(src_len - 1);
    memcpy(exe_path, source_path, src_len - 2);
    exe_path[src_len - 2] = '\0';

    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "cc -o %s %s", exe_path, asm_path);
    int ret = system(cmd);

    // Clean up the .s file after assembling
    remove(asm_path);

    free(asm_path);
    free(exe_path);
    destroy_asm_program(asm_prog);
    destroy_program(program);
    token_list_destroy(&tokens);

    if (ret != 0) {
        fprintf(stderr, "Error: assembler/linker failed\n");
        return 1;
    }

    return 0;
}
