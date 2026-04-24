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

struct token_list;

int str_to_token(const char *str, token_kind *token);
int tokenize(const char *str, struct token_list *tokens);
