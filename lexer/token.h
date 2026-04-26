#pragma once

#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include "../strlib/str.h"

typedef enum {
	ERR_OK = 0,
	ERR_FILE_NOT_FOUND,
	ERR_FILE_READ,
	ERR_UNEXPECTED_CHAR,
	ERR_NO_MEMORY,
} lexer_err;

typedef enum {
	SEP_LPAR,
	SEP_RPAR,
	SEP_LBRACE,
	SEP_RBRACE,
	SEP_COMMA,
	SEP_SEMICOLON,
} token_separator;

typedef enum {
	OP_PLUS,
	OP_MINUS,
	OP_EQ,
	OP_NEQ,
	OP_AND,
	OP_OR,
} token_operator;

typedef enum {
	KW_IF,
	KW_INT,
	KW_RETURN,
	KW_VOID,
} token_keyword;

typedef enum {
	TOK_SEPARATOR,
	TOK_OPERATOR,
	TOK_KEYWORD,
	TOK_IDENTIFIER,
	TOK_INT_LITERAL,
} token_kind;

typedef struct {
	token_kind kind;
	union {
		token_separator sep;
		token_operator  op;
		token_keyword   kw;
	} as;
	char * text;
	size_t line;
	size_t col;
} token;

void token_free(token *t);

// Display names for diagnostics. Out-of-range input returns "<unknown>".
// Never use these as lookup keys — they're for error messages only.
const char *token_kind_name(token_kind kind);
const char *separator_name(token_separator s);
const char *operator_name(token_operator o);
const char *keyword_name(token_keyword k);

struct token_list;

// Lex a NUL-terminated source buffer.
lexer_err tokenize(const char *source, struct token_list *tokens);

// Convenience wrapper: read the file into memory, then tokenize.
lexer_err tokenize_file(FILE *src, struct token_list *tokens);
