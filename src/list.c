#include "list.h"

TokenList token_list_create(size_t capacity) {
	TokenList list;
	list.count = 0;
	list.capacity = capacity;
	list.items = malloc(capacity * sizeof(Token));
	return list;
}

void token_list_destroy(TokenList *list) {
	for (size_t i = 0; i < list->count; i++) {
		if (list->items[i].kind == TOK_IDENTIFIER)
			free(list->items[i].ident);
	}
	free(list->items);
	list->items = NULL;
	list->count = 0;
	list->capacity = 0;
}

void token_list_push(TokenList *list, Token tok) {
	if (list->count == list->capacity) {
		list->capacity *= 2;
		list->items = realloc(list->items, list->capacity * sizeof(Token));
	}
	list->items[list->count++] = tok;
}

StringList string_list_create(size_t capacity) {
	StringList list;
	list.count = 0;
	list.capacity = capacity;
	list.items = malloc(capacity * sizeof(char *));
	return list;
}

void string_list_destroy(StringList *list) {
	free(list->items);
	list->items = NULL;
	list->count = 0;
	list->capacity = 0;
}

void string_list_push(StringList *list, char *s) {
	if (list->count == list->capacity) {
		list->capacity *= 2;
		list->items = realloc(list->items, list->capacity * sizeof(char *));
	}
	list->items[list->count++] = s;
}
