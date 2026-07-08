#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../src/lexer/token.h"
#include "../src/strlib/str.h"

// Exercises the generic containers in src/common/list.h through two concrete
// element types: Token (owns its identifier spelling) and char* (borrowed).

static Token tok_kw(TokenKeyword kw) {
	return (Token){ .kind = TOK_KEYWORD, .as.keyword = kw };
}
static Token tok_sep(TokenSeparator s) {
	return (Token){ .kind = TOK_SEPARATOR, .as.separator = s };
}
static Token tok_op(TokenOperator o) {
	return (Token){ .kind = TOK_OPERATOR, .as.operator = o };
}
static Token tok_ident(const char *name) {
	return (Token){ .kind = TOK_IDENTIFIER, .as.identifier = strdup(name) };
}
static Token tok_int(int value) {
	return (Token){ .kind = TOK_INT_LITERAL, .as.int_value = value };
}

// ---- Token list (LIST_OF(Token)) ----

void test_empty_is_zeroed() {
	TokenList tl = {0};
	assert(tl.items == NULL);
	assert(tl.count == 0);
	assert(tl.capacity == 0);
	free_tokens(&tl);
	printf("  PASS: empty_is_zeroed\n");
}

void test_push_grows_from_empty() {
	TokenList tl = {0};
	list_push(&tl, tok_kw(TOK_INT));
	assert(tl.count == 1);
	assert(tl.capacity == 4);       // first push allocates the initial block
	assert(tl.items[0].kind == TOK_KEYWORD);
	assert(tl.items[0].as.keyword == TOK_INT);
	free_tokens(&tl);
	printf("  PASS: push_grows_from_empty\n");
}

void test_push_each_kind() {
	TokenList tl = {0};
	list_push(&tl, tok_sep(TOK_LPAR));
	list_push(&tl, tok_op(TOK_EQ));
	list_push(&tl, tok_ident("main"));
	list_push(&tl, tok_int(42));
	assert(tl.count == 4);
	assert(tl.items[0].kind == TOK_SEPARATOR && tl.items[0].as.separator == TOK_LPAR);
	assert(tl.items[1].kind == TOK_OPERATOR  && tl.items[1].as.operator == TOK_EQ);
	assert(tl.items[2].kind == TOK_IDENTIFIER && strcmp(tl.items[2].as.identifier, "main") == 0);
	assert(tl.items[3].kind == TOK_INT_LITERAL && tl.items[3].as.int_value == 42);
	free_tokens(&tl);
	printf("  PASS: push_each_kind\n");
}

void test_push_doubles_capacity() {
	TokenList tl = {0};
	for (int i = 0; i < 5; i++)     // 0 -> 4 on first push, 4 -> 8 on the fifth
		list_push(&tl, tok_sep(TOK_SEMICOLON));
	assert(tl.count == 5);
	assert(tl.capacity == 8);
	free_tokens(&tl);
	printf("  PASS: push_doubles_capacity\n");
}

void test_push_many_reallocs() {
	TokenList tl = {0};
	for (int i = 0; i < 100; i++)
		list_push(&tl, tok_sep(TOK_SEMICOLON));
	assert(tl.count == 100);
	assert(tl.capacity >= 100);
	for (int i = 0; i < 100; i++)
		assert(tl.items[i].kind == TOK_SEPARATOR && tl.items[i].as.separator == TOK_SEMICOLON);
	free_tokens(&tl);
	printf("  PASS: push_many_reallocs\n");
}

void test_free_resets() {
	TokenList tl = {0};
	list_push(&tl, tok_kw(TOK_INT));
	list_push(&tl, tok_kw(TOK_RETURN));
	free_tokens(&tl);
	assert(tl.items == NULL);
	assert(tl.count == 0);
	assert(tl.capacity == 0);
	printf("  PASS: free_resets\n");
}

void test_free_tokens_frees_owned_text() {
	// Identifier tokens own their spelling — free_tokens must free each. Run
	// under valgrind to confirm no leak; here we verify no crash/double-free.
	TokenList tl = {0};
	list_push(&tl, tok_ident("foo"));
	list_push(&tl, tok_int(42));
	list_push(&tl, tok_ident("bar"));
	assert(strcmp(tl.items[0].as.identifier, "foo") == 0);
	assert(tl.items[1].as.int_value == 42);
	assert(strcmp(tl.items[2].as.identifier, "bar") == 0);
	free_tokens(&tl);
	printf("  PASS: free_tokens_frees_owned_text\n");
}

void test_preserves_order() {
	// int main ( void ) { return 2 ; }
	TokenList tl = {0};
	list_push(&tl, tok_kw(TOK_INT));
	list_push(&tl, tok_ident("main"));
	list_push(&tl, tok_sep(TOK_LPAR));
	list_push(&tl, tok_kw(TOK_VOID));
	list_push(&tl, tok_sep(TOK_RPAR));
	list_push(&tl, tok_sep(TOK_LBRACE));
	list_push(&tl, tok_kw(TOK_RETURN));
	list_push(&tl, tok_int(2));
	list_push(&tl, tok_sep(TOK_SEMICOLON));
	list_push(&tl, tok_sep(TOK_RBRACE));
	assert(tl.count == 10);
	assert(tl.items[0].kind == TOK_KEYWORD     && tl.items[0].as.keyword  == TOK_INT);
	assert(tl.items[1].kind == TOK_IDENTIFIER  && strcmp(tl.items[1].as.identifier, "main") == 0);
	assert(tl.items[2].kind == TOK_SEPARATOR   && tl.items[2].as.separator == TOK_LPAR);
	assert(tl.items[3].kind == TOK_KEYWORD     && tl.items[3].as.keyword  == TOK_VOID);
	assert(tl.items[4].kind == TOK_SEPARATOR   && tl.items[4].as.separator == TOK_RPAR);
	assert(tl.items[5].kind == TOK_SEPARATOR   && tl.items[5].as.separator == TOK_LBRACE);
	assert(tl.items[6].kind == TOK_KEYWORD     && tl.items[6].as.keyword  == TOK_RETURN);
	assert(tl.items[7].kind == TOK_INT_LITERAL && tl.items[7].as.int_value == 2);
	assert(tl.items[8].kind == TOK_SEPARATOR   && tl.items[8].as.separator == TOK_SEMICOLON);
	assert(tl.items[9].kind == TOK_SEPARATOR   && tl.items[9].as.separator == TOK_RBRACE);
	free_tokens(&tl);
	printf("  PASS: preserves_order\n");
}

// ---- String list (LIST_OF(char*), borrowed elements) ----

void test_string_list_push_and_order() {
	StringList sl = {0};
	list_push(&sl, "int");
	list_push(&sl, "main");
	list_push(&sl, "return");
	assert(sl.count == 3);
	assert(strcmp(sl.items[0], "int") == 0);
	assert(strcmp(sl.items[1], "main") == 0);
	assert(strcmp(sl.items[2], "return") == 0);
	list_free(&sl);
	assert(sl.items == NULL && sl.count == 0 && sl.capacity == 0);
	printf("  PASS: string_list_push_and_order\n");
}

void test_string_list_stores_pointers_not_copies() {
	char buf[] = "hello";
	StringList sl = {0};
	list_push(&sl, buf);
	buf[0] = 'H';
	assert(strcmp(sl.items[0], "Hello") == 0);
	list_free(&sl);
	printf("  PASS: string_list_stores_pointers_not_copies\n");
}

int main() {
	printf("list.h (Token) tests:\n");
	test_empty_is_zeroed();
	test_push_grows_from_empty();
	test_push_each_kind();
	test_push_doubles_capacity();
	test_push_many_reallocs();
	test_free_resets();
	test_free_tokens_frees_owned_text();
	test_preserves_order();

	printf("\nlist.h (char*) tests:\n");
	test_string_list_push_and_order();
	test_string_list_stores_pointers_not_copies();

	printf("\nall tests passed\n");
	return 0;
}
