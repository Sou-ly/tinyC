#pragma once

#include "ast.h"
#include "../lexer/token.h"
#include "../list.h"

typedef struct {
    token_list* tokens;
    int pos;
} Parser;

Parser parser_create(token_list* tokens);

// Parse a full program (one or more declarations).
AstProgram parse_program(Parser* p);
