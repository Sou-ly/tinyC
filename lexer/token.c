#include <stdlib.h>
#include <string.h>
#include "token.h"
#include "../list.h"

const char *token_names[] = {
	[TOK_INT]         = "int",
	[TOK_MAIN]        = "main",
	[TOK_LPAR]        = "(",
	[TOK_VOID]        = "void",
	[TOK_RPAR]        = ")",
	[TOK_LBRACE]      = "{",
	[TOK_RETURN]      = "return",
	[TOK_INT_LITERAL] = "2",
	[TOK_SEMICOLON]   = ";",
	[TOK_RBRACE]      = "}",
};

int str_to_token(const char *str, token_kind *token) {
	for (int kind = TOK_INT; kind < TOK_COUNT; kind++) {
		if (!strcmp(token_names[kind], str)) {
			*token = (token_kind)kind;
			return 0;
		}
	}
	return -1;
}

int tokenize(const char *str, token_list *tokens) {
	char *copy = strdup(str);
	if (!copy) return -1;

	char *word = strtok(copy, " \n\t\r");
	while (word != NULL) {
		token_kind token;
		if (str_to_token(word, &token) != 0) {
			free(copy);
			return -1;
		}
		token_list_push(tokens, token);
		word = strtok(NULL, " \n\t\r");
	}

	free(copy);
	return 0;
}
