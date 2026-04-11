#pragma once

#include <stdlib.h>
#include <stdbool.h>
#include "list.h"

typedef struct {
	char *data;
	size_t len;
} str;

// create / destroy
str str_from(const char *cstr);
str str_from_len(const char *data, size_t len);
str str_copy(str s);
void str_free(str *s);

// access
const char *str_cstr(str s);
bool str_empty(str s);

// comparison
bool str_eq(str a, str b);
bool str_eq_cstr(str a, const char *b);

// search
bool str_starts_with(str s, const char *prefix);
bool str_ends_with(str s, const char *suffix);
bool str_contains(str s, const char *needle);

// modification (returns new str, originals untouched)
str str_substr(str s, size_t start, size_t len);
str str_trim(str s);

// splitting
size_t str_split(str s, const char *delimiters, string_list *out);
