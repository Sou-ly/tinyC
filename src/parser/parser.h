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

typedef struct {
    char* key;
    x86_Variable val;
} VariableEntry;

typedef struct {
    VariableEntry* entries;
    int size;
    int capacity;
    int stack_offset;
} VariableMap;

AstDeclaration resolve_declaration(AstDeclaration declaration, VariableMap* map);
AstStatement resolve_statement(AstStatement stmt);
AstExpression resolve_expression(AstExpression exp);
AstProgram resolve_variables(AstProgram program);
