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
} LexerErr;

typedef enum {
	TOK_LPAR,
	TOK_RPAR,
	TOK_LBRACE,
	TOK_RBRACE,
	TOK_COMMA,
	TOK_SEMICOLON,
} TokenSeparator;

typedef enum {
	// arithmetic
	TOK_PLUS,
	TOK_MINUS,
	TOK_STAR,
	TOK_FSLASH,
	TOK_PERCENT,
	TOK_DECR,
	TOK_INCR,
	// bitwise
	TOK_AND,
	TOK_OR,
	TOK_XOR,
	TOK_LSHIFT,
	TOK_RSHIFT,
	TOK_NOT,
	// logical
	TOK_LNOT,
	TOK_LAND,
	TOK_LOR,
	TOK_EQ,
	TOK_NEQ,
	TOK_LESS,
	TOK_GREATER,
	TOK_LEQ,
	TOK_GEQ,
	// other
	TOK_ASSIGN,
	TOK_PLUS_EQ,
	TOK_MINUS_EQ,
	TOK_MUL_EQ,
	TOK_DIV_EQ,
	TOK_MOD_EQ,
	TOK_AND_EQ,
	TOK_OR_EQ,
	TOK_XOR_EQ,
	TOK_RSHIFT_EQ,
	TOK_LSHIFT_EQ
} TokenOperator;

typedef enum {
	TOK_IF,
	TOK_INT,
	TOK_RETURN,
	TOK_VOID,
} TokenKeyword;

typedef enum {
	TOK_SEPARATOR,
	TOK_OPERATOR,
	TOK_KEYWORD,
	TOK_IDENTIFIER,
	TOK_INT_LITERAL,
} TokenKind;

typedef struct {
	TokenKind kind;
	union {
		TokenSeparator sep;
		TokenOperator  op;
		TokenKeyword   kw;
		char          *ident;
		int            int_val;
	};
	size_t line;
	size_t col;
} Token;

// Display names for diagnostics. Out-of-range input returns "<unknown>".
// Never use these as lookup keys — they're for error messages only.
const char *token_kind_name(TokenKind kind);
const char *separator_name(TokenSeparator s);
const char *operator_name(TokenOperator o);
const char *keyword_name(TokenKeyword k);

struct TokenList;

// Lex a NUL-terminated source buffer.
LexerErr tokenize(const char *source, struct TokenList *tokens);

// Convenience wrapper: read the file into memory, then tokenize.
LexerErr tokenize_file(FILE *src, struct TokenList *tokens);
