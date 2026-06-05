#pragma once

#include <stdlib.h>
#include "lexer/token.h"

typedef struct token_list {
	token *items;
	size_t count;
	size_t capacity;
} token_list;

token_list token_list_create(size_t capacity);
// Frees each token's owned text, then the items array. Pushing transfers
// text ownership to the list — callers must not free a token they pushed.
void token_list_destroy(token_list *list);
void token_list_push(token_list *list, token tok);

typedef struct {
	char **items;
	size_t count;
	size_t capacity;
} string_list;

string_list string_list_create(size_t capacity);
void string_list_destroy(string_list *list);
void string_list_push(string_list *list, char *s);
