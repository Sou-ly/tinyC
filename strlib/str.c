#include "str.h"

static size_t cstr_len(const char *s) {
	size_t len = 0;
	while (s[len]) len++;
	return len;
}

static void mem_copy(char *dst, const char *src, size_t n) {
	for (size_t i = 0; i < n; i++) dst[i] = src[i];
}

static bool mem_equal(const char *a, const char *b, size_t n) {
	for (size_t i = 0; i < n; i++) {
		if (a[i] != b[i]) return false;
	}
	return true;
}

static bool is_space(char c) {
	return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

static bool is_delim(char c, const char *delimiters) {
	for (size_t i = 0; delimiters[i]; i++) {
		if (c == delimiters[i]) return true;
	}
	return false;
}

str str_from(const char *cstr) {
	if (cstr == NULL) {
		return (str){NULL, 0};
	}
	size_t len = cstr_len(cstr);
	char *data = malloc(len + 1);
	mem_copy(data, cstr, len + 1);
	return (str){data, len};
}

str str_from_len(const char *data, size_t len) {
	char *buf = malloc(len + 1);
	mem_copy(buf, data, len);
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
	return mem_equal(a.data, b.data, a.len);
}

bool str_eq_cstr(str a, const char *b) {
	size_t blen = cstr_len(b);
	if (a.len != blen) return false;
	return mem_equal(a.data, b, a.len);
}

bool str_starts_with(str s, const char *prefix) {
	size_t plen = cstr_len(prefix);
	if (s.len < plen) return false;
	return mem_equal(s.data, prefix, plen);
}

bool str_ends_with(str s, const char *suffix) {
	size_t slen = cstr_len(suffix);
	if (s.len < slen) return false;
	return mem_equal(s.data + s.len - slen, suffix, slen);
}

bool str_contains(str s, const char *needle) {
	size_t nlen = cstr_len(needle);
	if (nlen == 0) return true;
	if (s.len < nlen) return false;
	for (size_t i = 0; i <= s.len - nlen; i++) {
		if (mem_equal(s.data + i, needle, nlen)) return true;
	}
	return false;
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
	while (start < end && is_space(s.data[start])) start++;
	while (end > start && is_space(s.data[end - 1])) end--;
	return str_from_len(s.data + start, end - start);
}

size_t str_split(str s, const char *delimiters, string_list *out) {
	if (s.len == 0) return 0;

	size_t count = 0;
	size_t i = 0;

	while (i < s.len) {
		// skip delimiters
		while (i < s.len && is_delim(s.data[i], delimiters)) i++;
		if (i >= s.len) break;

		// find end of token
		size_t start = i;
		while (i < s.len && !is_delim(s.data[i], delimiters)) i++;

		// copy token
		str token = str_from_len(s.data + start, i - start);
		string_list_push(out, token.data);
		count++;
	}

	return count;
}
