#pragma once

#include <stdlib.h>

typedef struct {
	char **items;
	size_t count;
	size_t capacity;
} StringList;

StringList string_list_create(size_t capacity);
void string_list_destroy(StringList *list);
void string_list_push(StringList *list, char *s);
