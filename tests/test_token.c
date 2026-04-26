#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../lexer/token.h"
#include "../list.h"

// ---- token_kind_name ----

void test_token_kind_name() {
	assert(strcmp(token_kind_name(TOK_SEPARATOR),   "separator")   == 0);
	assert(strcmp(token_kind_name(TOK_OPERATOR),    "operator")    == 0);
	assert(strcmp(token_kind_name(TOK_KEYWORD),     "keyword")     == 0);
	assert(strcmp(token_kind_name(TOK_IDENTIFIER),  "identifier")  == 0);
	assert(strcmp(token_kind_name(TOK_INT_LITERAL), "int literal") == 0);
	printf("  PASS: token_kind_name\n");
}

void test_token_kind_name_unknown() {
	assert(strcmp(token_kind_name((token_kind)999), "<unknown>") == 0);
	printf("  PASS: token_kind_name_unknown\n");
}

// ---- separator_name ----

void test_separator_name() {
	assert(strcmp(separator_name(SEP_LPAR),      "(") == 0);
	assert(strcmp(separator_name(SEP_RPAR),      ")") == 0);
	assert(strcmp(separator_name(SEP_LBRACE),    "{") == 0);
	assert(strcmp(separator_name(SEP_RBRACE),    "}") == 0);
	assert(strcmp(separator_name(SEP_COMMA),     ",") == 0);
	assert(strcmp(separator_name(SEP_SEMICOLON), ";") == 0);
	printf("  PASS: separator_name\n");
}

void test_separator_name_unknown() {
	assert(strcmp(separator_name((token_separator)999), "<unknown>") == 0);
	printf("  PASS: separator_name_unknown\n");
}

// ---- operator_name ----

void test_operator_name() {
	assert(strcmp(operator_name(OP_PLUS),  "+")  == 0);
	assert(strcmp(operator_name(OP_MINUS), "-")  == 0);
	assert(strcmp(operator_name(OP_EQ),    "==") == 0);
	assert(strcmp(operator_name(OP_NEQ),   "!=") == 0);
	assert(strcmp(operator_name(OP_AND),   "&&") == 0);
	assert(strcmp(operator_name(OP_OR),    "||") == 0);
	printf("  PASS: operator_name\n");
}

void test_operator_name_unknown() {
	assert(strcmp(operator_name((token_operator)999), "<unknown>") == 0);
	printf("  PASS: operator_name_unknown\n");
}

// ---- keyword_name ----

void test_keyword_name() {
	assert(strcmp(keyword_name(KW_IF),     "if")     == 0);
	assert(strcmp(keyword_name(KW_INT),    "int")    == 0);
	assert(strcmp(keyword_name(KW_RETURN), "return") == 0);
	assert(strcmp(keyword_name(KW_VOID),   "void")   == 0);
	printf("  PASS: keyword_name\n");
}

void test_keyword_name_unknown() {
	assert(strcmp(keyword_name((token_keyword)999), "<unknown>") == 0);
	printf("  PASS: keyword_name_unknown\n");
}

// ---- token_free ----

void test_token_free_clears_text() {
	token t = (token){ .kind = TOK_IDENTIFIER, .text = strdup("hello") };
	assert(t.text != NULL);
	assert(strcmp(t.text, "hello") == 0);
	token_free(&t);
	assert(t.text == NULL);
	printf("  PASS: token_free_clears_text\n");
}

void test_token_free_no_text() {
	token t = (token){ .kind = TOK_KEYWORD, .as.kw = KW_INT };
	token_free(&t);  // must not crash on empty/null text
	printf("  PASS: token_free_no_text\n");
}

void test_token_free_null() {
	token_free(NULL);  // must not crash
	printf("  PASS: token_free_null\n");
}

// ---- tokenize stub smoke test ----
// Real tokenize tests are pending the lexer implementation. For now, just
// verify the stub returns ERR_OK and leaves the list untouched.

void test_tokenize_stub_returns_ok() {
	token_list tl = token_list_create(4);
	assert(tokenize("int main(void) { return 2; }", &tl) == ERR_OK);
	assert(tl.count == 0);  // stub does nothing
	token_list_destroy(&tl);
	printf("  PASS: tokenize_stub_returns_ok\n");
}

int main() {
	printf("token_kind_name tests:\n");
	test_token_kind_name();
	test_token_kind_name_unknown();

	printf("\nseparator_name tests:\n");
	test_separator_name();
	test_separator_name_unknown();

	printf("\noperator_name tests:\n");
	test_operator_name();
	test_operator_name_unknown();

	printf("\nkeyword_name tests:\n");
	test_keyword_name();
	test_keyword_name_unknown();

	printf("\ntoken_free tests:\n");
	test_token_free_clears_text();
	test_token_free_no_text();
	test_token_free_null();

	printf("\ntokenize stub:\n");
	test_tokenize_stub_returns_ok();

	printf("\nall tests passed\n");
	return 0;
}
