#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../src/lexer/token.h"
#include "../src/list.h"

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
	assert(strcmp(token_kind_name((TokenKind)999), "<unknown>") == 0);
	printf("  PASS: token_kind_name_unknown\n");
}

// ---- separator_name ----

void test_separator_name() {
	assert(strcmp(separator_name(TOK_LPAR),      "(") == 0);
	assert(strcmp(separator_name(TOK_RPAR),      ")") == 0);
	assert(strcmp(separator_name(TOK_LBRACE),    "{") == 0);
	assert(strcmp(separator_name(TOK_RBRACE),    "}") == 0);
	assert(strcmp(separator_name(TOK_COMMA),     ",") == 0);
	assert(strcmp(separator_name(TOK_SEMICOLON), ";") == 0);
	printf("  PASS: separator_name\n");
}

void test_separator_name_unknown() {
	assert(strcmp(separator_name((TokenSeparator)999), "<unknown>") == 0);
	printf("  PASS: separator_name_unknown\n");
}

// ---- operator_name ----

void test_operator_name() {
	assert(strcmp(operator_name(TOK_PLUS),       "+")  == 0);
	assert(strcmp(operator_name(TOK_MINUS),      "-")  == 0);
	assert(strcmp(operator_name(TOK_STAR),       "*")  == 0);
	assert(strcmp(operator_name(TOK_PERCENT),    "%")  == 0);
	assert(strcmp(operator_name(TOK_FSLASH),     "/")  == 0);
	assert(strcmp(operator_name(TOK_EQ),         "==") == 0);
	assert(strcmp(operator_name(TOK_NEQ),        "!=") == 0);
	assert(strcmp(operator_name(TOK_AND),        "&") == 0);
	assert(strcmp(operator_name(TOK_OR),         "|") == 0);
	assert(strcmp(operator_name(TOK_XOR),        "^") == 0);
	assert(strcmp(operator_name(TOK_NOT),        "~")  == 0);
	assert(strcmp(operator_name(TOK_RSHIFT),     ">>") == 0);
	assert(strcmp(operator_name(TOK_LSHIFT),     "<<") == 0);
	assert(strcmp(operator_name(TOK_DECR),       "--") == 0);
	assert(strcmp(operator_name(TOK_INCR),       "++") == 0);
	assert(strcmp(operator_name(TOK_ASSIGN),     "=") == 0);
	assert(strcmp(operator_name(TOK_PLUS_EQ),    "+=")  == 0);
	assert(strcmp(operator_name(TOK_MINUS_EQ),   "-=")  == 0);
	assert(strcmp(operator_name(TOK_MUL_EQ),     "*=")  == 0);
	assert(strcmp(operator_name(TOK_DIV_EQ),     "/=")  == 0);
	assert(strcmp(operator_name(TOK_MOD_EQ),     "%=")  == 0);
	assert(strcmp(operator_name(TOK_AND_EQ),     "&=")  == 0);
	assert(strcmp(operator_name(TOK_OR_EQ),      "|=")  == 0);
	assert(strcmp(operator_name(TOK_XOR_EQ),     "^=")  == 0);
	assert(strcmp(operator_name(TOK_RSHIFT_EQ),  ">>=") == 0);
	assert(strcmp(operator_name(TOK_LSHIFT_EQ),  "<<=") == 0);
	printf("  PASS: operator_name\n");
}

void test_operator_name_unknown() {
	assert(strcmp(operator_name((TokenOperator)999), "<unknown>") == 0);
	printf("  PASS: operator_name_unknown\n");
}

// ---- keyword_name ----

void test_keyword_name() {
	assert(strcmp(keyword_name(TOK_IF),     "if")     == 0);
	assert(strcmp(keyword_name(TOK_INT),    "int")    == 0);
	assert(strcmp(keyword_name(TOK_RETURN), "return") == 0);
	assert(strcmp(keyword_name(TOK_VOID),   "void")   == 0);
	printf("  PASS: keyword_name\n");
}

void test_keyword_name_unknown() {
	assert(strcmp(keyword_name((TokenKeyword)999), "<unknown>") == 0);
	printf("  PASS: keyword_name_unknown\n");
}

// ---- tokenize tests ----

void test_tokenize_empty() {
	TokenList tl = token_list_create(4);
	assert(tokenize("", &tl) == ERR_OK);
	assert(tl.count == 0);
	token_list_destroy(&tl);
	printf("  PASS: tokenize_empty\n");
}

void test_tokenize_whitespace_only() {
	TokenList tl = token_list_create(4);
	assert(tokenize("   \n\t\n  ", &tl) == ERR_OK);
	assert(tl.count == 0);
	token_list_destroy(&tl);
	printf("  PASS: tokenize_whitespace_only\n");
}

void test_tokenize_single_keyword() {
	TokenList tl = token_list_create(4);
	assert(tokenize("return", &tl) == ERR_OK);
	assert(tl.count == 1);
	assert(tl.items[0].kind == TOK_KEYWORD);
	assert(tl.items[0].kw == TOK_RETURN);
	token_list_destroy(&tl);
	printf("  PASS: tokenize_single_keyword\n");
}

void test_tokenize_all_keywords() {
	TokenList tl = token_list_create(4);
	assert(tokenize("if int return void", &tl) == ERR_OK);
	assert(tl.count == 4);
	assert(tl.items[0].kw == TOK_IF);
	assert(tl.items[1].kw == TOK_INT);
	assert(tl.items[2].kw == TOK_RETURN);
	assert(tl.items[3].kw == TOK_VOID);
	token_list_destroy(&tl);
	printf("  PASS: tokenize_all_keywords\n");
}

void test_tokenize_identifier() {
	TokenList tl = token_list_create(4);
	assert(tokenize("foo", &tl) == ERR_OK);
	assert(tl.count == 1);
	assert(tl.items[0].kind == TOK_IDENTIFIER);
	assert(strcmp(tl.items[0].ident, "foo") == 0);
	token_list_destroy(&tl);
	printf("  PASS: tokenize_identifier\n");
}

void test_tokenize_identifier_with_underscores() {
	TokenList tl = token_list_create(4);
	assert(tokenize("_foo_bar2", &tl) == ERR_OK);
	assert(tl.count == 1);
	assert(tl.items[0].kind == TOK_IDENTIFIER);
	assert(strcmp(tl.items[0].ident, "_foo_bar2") == 0);
	token_list_destroy(&tl);
	printf("  PASS: tokenize_identifier_with_underscores\n");
}

void test_tokenize_int_literal() {
	TokenList tl = token_list_create(4);
	assert(tokenize("42", &tl) == ERR_OK);
	assert(tl.count == 1);
	assert(tl.items[0].kind == TOK_INT_LITERAL);
	assert(tl.items[0].int_val == 42);
	token_list_destroy(&tl);
	printf("  PASS: tokenize_int_literal\n");
}

void test_tokenize_separators() {
	TokenList tl = token_list_create(8);
	assert(tokenize("(){},;", &tl) == ERR_OK);
	assert(tl.count == 6);
	assert(tl.items[0].sep == TOK_LPAR);
	assert(tl.items[1].sep == TOK_RPAR);
	assert(tl.items[2].sep == TOK_LBRACE);
	assert(tl.items[3].sep == TOK_RBRACE);
	assert(tl.items[4].sep == TOK_COMMA);
	assert(tl.items[5].sep == TOK_SEMICOLON);
	token_list_destroy(&tl);
	printf("  PASS: tokenize_separators\n");
}

void test_tokenize_operators() {
	TokenList tl = token_list_create(8);
	assert(tokenize("+ - == != & |", &tl) == ERR_OK);
	assert(tl.count == 6);
	assert(tl.items[0].op == TOK_PLUS);
	assert(tl.items[1].op == TOK_MINUS);
	assert(tl.items[2].op == TOK_EQ);
	assert(tl.items[3].op == TOK_NEQ);
	assert(tl.items[4].op == TOK_AND);
	assert(tl.items[5].op == TOK_OR);
	token_list_destroy(&tl);
	printf("  PASS: tokenize_operators\n");
}

void test_tokenize_full_function() {
	TokenList tl = token_list_create(4);
	assert(tokenize("int main(void) { return 2; }", &tl) == ERR_OK);
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
	printf("  PASS: tokenize_full_function\n");
}

void test_tokenize_unexpected_char() {
	TokenList tl = token_list_create(4);
	assert(tokenize("int @", &tl) == ERR_UNEXPECTED_CHAR);
	token_list_destroy(&tl);
	printf("  PASS: tokenize_unexpected_char\n");
}

void test_tokenize_two_char_ops_no_spaces() {
	TokenList tl = token_list_create(4);
	assert(tokenize("a==b", &tl) == ERR_OK);
	assert(tl.count == 3);
	assert(tl.items[0].kind == TOK_IDENTIFIER);
	assert(tl.items[1].kind == TOK_OPERATOR && tl.items[1].op == TOK_EQ);
	assert(tl.items[2].kind == TOK_IDENTIFIER);
	token_list_destroy(&tl);
	printf("  PASS: tokenize_two_char_ops_no_spaces\n");
}

void test_tokenize_assign_int_simple() {
	TokenList tl = token_list_create(4);
	assert(tokenize("int a = 42;", &tl) == ERR_OK);
	assert(tl.count == 5);
	assert(tl.items[0].kind == TOK_KEYWORD);
	assert(tl.items[0].kw	== TOK_INT);
	assert(tl.items[1].kind == TOK_IDENTIFIER);
	assert(tl.items[2].kind == TOK_OPERATOR);
	assert(tl.items[2].op	== TOK_ASSIGN);
	assert(tl.items[3].kind == TOK_INT_LITERAL && tl.items[3].int_val == 42);
	assert(tl.items[4].kind == TOK_SEPARATOR && tl.items[4].sep == TOK_SEMICOLON);
	token_list_destroy(&tl);
	printf(" PASS: test_tokenize_assign_int_simple\n");
}

// All ten compound-assignment operators tokenize to their own operator token.
void test_tokenize_compound_assign_ops() {
	TokenList tl = token_list_create(16);
	assert(tokenize("+= -= *= /= %= &= |= ^= >>= <<=", &tl) == ERR_OK);
	assert(tl.count == 10);
	TokenOperator expected[] = {
		TOK_PLUS_EQ, TOK_MINUS_EQ, TOK_MUL_EQ, TOK_DIV_EQ, TOK_MOD_EQ,
		TOK_AND_EQ,  TOK_OR_EQ,    TOK_XOR_EQ, TOK_RSHIFT_EQ, TOK_LSHIFT_EQ,
	};
	for (size_t i = 0; i < sizeof(expected) / sizeof(expected[0]); i++) {
		assert(tl.items[i].kind == TOK_OPERATOR);
		assert(tl.items[i].op == expected[i]);
	}
	token_list_destroy(&tl);
	printf("  PASS: tokenize_compound_assign_ops\n");
}

// Maximal munch: a compound-assign lexeme must win over its shorter prefixes,
// and the lexer must not greedily merge a plain operator with a following '='.
void test_tokenize_compound_assign_maximal_munch() {
	// ">>=" is one token, distinct from ">>" then "=".
	TokenList tl = token_list_create(8);
	assert(tokenize("a>>=b", &tl) == ERR_OK);
	assert(tl.count == 3);
	assert(tl.items[0].kind == TOK_IDENTIFIER);
	assert(tl.items[1].kind == TOK_OPERATOR && tl.items[1].op == TOK_RSHIFT_EQ);
	assert(tl.items[2].kind == TOK_IDENTIFIER);
	token_list_destroy(&tl);

	// "+=" with no surrounding spaces is still a single compound token.
	tl = token_list_create(8);
	assert(tokenize("a+=1", &tl) == ERR_OK);
	assert(tl.count == 3);
	assert(tl.items[1].kind == TOK_OPERATOR && tl.items[1].op == TOK_PLUS_EQ);
	token_list_destroy(&tl);

	// ">> =" with a space stays a shift followed by an assign, not ">>=".
	tl = token_list_create(8);
	assert(tokenize("a >> = b", &tl) == ERR_OK);
	assert(tl.count == 4);
	assert(tl.items[1].kind == TOK_OPERATOR && tl.items[1].op == TOK_RSHIFT);
	assert(tl.items[2].kind == TOK_OPERATOR && tl.items[2].op == TOK_ASSIGN);
	token_list_destroy(&tl);

	printf("  PASS: tokenize_compound_assign_maximal_munch\n");
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
	test_tokenize_assign_int_simple();
	test_tokenize_compound_assign_ops();
	test_tokenize_compound_assign_maximal_munch();

	printf("\nall tests passed\n");
	return 0;
}
