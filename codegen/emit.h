#pragma once

#include <stdio.h>
#include "asm_ast.h"

// Emit ARM64 assembly text from the assembly AST.
void emit_arm64(AsmProgram* prog, FILE* out);
