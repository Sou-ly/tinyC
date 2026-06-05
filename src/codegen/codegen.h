#pragma once

#include "x86/x86_ast.h"
#include "../parser/ast.h"

// Translate high-level AST to assembly AST.
x86_Program* codegen(AstProgram* program);
