#pragma once

#include "asm_ast.h"
#include "../parser/ast.h"

// Translate high-level AST to assembly AST.
AsmProgram* codegen(Program* program);
