#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../list.h"

// ---- token_list tests ----

void test_token_list_create() {
	token_list tl = token_list_create(4);
	assert(tl.items != NULL);
	assert(tl.count == 0);
	assert(tl.capacity == 4);
	token_list_destroy(&tl);
	printf("  PASS: token_list_create\n");
}

void test_token_list_push_single() {
	token_list tl = token_list_create(4);
	token_list_push(&tl, TOK_INT);
	assert(tl.count == 1);
	assert(tl.items[0] == TOK_INT);
	token_list_destroy(&tl);
	printf("  PASS: token_list_push_single\n");
}

void test_token_list_push_multiple() {
	token_list tl = token_list_create(4);
	token_list_push(&tl, TOK_INT);
	token_list_push(&tl, TOK_MAIN);
	token_list_push(&tl, TOK_LPAR);
	assert(tl.count == 3);
	assert(tl.items[0] == TOK_INT);
	assert(tl.items[1] == TOK_MAIN);
	assert(tl.items[2] == TOK_LPAR);
	token_list_destroy(&tl);
	printf("  PASS: token_list_push_multiple\n");
}

void test_token_list_push_triggers_realloc() {
	token_list tl = token_list_create(2);
	token_list_push(&tl, TOK_INT);
	token_list_push(&tl, TOK_MAIN);
	// this should trigger a realloc
	token_list_push(&tl, TOK_LPAR);
	assert(tl.count == 3);
	assert(tl.capacity == 4);
	assert(tl.items[0] == TOK_INT);
	assert(tl.items[1] == TOK_MAIN);
	assert(tl.items[2] == TOK_LPAR);
	token_list_destroy(&tl);
	printf("  PASS: token_list_push_triggers_realloc\n");
}

void test_token_list_push_many_reallocs() {
	token_list tl = token_list_create(1);
	for (int i = 0; i < 100; i++) {
		token_list_push(&tl, TOK_SEMICOLON);
	}
	assert(tl.count == 100);
	assert(tl.capacity >= 100);
	for (int i = 0; i < 100; i++) {
		assert(tl.items[i] == TOK_SEMICOLON);
	}
	token_list_destroy(&tl);
	printf("  PASS: token_list_push_many_reallocs\n");
}

void test_token_list_destroy() {
	token_list tl = token_list_create(4);
	token_list_push(&tl, TOK_INT);
	token_list_push(&tl, TOK_RETURN);
	token_list_destroy(&tl);
	assert(tl.items == NULL);
	assert(tl.count == 0);
	assert(tl.capacity == 0);
	printf("  PASS: token_list_destroy\n");
}

void test_token_list_preserves_order() {
	token_kind expected[] = {
		TOK_INT, TOK_MAIN, TOK_LPAR, TOK_VOID, TOK_RPAR,
		TOK_LBRACE, TOK_RETURN, TOK_INT_LITERAL, TOK_SEMICOLON, TOK_RBRACE
	};
	size_t n = sizeof(expected) / sizeof(expected[0]);

	token_list tl = token_list_create(2);
	for (size_t i = 0; i < n; i++) {
		token_list_push(&tl, expected[i]);
	}
	assert(tl.count == n);
	for (size_t i = 0; i < n; i++) {
		assert(tl.items[i] == expected[i]);
	}
	token_list_destroy(&tl);
	printf("  PASS: token_list_preserves_order\n");
}

// ---- string_list tests ----

void test_string_list_create() {
	string_list sl = string_list_create(4);
	assert(sl.items != NULL);
	assert(sl.count == 0);
	assert(sl.capacity == 4);
	string_list_destroy(&sl);
	printf("  PASS: string_list_create\n");
}

void test_string_list_push_single() {
	string_list sl = string_list_create(4);
	string_list_push(&sl, "hello");
	assert(sl.count == 1);
	assert(strcmp(sl.items[0], "hello") == 0);
	string_list_destroy(&sl);
	printf("  PASS: string_list_push_single\n");
}

void test_string_list_push_multiple() {
	string_list sl = string_list_create(4);
	string_list_push(&sl, "int");
	string_list_push(&sl, "main");
	string_list_push(&sl, "return");
	assert(sl.count == 3);
	assert(strcmp(sl.items[0], "int") == 0);
	assert(strcmp(sl.items[1], "main") == 0);
	assert(strcmp(sl.items[2], "return") == 0);
	string_list_destroy(&sl);
	printf("  PASS: string_list_push_multiple\n");
}

void test_string_list_push_triggers_realloc() {
	string_list sl = string_list_create(2);
	string_list_push(&sl, "a");
	string_list_push(&sl, "b");
	// should trigger realloc
	string_list_push(&sl, "c");
	assert(sl.count == 3);
	assert(sl.capacity == 4);
	assert(strcmp(sl.items[0], "a") == 0);
	assert(strcmp(sl.items[1], "b") == 0);
	assert(strcmp(sl.items[2], "c") == 0);
	string_list_destroy(&sl);
	printf("  PASS: string_list_push_triggers_realloc\n");
}

void test_string_list_push_many_reallocs() {
	string_list sl = string_list_create(1);
	for (int i = 0; i < 100; i++) {
		string_list_push(&sl, "tok");
	}
	assert(sl.count == 100);
	assert(sl.capacity >= 100);
	for (int i = 0; i < 100; i++) {
		assert(strcmp(sl.items[i], "tok") == 0);
	}
	string_list_destroy(&sl);
	printf("  PASS: string_list_push_many_reallocs\n");
}

void test_string_list_destroy() {
	string_list sl = string_list_create(4);
	string_list_push(&sl, "x");
	string_list_push(&sl, "y");
	string_list_destroy(&sl);
	assert(sl.items == NULL);
	assert(sl.count == 0);
	assert(sl.capacity == 0);
	printf("  PASS: string_list_destroy\n");
}

void test_string_list_preserves_order() {
	char *expected[] = {"int", "main", "(", "void", ")", "{", "return", "2", ";", "}"};
	size_t n = sizeof(expected) / sizeof(expected[0]);

	string_list sl = string_list_create(2);
	for (size_t i = 0; i < n; i++) {
		string_list_push(&sl, expected[i]);
	}
	assert(sl.count == n);
	for (size_t i = 0; i < n; i++) {
		assert(strcmp(sl.items[i], expected[i]) == 0);
	}
	string_list_destroy(&sl);
	printf("  PASS: string_list_preserves_order\n");
}

void test_string_list_stores_pointers_not_copies() {
	char buf[] = "hello";
	string_list sl = string_list_create(4);
	string_list_push(&sl, buf);
	// modifying buf should be visible through the list
	buf[0] = 'H';
	assert(strcmp(sl.items[0], "Hello") == 0);
	string_list_destroy(&sl);
	printf("  PASS: string_list_stores_pointers_not_copies\n");
}

int main() {
	printf("token_list tests:\n");
	test_token_list_create();
	test_token_list_push_single();
	test_token_list_push_multiple();
	test_token_list_push_triggers_realloc();
	test_token_list_push_many_reallocs();
	test_token_list_destroy();
	test_token_list_preserves_order();

	printf("\nstring_list tests:\n");
	test_string_list_create();
	test_string_list_push_single();
	test_string_list_push_multiple();
	test_string_list_push_triggers_realloc();
	test_string_list_push_many_reallocs();
	test_string_list_destroy();
	test_string_list_preserves_order();
	test_string_list_stores_pointers_not_copies();

	printf("\nall tests passed\n");
	return 0;
}
