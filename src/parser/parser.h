#pragma once

#include "ast.h"
#include "../lexer/token.h"

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
	bool is_cur_scope;
} VarMapEntry;

typedef struct {
    VarMapEntry* entries;
    int size;
    int capacity;
} VarMap;

VarMap varmap_create(int capacity);
VarMap varmap_copy(VarMap map);
void varmap_destroy(VarMap* map);
VarMapEntry* varmap_get(VarMap* map, const char* key);
void varmap_put(VarMap* map, VarMapEntry entry);

void resolve_variables(AstProgram* program);

// --- Typechecking ---
// Runs after resolve_variables: every variable name is program-unique and
// functions keep their source names, so one flat table covers the whole
// program. Maps each name to what it is — an int variable, or a function
// and its arity.

typedef enum { SYM_INT, SYM_FUNCTION } SymbolKind;

typedef struct {
    char* key;
    SymbolKind kind;
    size_t param_count;	// SYM_FUNCTION only
    bool defined;		// SYM_FUNCTION only: a definition (body) has been seen
} Symbol;

typedef struct {
    Symbol* entries;
    int size;
    int capacity;
} SymbolTable;

SymbolTable symtab_create(int capacity);
void symtab_destroy(SymbolTable* table);
Symbol* symtab_get(SymbolTable* table, const char* key);
void symtab_put(SymbolTable* table, Symbol symbol);

// Rejects, with an error on stderr and exit(1):
//   - calling a variable like a function / using a function as a variable
//   - redeclaring a function with a different number of parameters
//   - calling a function with the wrong number of arguments
//   - defining the same function more than once
void typecheck(AstProgram* program);

void resolve_labels(AstProgram* program);

// --- goto / label resolution ---
// Renames every source label to a program-unique name and rewrites each goto to
// its target's unique name, rejecting a goto with no matching label and two
// labels sharing a name in one function.
void resolve_goto_labels(AstProgram* program);
