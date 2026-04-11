#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../str.h"

// ---- create / destroy ----

void test_str_from() {
	str s = str_from("hello");
	assert(s.len == 5);
	assert(strcmp(s.data, "hello") == 0);
	str_free(&s);
	printf("  PASS: str_from\n");
}

void test_str_from_null() {
	str s = str_from(NULL);
	assert(s.data == NULL);
	assert(s.len == 0);
	printf("  PASS: str_from_null\n");
}

void test_str_from_empty() {
	str s = str_from("");
	assert(s.len == 0);
	assert(strcmp(s.data, "") == 0);
	str_free(&s);
	printf("  PASS: str_from_empty\n");
}

void test_str_from_len() {
	str s = str_from_len("hello world", 5);
	assert(s.len == 5);
	assert(strcmp(s.data, "hello") == 0);
	str_free(&s);
	printf("  PASS: str_from_len\n");
}

void test_str_copy() {
	str a = str_from("test");
	str b = str_copy(a);
	assert(str_eq(a, b));
	assert(a.data != b.data);  // different allocations
	str_free(&a);
	str_free(&b);
	printf("  PASS: str_copy\n");
}

void test_str_free() {
	str s = str_from("gone");
	str_free(&s);
	assert(s.data == NULL);
	assert(s.len == 0);
	printf("  PASS: str_free\n");
}

// ---- access ----

void test_str_cstr() {
	str s = str_from("hello");
	assert(strcmp(str_cstr(s), "hello") == 0);
	str_free(&s);
	printf("  PASS: str_cstr\n");
}

void test_str_empty() {
	str a = str_from("");
	str b = str_from("x");
	assert(str_empty(a));
	assert(!str_empty(b));
	str_free(&a);
	str_free(&b);
	printf("  PASS: str_empty\n");
}

// ---- comparison ----

void test_str_eq_same() {
	str a = str_from("hello");
	str b = str_from("hello");
	assert(str_eq(a, b));
	str_free(&a);
	str_free(&b);
	printf("  PASS: str_eq_same\n");
}

void test_str_eq_different() {
	str a = str_from("hello");
	str b = str_from("world");
	assert(!str_eq(a, b));
	str_free(&a);
	str_free(&b);
	printf("  PASS: str_eq_different\n");
}

void test_str_eq_different_lengths() {
	str a = str_from("hi");
	str b = str_from("hello");
	assert(!str_eq(a, b));
	str_free(&a);
	str_free(&b);
	printf("  PASS: str_eq_different_lengths\n");
}

void test_str_eq_cstr() {
	str s = str_from("--lex");
	assert(str_eq_cstr(s, "--lex"));
	assert(!str_eq_cstr(s, "--parse"));
	assert(!str_eq_cstr(s, "--le"));
	str_free(&s);
	printf("  PASS: str_eq_cstr\n");
}

// ---- search ----

void test_str_starts_with() {
	str s = str_from("hello world");
	assert(str_starts_with(s, "hello"));
	assert(str_starts_with(s, ""));
	assert(!str_starts_with(s, "world"));
	assert(!str_starts_with(s, "hello world!!!"));
	str_free(&s);
	printf("  PASS: str_starts_with\n");
}

void test_str_ends_with() {
	str s = str_from("main.c");
	assert(str_ends_with(s, ".c"));
	assert(str_ends_with(s, ""));
	assert(!str_ends_with(s, ".h"));
	assert(!str_ends_with(s, "xxxmain.c"));
	str_free(&s);
	printf("  PASS: str_ends_with\n");
}

void test_str_contains() {
	str s = str_from("int main(void)");
	assert(str_contains(s, "main"));
	assert(str_contains(s, "int"));
	assert(str_contains(s, "void"));
	assert(!str_contains(s, "return"));
	str_free(&s);
	printf("  PASS: str_contains\n");
}

// ---- modification ----

void test_str_substr() {
	str s = str_from("hello world");
	str sub = str_substr(s, 6, 5);
	assert(str_eq_cstr(sub, "world"));
	str_free(&sub);
	str_free(&s);
	printf("  PASS: str_substr\n");
}

void test_str_substr_past_end() {
	str s = str_from("hi");
	str sub = str_substr(s, 1, 100);
	assert(str_eq_cstr(sub, "i"));
	str_free(&sub);
	str_free(&s);
	printf("  PASS: str_substr_past_end\n");
}

void test_str_substr_out_of_bounds() {
	str s = str_from("hi");
	str sub = str_substr(s, 10, 5);
	assert(sub.data == NULL);
	assert(sub.len == 0);
	str_free(&s);
	printf("  PASS: str_substr_out_of_bounds\n");
}

void test_str_trim() {
	str s = str_from("  hello  ");
	str t = str_trim(s);
	assert(str_eq_cstr(t, "hello"));
	str_free(&t);
	str_free(&s);
	printf("  PASS: str_trim\n");
}

void test_str_trim_tabs_newlines() {
	str s = str_from("\t\n  hello world \n\t");
	str t = str_trim(s);
	assert(str_eq_cstr(t, "hello world"));
	str_free(&t);
	str_free(&s);
	printf("  PASS: str_trim_tabs_newlines\n");
}

void test_str_trim_empty() {
	str s = str_from("   ");
	str t = str_trim(s);
	assert(str_empty(t));
	str_free(&t);
	str_free(&s);
	printf("  PASS: str_trim_empty\n");
}

void test_str_trim_no_whitespace() {
	str s = str_from("clean");
	str t = str_trim(s);
	assert(str_eq_cstr(t, "clean"));
	str_free(&t);
	str_free(&s);
	printf("  PASS: str_trim_no_whitespace\n");
}

// ---- splitting ----

void test_str_split_spaces() {
	str s = str_from("int main void");
	string_list sl = string_list_create(4);
	size_t count = str_split(s, " ", &sl);
	assert(count == 3);
	assert(sl.count == 3);
	assert(strcmp(sl.items[0], "int") == 0);
	assert(strcmp(sl.items[1], "main") == 0);
	assert(strcmp(sl.items[2], "void") == 0);
	for (size_t i = 0; i < sl.count; i++) free(sl.items[i]);
	string_list_destroy(&sl);
	str_free(&s);
	printf("  PASS: str_split_spaces\n");
}

void test_str_split_multiple_delimiters() {
	str s = str_from("int\tmain\n{return 0;}");
	string_list sl = string_list_create(4);
	size_t count = str_split(s, " \t\n", &sl);
	assert(count == 4);
	assert(strcmp(sl.items[0], "int") == 0);
	assert(strcmp(sl.items[1], "main") == 0);
	assert(strcmp(sl.items[2], "{return") == 0);
	assert(strcmp(sl.items[3], "0;}") == 0);
	for (size_t i = 0; i < sl.count; i++) free(sl.items[i]);
	string_list_destroy(&sl);
	str_free(&s);
	printf("  PASS: str_split_multiple_delimiters\n");
}

void test_str_split_empty() {
	str s = str_from("");
	string_list sl = string_list_create(4);
	size_t count = str_split(s, " ", &sl);
	assert(count == 0);
	assert(sl.count == 0);
	string_list_destroy(&sl);
	str_free(&s);
	printf("  PASS: str_split_empty\n");
}

void test_str_split_no_delimiters_found() {
	str s = str_from("hello");
	string_list sl = string_list_create(4);
	size_t count = str_split(s, ",", &sl);
	assert(count == 1);
	assert(strcmp(sl.items[0], "hello") == 0);
	free(sl.items[0]);
	string_list_destroy(&sl);
	str_free(&s);
	printf("  PASS: str_split_no_delimiters_found\n");
}

void test_str_split_leading_trailing() {
	str s = str_from("  hello  world  ");
	string_list sl = string_list_create(4);
	size_t count = str_split(s, " ", &sl);
	assert(count == 2);
	assert(strcmp(sl.items[0], "hello") == 0);
	assert(strcmp(sl.items[1], "world") == 0);
	for (size_t i = 0; i < sl.count; i++) free(sl.items[i]);
	string_list_destroy(&sl);
	str_free(&s);
	printf("  PASS: str_split_leading_trailing\n");
}

int main() {
	printf("str create/destroy tests:\n");
	test_str_from();
	test_str_from_null();
	test_str_from_empty();
	test_str_from_len();
	test_str_copy();
	test_str_free();

	printf("\nstr access tests:\n");
	test_str_cstr();
	test_str_empty();

	printf("\nstr comparison tests:\n");
	test_str_eq_same();
	test_str_eq_different();
	test_str_eq_different_lengths();
	test_str_eq_cstr();

	printf("\nstr search tests:\n");
	test_str_starts_with();
	test_str_ends_with();
	test_str_contains();

	printf("\nstr modification tests:\n");
	test_str_substr();
	test_str_substr_past_end();
	test_str_substr_out_of_bounds();
	test_str_trim();
	test_str_trim_tabs_newlines();
	test_str_trim_empty();
	test_str_trim_no_whitespace();

	printf("\nstr split tests:\n");
	test_str_split_spaces();
	test_str_split_multiple_delimiters();
	test_str_split_empty();
	test_str_split_no_delimiters_found();
	test_str_split_leading_trailing();

	printf("\nall tests passed\n");
	return 0;
}
