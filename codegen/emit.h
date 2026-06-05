#pragma once

#include <stdio.h>
#include "x86/x86_ast.h"

// Emit ARM64 assembly text from the assembly AST.
void emit_arm64(x86_Program* prog, FILE* out);
