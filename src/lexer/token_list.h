#pragma once

#include <stdlib.h>
#include "token.h"

typedef struct TokenList {
	Token *items;
	size_t count;
	size_t capacity;
} TokenList;

TokenList token_list_create(size_t capacity);
// Frees each token's owned text, then the items array. Pushing transfers
// text ownership to the list — callers must not free a token they pushed.
void token_list_destroy(TokenList *list);
void token_list_push(TokenList *list, Token tok);
