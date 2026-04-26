#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "token.h"
#include "../list.h"

void token_free(token *t) {
	if (!t) return;
	free(t->text);
	t->text = NULL;
}

const char *token_kind_name(token_kind kind) {
	switch (kind) {
		case TOK_SEPARATOR:   return "separator";
		case TOK_OPERATOR:    return "operator";
		case TOK_KEYWORD:     return "keyword";
		case TOK_IDENTIFIER:  return "identifier";
		case TOK_INT_LITERAL: return "int literal";
	}
	return "<unknown>";
}

const char *separator_name(token_separator s) {
	switch (s) {
		case SEP_LPAR:      return "(";
		case SEP_RPAR:      return ")";
		case SEP_LBRACE:    return "{";
		case SEP_RBRACE:    return "}";
		case SEP_COMMA:     return ",";
		case SEP_SEMICOLON: return ";";
	}
	return "<unknown>";
}

const char *operator_name(token_operator o) {
	switch (o) {
		case OP_PLUS:  return "+";
		case OP_MINUS: return "-";
		case OP_EQ:    return "==";
		case OP_NEQ:   return "!=";
		case OP_AND:   return "&&";
		case OP_OR:    return "||";
	}
	return "<unknown>";
}

const char *keyword_name(token_keyword k) {
	switch (k) {
		case KW_IF:     return "if";
		case KW_INT:    return "int";
		case KW_RETURN: return "return";
		case KW_VOID:   return "void";
	}
	return "<unknown>";
}

// TODO: real lexer — recognize keywords, identifiers, integer literals,
// separators, operators; track line/col; populate token.text for ident/literal.
lexer_err tokenize(const char *source, struct token_list *tokens) {
	size_t i = 0;
	while (source[i] != '\0') {
		if (isalpha((unsigned char)source[i]) || source[i] == '_') {
		    // identifier OR keyword: read [a-zA-Z0-9_]+
		    size_t start = i;
		    while (isalnum((unsigned char)source[i]) || source[i] == '_') i++;
		    // slice source[start..i] is the lexeme
		    // then: lookup in keyword table → if hit, it's a keyword;
		    //       otherwise it's an identifier
		} else if (isdigit((unsigned char)source[i])) {
		    // integer literal: read [0-9]+
		    size_t start = i;
		    while (isdigit((unsigned char)source[i])) i++;
		    // slice is the literal's spelling
		} else if (isspace((unsigned char)source[i])) {
		    if (source[i] == '\n') line++;
		    i++;
		} else {
		    // separator / operator / unknown
		    i++;
		}
	}
	return ERR_OK;
}

// TODO: slurp the file into a buffer, then call tokenize.
lexer_err tokenize_file(FILE *src, struct token_list *tokens) {
	(void)src; (void)tokens;
	return ERR_OK;
}
