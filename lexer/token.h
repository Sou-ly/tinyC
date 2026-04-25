#pragma once

typedef enum {
    ERR_OK = 0,
    ERR_FILE_NOT_FOUND,
    ERR_FILE_READ,
    ERR_UNEXPECTED_CHAR,
    ERR_NO_MEMORY
} lexer_err;

typedef enum {
	TOK_INT,
	TOK_MAIN,
	TOK_LPAR,
	TOK_VOID,
	TOK_RPAR,
	TOK_LBRACE,
	TOK_RETURN,
	TOK_INT,
	TOK_SEMICOLON,
	TOK_RBRACE,
} token_kind;

extern const char *token_names[];

struct token_list;

lexer_err str_to_token(const char *str, token_kind *token);

lexer_err tokenize(const char *str, struct token_list *tokens);

lexer_err tokenize(const FILE* src, struct token_list *tokens);

