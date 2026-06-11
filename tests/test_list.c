#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../src/list.h"

// Identifiers strdup their spelling — pushing transfers ownership to the
// list, which frees it on destroy.

static Token tok_kw(TokenKeyword kw) {
	return (Token){ .kind = TOK_KEYWORD, .kw = kw };
}
static Token tok_sep(TokenSeparator s) {
	return (Token){ .kind = TOK_SEPARATOR, .sep = s };
}
static Token tok_op(TokenOperator o) {
	return (Token){ .kind = TOK_OPERATOR, .op = o };
}
static Token tok_ident(const char *name) {
	return (Token){ .kind = TOK_IDENTIFIER, .ident = strdup(name) };
}
static Token tok_int(int value) {
	return (Token){ .kind = TOK_INT_LITERAL, .int_val = value };
}

// ---- TokenList tests ----

void test_token_list_create() {
	TokenList tl = token_list_create(4);
	assert(tl.items != NULL);
	assert(tl.count == 0);
	assert(tl.capacity == 4);
	token_list_destroy(&tl);
	printf("  PASS: token_list_create\n");
}

void test_token_list_push_keyword() {
	TokenList tl = token_list_create(4);
	token_list_push(&tl, tok_kw(TOK_INT));
	assert(tl.count == 1);
	assert(tl.items[0].kind == TOK_KEYWORD);
	assert(tl.items[0].kw == TOK_INT);
	token_list_destroy(&tl);
	printf("  PASS: token_list_push_keyword\n");
}

void test_token_list_push_separator() {
	TokenList tl = token_list_create(4);
	token_list_push(&tl, tok_sep(TOK_LPAR));
	assert(tl.count == 1);
	assert(tl.items[0].kind == TOK_SEPARATOR);
	assert(tl.items[0].sep == TOK_LPAR);
	token_list_destroy(&tl);
	printf("  PASS: token_list_push_separator\n");
}

void test_token_list_push_operator() {
	TokenList tl = token_list_create(4);
	token_list_push(&tl, tok_op(TOK_EQ));
	assert(tl.count == 1);
	assert(tl.items[0].kind == TOK_OPERATOR);
	assert(tl.items[0].op == TOK_EQ);
	token_list_destroy(&tl);
	printf("  PASS: token_list_push_operator\n");
}

void test_token_list_push_identifier() {
	TokenList tl = token_list_create(4);
	token_list_push(&tl, tok_ident("main"));
	assert(tl.count == 1);
	assert(tl.items[0].kind == TOK_IDENTIFIER);
	assert(strcmp(tl.items[0].ident, "main") == 0);
	token_list_destroy(&tl);
	printf("  PASS: token_list_push_identifier\n");
}

void test_token_list_push_int_literal() {
	TokenList tl = token_list_create(4);
	token_list_push(&tl, tok_int(42));
	assert(tl.count == 1);
	assert(tl.items[0].kind == TOK_INT_LITERAL);
	assert(tl.items[0].int_val == 42);
	token_list_destroy(&tl);
	printf("  PASS: token_list_push_int_literal\n");
}

void test_token_list_push_mixed() {
	TokenList tl = token_list_create(4);
	token_list_push(&tl, tok_kw(TOK_INT));
	token_list_push(&tl, tok_ident("main"));
	token_list_push(&tl, tok_sep(TOK_LPAR));
	assert(tl.count == 3);
	assert(tl.items[0].kind == TOK_KEYWORD && tl.items[0].kw == TOK_INT);
	assert(tl.items[1].kind == TOK_IDENTIFIER && strcmp(tl.items[1].ident, "main") == 0);
	assert(tl.items[2].kind == TOK_SEPARATOR && tl.items[2].sep == TOK_LPAR);
	token_list_destroy(&tl);
	printf("  PASS: token_list_push_mixed\n");
}

void test_token_list_push_triggers_realloc() {
	TokenList tl = token_list_create(2);
	token_list_push(&tl, tok_kw(TOK_INT));
	token_list_push(&tl, tok_kw(TOK_RETURN));
	// triggers realloc
	token_list_push(&tl, tok_sep(TOK_LPAR));
	assert(tl.count == 3);
	assert(tl.capacity == 4);
	assert(tl.items[0].kw == TOK_INT);
	assert(tl.items[1].kw == TOK_RETURN);
	assert(tl.items[2].sep == TOK_LPAR);
	token_list_destroy(&tl);
	printf("  PASS: token_list_push_triggers_realloc\n");
}

void test_token_list_push_many_reallocs() {
	TokenList tl = token_list_create(1);
	for (int i = 0; i < 100; i++) {
		token_list_push(&tl, tok_sep(TOK_SEMICOLON));
	}
	assert(tl.count == 100);
	assert(tl.capacity >= 100);
	for (int i = 0; i < 100; i++) {
		assert(tl.items[i].kind == TOK_SEPARATOR);
		assert(tl.items[i].sep == TOK_SEMICOLON);
	}
	token_list_destroy(&tl);
	printf("  PASS: token_list_push_many_reallocs\n");
}

void test_token_list_destroy() {
	TokenList tl = token_list_create(4);
	token_list_push(&tl, tok_kw(TOK_INT));
	token_list_push(&tl, tok_kw(TOK_RETURN));
	token_list_destroy(&tl);
	assert(tl.items == NULL);
	assert(tl.count == 0);
	assert(tl.capacity == 0);
	printf("  PASS: token_list_destroy\n");
}

void test_token_list_destroy_frees_owned_text() {
	// Identifier tokens own their spelling — destroy must free each. Run under
	// valgrind to confirm no leak; here we mostly verify no crash/double-free.
	TokenList tl = token_list_create(4);
	token_list_push(&tl, tok_ident("foo"));
	token_list_push(&tl, tok_int(42));
	token_list_push(&tl, tok_ident("bar"));
	assert(strcmp(tl.items[0].ident, "foo") == 0);
	assert(tl.items[1].int_val == 42);
	assert(strcmp(tl.items[2].ident, "bar") == 0);
	token_list_destroy(&tl);
	printf("  PASS: token_list_destroy_frees_owned_text\n");
}

void test_token_list_preserves_order() {
	// int main ( void ) { return 2 ; }
	TokenList tl = token_list_create(2);
	token_list_push(&tl, tok_kw(TOK_INT));
	token_list_push(&tl, tok_ident("main"));
	token_list_push(&tl, tok_sep(TOK_LPAR));
	token_list_push(&tl, tok_kw(TOK_VOID));
	token_list_push(&tl, tok_sep(TOK_RPAR));
	token_list_push(&tl, tok_sep(TOK_LBRACE));
	token_list_push(&tl, tok_kw(TOK_RETURN));
	token_list_push(&tl, tok_int(2));
	token_list_push(&tl, tok_sep(TOK_SEMICOLON));
	token_list_push(&tl, tok_sep(TOK_RBRACE));
	assert(tl.count == 10);
	assert(tl.items[0].kind == TOK_KEYWORD     && tl.items[0].kw  == TOK_INT);
	assert(tl.items[1].kind == TOK_IDENTIFIER  && strcmp(tl.items[1].ident, "main") == 0);
	assert(tl.items[2].kind == TOK_SEPARATOR   && tl.items[2].sep == TOK_LPAR);
	assert(tl.items[3].kind == TOK_KEYWORD     && tl.items[3].kw  == TOK_VOID);
	assert(tl.items[4].kind == TOK_SEPARATOR   && tl.items[4].sep == TOK_RPAR);
	assert(tl.items[5].kind == TOK_SEPARATOR   && tl.items[5].sep == TOK_LBRACE);
	assert(tl.items[6].kind == TOK_KEYWORD     && tl.items[6].kw  == TOK_RETURN);
	assert(tl.items[7].kind == TOK_INT_LITERAL && tl.items[7].int_val == 2);
	assert(tl.items[8].kind == TOK_SEPARATOR   && tl.items[8].sep == TOK_SEMICOLON);
	assert(tl.items[9].kind == TOK_SEPARATOR   && tl.items[9].sep == TOK_RBRACE);
	token_list_destroy(&tl);
	printf("  PASS: token_list_preserves_order\n");
}

// ---- StringList tests ----

void test_string_list_create() {
	StringList sl = string_list_create(4);
	assert(sl.items != NULL);
	assert(sl.count == 0);
	assert(sl.capacity == 4);
	string_list_destroy(&sl);
	printf("  PASS: string_list_create\n");
}

void test_string_list_push_single() {
	StringList sl = string_list_create(4);
	string_list_push(&sl, "hello");
	assert(sl.count == 1);
	assert(strcmp(sl.items[0], "hello") == 0);
	string_list_destroy(&sl);
	printf("  PASS: string_list_push_single\n");
}

void test_string_list_push_multiple() {
	StringList sl = string_list_create(4);
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
	StringList sl = string_list_create(2);
	string_list_push(&sl, "a");
	string_list_push(&sl, "b");
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
	StringList sl = string_list_create(1);
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
	StringList sl = string_list_create(4);
	string_list_push(&sl, "x");
	string_list_push(&sl, "y");
	string_list_destroy(&sl);
	assert(sl.items == NULL);
	assert(sl.count == 0);
	assert(sl.capacity == 0);
	printf("  PASS: string_list_destroy\n");
}

void test_string_list_stores_pointers_not_copies() {
	char buf[] = "hello";
	StringList sl = string_list_create(4);
	string_list_push(&sl, buf);
	buf[0] = 'H';
	assert(strcmp(sl.items[0], "Hello") == 0);
	string_list_destroy(&sl);
	printf("  PASS: string_list_stores_pointers_not_copies\n");
}

int main() {
	printf("TokenList tests:\n");
	test_token_list_create();
	test_token_list_push_keyword();
	test_token_list_push_separator();
	test_token_list_push_operator();
	test_token_list_push_identifier();
	test_token_list_push_int_literal();
	test_token_list_push_mixed();
	test_token_list_push_triggers_realloc();
	test_token_list_push_many_reallocs();
	test_token_list_destroy();
	test_token_list_destroy_frees_owned_text();
	test_token_list_preserves_order();

	printf("\nstring_list tests:\n");
	test_string_list_create();
	test_string_list_push_single();
	test_string_list_push_multiple();
	test_string_list_push_triggers_realloc();
	test_string_list_push_many_reallocs();
	test_string_list_destroy();
	test_string_list_stores_pointers_not_copies();

	printf("\nall tests passed\n");
	return 0;
}
