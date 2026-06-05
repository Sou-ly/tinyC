#pragma once

#include "ast.h"
#include "../lexer/token.h"
#include "../list.h"

typedef struct {
    token* tokens;
    int num_tokens;
    int pos;
} Parser;

Parser parser_create(token_list* tokens);

// Parse a full program (one or more declarations).
AstProgram* parse_program(Parser* p);
