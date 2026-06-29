#pragma once

#include "ast.h"
#include "../lexer/token.h"
#include "../lexer/token_list.h"

typedef struct {
    TokenList* tokens;
    int pos;
} Parser;

Parser parser_create(TokenList* tokens);

// Parse a full program (one or more declarations).
AstProgram parse_program(Parser* p);

// --- Variable resolution ---
// Maps original variable names to unique names so that
// duplicate declarations are caught and each variable gets
// a distinct identity in the IR.

typedef struct {
    char* key;
    char* val;
} VarMapEntry;

typedef struct {
    VarMapEntry* entries;
    int size;
    int capacity;
} VarMap;

VarMap varmap_create(int capacity);
void varmap_destroy(VarMap* map);
char* varmap_get(VarMap* map, const char* key);
void varmap_put(VarMap* map, const char* key, const char* val);

AstProgram resolve_variables(AstProgram program);
