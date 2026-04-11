#pragma once

typedef enum {
	TOK_INT,
	TOK_MAIN,
	TOK_LPAR,
	TOK_VOID,
	TOK_RPAR,
	TOK_LBRACE,
	TOK_RETURN,
	TOK_INT_LITERAL,
	TOK_SEMICOLON,
	TOK_RBRACE,
	TOK_COUNT
} token_kind;

extern const char *token_names[];
