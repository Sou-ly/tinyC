#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../lexer/token.h"
#include "../list.h"

// ---- str_to_token tests ----

void test_str_to_token_keyword() {
	token_kind t;
	assert(str_to_token("int", &t) == 0);
	assert(t == TOK_INT);
	assert(str_to_token("main", &t) == 0);
	assert(t == TOK_MAIN);
	assert(str_to_token("void", &t) == 0);
	assert(t == TOK_VOID);
	assert(str_to_token("return", &t) == 0);
	assert(t == TOK_RETURN);
	printf("  PASS: str_to_token_keyword\n");
}

void test_str_to_token_punct() {
	token_kind t;
	assert(str_to_token("(", &t) == 0);
	assert(t == TOK_LPAR);
	assert(str_to_token(")", &t) == 0);
	assert(t == TOK_RPAR);
	assert(str_to_token("{", &t) == 0);
	assert(t == TOK_LBRACE);
	assert(str_to_token("}", &t) == 0);
	assert(t == TOK_RBRACE);
	assert(str_to_token(";", &t) == 0);
	assert(t == TOK_SEMICOLON);
	printf("  PASS: str_to_token_punct\n");
}

void test_str_to_token_int_literal() {
	token_kind t;
	assert(str_to_token("2", &t) == 0);
	assert(t == TOK_INT_LITERAL);
	printf("  PASS: str_to_token_int_literal\n");
}

void test_str_to_token_unknown() {
	token_kind t;
	assert(str_to_token("banana", &t) == -1);
	assert(str_to_token("", &t) == -1);
	assert(str_to_token("Int", &t) == -1);
	printf("  PASS: str_to_token_unknown\n");
}

// ---- tokenize tests ----

void test_tokenize_full_program() {
	token_kind expected[] = {
		TOK_INT, TOK_MAIN, TOK_LPAR, TOK_VOID, TOK_RPAR,
		TOK_LBRACE, TOK_RETURN, TOK_INT_LITERAL, TOK_SEMICOLON, TOK_RBRACE
	};
	size_t n = sizeof(expected) / sizeof(expected[0]);

	char src[] = "int main ( void ) { return 2 ; }";
	token_list tl = token_list_create(4);
	assert(tokenize(src, &tl) == 0);
	assert(tl.count == n);
	for (size_t i = 0; i < n; i++) {
		assert(tl.items[i] == expected[i]);
	}
	token_list_destroy(&tl);
	printf("  PASS: tokenize_full_program\n");
}

void test_tokenize_mixed_whitespace() {
	token_kind expected[] = { TOK_INT, TOK_MAIN, TOK_LPAR, TOK_VOID, TOK_RPAR };
	size_t n = sizeof(expected) / sizeof(expected[0]);

	char src[] = "int\tmain\n( void )\r\n";
	token_list tl = token_list_create(4);
	assert(tokenize(src, &tl) == 0);
	assert(tl.count == n);
	for (size_t i = 0; i < n; i++) {
		assert(tl.items[i] == expected[i]);
	}
	token_list_destroy(&tl);
	printf("  PASS: tokenize_mixed_whitespace\n");
}

void test_tokenize_single() {
	char src[] = "return";
	token_list tl = token_list_create(4);
	assert(tokenize(src, &tl) == 0);
	assert(tl.count == 1);
	assert(tl.items[0] == TOK_RETURN);
	token_list_destroy(&tl);
	printf("  PASS: tokenize_single\n");
}

void test_tokenize_empty() {
	char src[] = "";
	token_list tl = token_list_create(4);
	assert(tokenize(src, &tl) == 0);
	assert(tl.count == 0);
	token_list_destroy(&tl);
	printf("  PASS: tokenize_empty\n");
}

void test_tokenize_only_whitespace() {
	char src[] = "   \t\n  ";
	token_list tl = token_list_create(4);
	assert(tokenize(src, &tl) == 0);
	assert(tl.count == 0);
	token_list_destroy(&tl);
	printf("  PASS: tokenize_only_whitespace\n");
}

void test_tokenize_triggers_realloc() {
	// capacity 2, program has 10 tokens → forces reallocs
	char src[] = "int main ( void ) { return 2 ; }";
	token_list tl = token_list_create(2);
	assert(tokenize(src, &tl) == 0);
	assert(tl.count == 10);
	assert(tl.capacity >= 10);
	token_list_destroy(&tl);
	printf("  PASS: tokenize_triggers_realloc\n");
}

int main() {
	printf("str_to_token tests:\n");
	test_str_to_token_keyword();
	test_str_to_token_punct();
	test_str_to_token_int_literal();
	test_str_to_token_unknown();

	printf("\ntokenize tests:\n");
	test_tokenize_full_program();
	test_tokenize_mixed_whitespace();
	test_tokenize_single();
	test_tokenize_empty();
	test_tokenize_only_whitespace();
	test_tokenize_triggers_realloc();

	printf("\nall tests passed\n");
	return 0;
}
