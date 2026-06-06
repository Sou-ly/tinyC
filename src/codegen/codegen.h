#pragma once

#include "x86/x86_ast.h"
#include "../ir/ir.h"

// Translate IR to assembly AST.
x86_Program codegen(IrProgram* program);
