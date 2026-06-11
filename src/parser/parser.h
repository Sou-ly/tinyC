#pragma once

#include "ast.h"
#include "../lexer/token.h"
#include "../list.h"

typedef struct {
    TokenList* tokens;
    int pos;
} Parser;

Parser parser_create(TokenList* tokens);

// Parse a full program (one or more declarations).
AstProgram parse_program(Parser* p);
