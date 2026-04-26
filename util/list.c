#include "list.h"

token_list token_list_create(size_t capacity) {
	token_list list;
	list.count = 0;
	list.capacity = capacity;
	list.items = malloc(capacity * sizeof(token_kind));
	return list;
}

void token_list_destroy(token_list *list) {
	free(list->items);
	list->items = NULL;
	list->count = 0;
	list->capacity = 0;
}

void token_list_push(token_list *list, token_kind tok) {
	if (list->count == list->capacity) {
		list->capacity *= 2;
		list->items = realloc(list->items, list->capacity * sizeof(token_kind));
	}
	list->items[list->count++] = tok;
}

string_list string_list_create(size_t capacity) {
	string_list list;
	list.count = 0;
	list.capacity = capacity;
	list.items = malloc(capacity * sizeof(char *));
	return list;
}

void string_list_destroy(string_list *list) {
	free(list->items);
	list->items = NULL;
	list->count = 0;
	list->capacity = 0;
}

void string_list_push(string_list *list, char *s) {
	if (list->count == list->capacity) {
		list->capacity *= 2;
		list->items = realloc(list->items, list->capacity * sizeof(char *));
	}
	list->items[list->count++] = s;
}
