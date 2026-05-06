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

void test_token_free_ident() {
	token t = (token){ .kind = TOK_IDENTIFIER, .as.ident = strdup("hello") };
	assert(t.as.ident != NULL);
	assert(strcmp(t.as.ident, "hello") == 0);
	token_free(&t);
	printf("  PASS: token_free_ident\n");
}

void test_token_free_no_text() {
	token t = (token){ .kind = TOK_KEYWORD, .as.kw = KW_INT };
	token_free(&t);  // must not crash on keyword token
	printf("  PASS: token_free_no_text\n");
}

void test_token_free_null() {
	token_free(NULL);  // must not crash
	printf("  PASS: token_free_null\n");
}

// ---- tokenize tests ----

void test_tokenize_empty() {
	token_list tl = token_list_create(4);
	assert(tokenize("", &tl) == ERR_OK);
	assert(tl.count == 0);
	token_list_destroy(&tl);
	printf("  PASS: tokenize_empty\n");
}

void test_tokenize_whitespace_only() {
	token_list tl = token_list_create(4);
	assert(tokenize("   \n\t\n  ", &tl) == ERR_OK);
	assert(tl.count == 0);
	token_list_destroy(&tl);
	printf("  PASS: tokenize_whitespace_only\n");
}

void test_tokenize_single_keyword() {
	token_list tl = token_list_create(4);
	assert(tokenize("return", &tl) == ERR_OK);
	assert(tl.count == 1);
	assert(tl.items[0].kind == TOK_KEYWORD);
	assert(tl.items[0].as.kw == KW_RETURN);
	token_list_destroy(&tl);
	printf("  PASS: tokenize_single_keyword\n");
}

void test_tokenize_all_keywords() {
	token_list tl = token_list_create(4);
	assert(tokenize("if int return void", &tl) == ERR_OK);
	assert(tl.count == 4);
	assert(tl.items[0].as.kw == KW_IF);
	assert(tl.items[1].as.kw == KW_INT);
	assert(tl.items[2].as.kw == KW_RETURN);
	assert(tl.items[3].as.kw == KW_VOID);
	token_list_destroy(&tl);
	printf("  PASS: tokenize_all_keywords\n");
}

void test_tokenize_identifier() {
	token_list tl = token_list_create(4);
	assert(tokenize("foo", &tl) == ERR_OK);
	assert(tl.count == 1);
	assert(tl.items[0].kind == TOK_IDENTIFIER);
	assert(strcmp(tl.items[0].as.ident, "foo") == 0);
	token_list_destroy(&tl);
	printf("  PASS: tokenize_identifier\n");
}

void test_tokenize_identifier_with_underscores() {
	token_list tl = token_list_create(4);
	assert(tokenize("_foo_bar2", &tl) == ERR_OK);
	assert(tl.count == 1);
	assert(tl.items[0].kind == TOK_IDENTIFIER);
	assert(strcmp(tl.items[0].as.ident, "_foo_bar2") == 0);
	token_list_destroy(&tl);
	printf("  PASS: tokenize_identifier_with_underscores\n");
}

void test_tokenize_int_literal() {
	token_list tl = token_list_create(4);
	assert(tokenize("42", &tl) == ERR_OK);
	assert(tl.count == 1);
	assert(tl.items[0].kind == TOK_INT_LITERAL);
	assert(strcmp(tl.items[0].as.literal, "42") == 0);
	token_list_destroy(&tl);
	printf("  PASS: tokenize_int_literal\n");
}

void test_tokenize_separators() {
	token_list tl = token_list_create(8);
	assert(tokenize("(){},;", &tl) == ERR_OK);
	assert(tl.count == 6);
	assert(tl.items[0].as.sep == SEP_LPAR);
	assert(tl.items[1].as.sep == SEP_RPAR);
	assert(tl.items[2].as.sep == SEP_LBRACE);
	assert(tl.items[3].as.sep == SEP_RBRACE);
	assert(tl.items[4].as.sep == SEP_COMMA);
	assert(tl.items[5].as.sep == SEP_SEMICOLON);
	token_list_destroy(&tl);
	printf("  PASS: tokenize_separators\n");
}

void test_tokenize_operators() {
	token_list tl = token_list_create(8);
	assert(tokenize("+ - == != && ||", &tl) == ERR_OK);
	assert(tl.count == 6);
	assert(tl.items[0].as.op == OP_PLUS);
	assert(tl.items[1].as.op == OP_MINUS);
	assert(tl.items[2].as.op == OP_EQ);
	assert(tl.items[3].as.op == OP_NEQ);
	assert(tl.items[4].as.op == OP_AND);
	assert(tl.items[5].as.op == OP_OR);
	token_list_destroy(&tl);
	printf("  PASS: tokenize_operators\n");
}

void test_tokenize_full_function() {
	token_list tl = token_list_create(4);
	assert(tokenize("int main(void) { return 2; }", &tl) == ERR_OK);
	assert(tl.count == 10);
	assert(tl.items[0].kind == TOK_KEYWORD     && tl.items[0].as.kw  == KW_INT);
	assert(tl.items[1].kind == TOK_IDENTIFIER  && strcmp(tl.items[1].as.ident, "main") == 0);
	assert(tl.items[2].kind == TOK_SEPARATOR   && tl.items[2].as.sep == SEP_LPAR);
	assert(tl.items[3].kind == TOK_KEYWORD     && tl.items[3].as.kw  == KW_VOID);
	assert(tl.items[4].kind == TOK_SEPARATOR   && tl.items[4].as.sep == SEP_RPAR);
	assert(tl.items[5].kind == TOK_SEPARATOR   && tl.items[5].as.sep == SEP_LBRACE);
	assert(tl.items[6].kind == TOK_KEYWORD     && tl.items[6].as.kw  == KW_RETURN);
	assert(tl.items[7].kind == TOK_INT_LITERAL && strcmp(tl.items[7].as.literal, "2") == 0);
	assert(tl.items[8].kind == TOK_SEPARATOR   && tl.items[8].as.sep == SEP_SEMICOLON);
	assert(tl.items[9].kind == TOK_SEPARATOR   && tl.items[9].as.sep == SEP_RBRACE);
	token_list_destroy(&tl);
	printf("  PASS: tokenize_full_function\n");
}

void test_tokenize_unexpected_char() {
	token_list tl = token_list_create(4);
	assert(tokenize("int @", &tl) == ERR_UNEXPECTED_CHAR);
	token_list_destroy(&tl);
	printf("  PASS: tokenize_unexpected_char\n");
}

void test_tokenize_two_char_ops_no_spaces() {
	token_list tl = token_list_create(4);
	assert(tokenize("a==b", &tl) == ERR_OK);
	assert(tl.count == 3);
	assert(tl.items[0].kind == TOK_IDENTIFIER);
	assert(tl.items[1].kind == TOK_OPERATOR && tl.items[1].as.op == OP_EQ);
	assert(tl.items[2].kind == TOK_IDENTIFIER);
	token_list_destroy(&tl);
	printf("  PASS: tokenize_two_char_ops_no_spaces\n");
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
	test_token_free_ident();
	test_token_free_no_text();
	test_token_free_null();

	printf("\ntokenize tests:\n");
	test_tokenize_empty();
	test_tokenize_whitespace_only();
	test_tokenize_single_keyword();
	test_tokenize_all_keywords();
	test_tokenize_identifier();
	test_tokenize_identifier_with_underscores();
	test_tokenize_int_literal();
	test_tokenize_separators();
	test_tokenize_operators();
	test_tokenize_full_function();
	test_tokenize_unexpected_char();
	test_tokenize_two_char_ops_no_spaces();

	printf("\nall tests passed\n");
	return 0;
}
