#pragma once

#include <stdio.h>
#include "x86/x86_ast.h"

// Emit assembly text from the assembly AST.
void emit_asm(x86_Program* prog, FILE* out);
