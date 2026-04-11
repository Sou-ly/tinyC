#pragma once

#include <stdlib.h>
#include "token.h"

typedef struct {
	token_kind *items;
	size_t count;
	size_t capacity;
} token_list;

token_list token_list_create(size_t capacity);
void token_list_destroy(token_list *list);
void token_list_push(token_list *list, token_kind tok);

typedef struct {
	char **items;
	size_t count;
	size_t capacity;
} string_list;

string_list string_list_create(size_t capacity);
void string_list_destroy(string_list *list);
void string_list_push(string_list *list, char *s);
