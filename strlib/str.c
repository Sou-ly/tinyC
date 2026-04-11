#include "str.h"
#include "list.h"
#include <string.h>
#include <ctype.h>

str str_from(const char *cstr) {
	if (cstr == NULL) {
		return (str){NULL, 0};
	}
	size_t len = strlen(cstr);
	char *data = malloc(len + 1);
	memcpy(data, cstr, len + 1);
	return (str){data, len};
}

str str_from_len(const char *data, size_t len) {
	char *buf = malloc(len + 1);
	memcpy(buf, data, len);
	buf[len] = '\0';
	return (str){buf, len};
}

str str_copy(str s) {
	return str_from_len(s.data, s.len);
}

void str_free(str *s) {
	free(s->data);
	s->data = NULL;
	s->len = 0;
}

const char *str_cstr(str s) {
	return s.data;
}

bool str_empty(str s) {
	return s.len == 0;
}

bool str_eq(str a, str b) {
	if (a.len != b.len) return false;
	return memcmp(a.data, b.data, a.len) == 0;
}

bool str_eq_cstr(str a, const char *b) {
	size_t blen = strlen(b);
	if (a.len != blen) return false;
	return memcmp(a.data, b, a.len) == 0;
}

bool str_starts_with(str s, const char *prefix) {
	size_t plen = strlen(prefix);
	if (s.len < plen) return false;
	return memcmp(s.data, prefix, plen) == 0;
}

bool str_ends_with(str s, const char *suffix) {
	size_t slen = strlen(suffix);
	if (s.len < slen) return false;
	return memcmp(s.data + s.len - slen, suffix, slen) == 0;
}

bool str_contains(str s, const char *needle) {
	if (s.data == NULL) return false;
	return strstr(s.data, needle) != NULL;
}

str str_substr(str s, size_t start, size_t len) {
	if (start >= s.len) return (str){NULL, 0};
	if (start + len > s.len) len = s.len - start;
	return str_from_len(s.data + start, len);
}

str str_trim(str s) {
	if (s.len == 0) return str_from("");
	size_t start = 0;
	size_t end = s.len;
	while (start < end && isspace((unsigned char)s.data[start])) start++;
	while (end > start && isspace((unsigned char)s.data[end - 1])) end--;
	return str_from_len(s.data + start, end - start);
}

size_t str_split(str s, const char *delimiters, string_list *out) {
	if (s.len == 0) return 0;
	// work on a copy since strtok modifies the string
	char *copy = malloc(s.len + 1);
	memcpy(copy, s.data, s.len + 1);

	size_t count = 0;
	char *tok = strtok(copy, delimiters);
	while (tok != NULL) {
		// push a copy of each token since the working copy will be freed
		str token = str_from(tok);
		string_list_push(out, token.data);
		count++;
		tok = strtok(NULL, delimiters);
	}
	free(copy);
	return count;
}
