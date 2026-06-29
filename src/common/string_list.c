#include "string_list.h"

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
