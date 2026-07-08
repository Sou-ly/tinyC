#pragma once

#include <stdlib.h>

#define LIST_OF(T) struct { T* items; size_t count; size_t capacity; }

// Tagged variant, for types that need forward-declaring (an anonymous LIST_OF
// typedef cannot be).
#define LIST_TYPE(Name, T) struct Name { T* items; size_t count; size_t capacity; }

// Evaluates (list) more than once — pass a plain lvalue pointer.
#define list_push(list, item)                                                  \
	do {                                                                       \
		if ((list)->count >= (list)->capacity) {                               \
			(list)->capacity = (list)->capacity ? (list)->capacity * 2 : 4;    \
			(list)->items = realloc((list)->items,                             \
			                        (list)->capacity * sizeof(*(list)->items));\
		}                                                                      \
		(list)->items[(list)->count++] = (item);                              \
	} while (0)

// Frees the backing array only, not what elements own.
#define list_free(list)                                                        \
	do {                                                                       \
		free((list)->items);                                                   \
		(list)->items = NULL;                                                  \
		(list)->count = 0;                                                      \
		(list)->capacity = 0;                                                  \
	} while (0)
