#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "token.h"
#include "../list.h"

void token_free(token *t) {
	if (!t) return;
	if (t->kind == TOK_IDENTIFIER)
		free(t->as.ident);
	else if (t->kind == TOK_INT_LITERAL)
		free(t->as.literal);
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

static int keyword_lookup(const char *word) {
	static const struct { const char *name; token_keyword kw; } table[] = {
		{ "if",     KW_IF     },
		{ "int",    KW_INT    },
		{ "return", KW_RETURN },
		{ "void",   KW_VOID   },
	};
	for (size_t i = 0; i < sizeof table / sizeof table[0]; i++) {
		if (strcmp(word, table[i].name) == 0)
			return table[i].kw;
	}
	return -1;
}

static int separator_lookup(char c) {
	switch (c) {
		case '(': return SEP_LPAR;
		case ')': return SEP_RPAR;
		case '{': return SEP_LBRACE;
		case '}': return SEP_RBRACE;
		case ',': return SEP_COMMA;
		case ';': return SEP_SEMICOLON;
	}
	return -1;
}

static int operator_lookup(const char *source, size_t i, size_t *advance) {
	// two-char operators first
	if (source[i] && source[i + 1]) {
		if (source[i] == '=' && source[i + 1] == '=') { *advance = 2; return OP_EQ; }
		if (source[i] == '!' && source[i + 1] == '=') { *advance = 2; return OP_NEQ; }
		if (source[i] == '&' && source[i + 1] == '&') { *advance = 2; return OP_AND; }
		if (source[i] == '|' && source[i + 1] == '|') { *advance = 2; return OP_OR; }
	}
	// single-char operators
	if (source[i] == '+') { *advance = 1; return OP_PLUS; }
	if (source[i] == '-') { *advance = 1; return OP_MINUS; }
	return -1;
}

lexer_err tokenize_file(FILE *src, struct token_list *tokens) {
	fseek(src, 0, SEEK_END);
	long len = ftell(src);
	if (len < 0) return ERR_FILE_READ;
	rewind(src);

	char *buf = malloc(len + 1);
	if (!buf) return ERR_NO_MEMORY;

	size_t nread = fread(buf, 1, len, src);
	if (nread == 0 && len > 0) {
		free(buf);
		return ERR_FILE_READ;
	}
	buf[nread] = '\0';

	lexer_err err = tokenize(buf, tokens);
	free(buf);
	return err;
}

lexer_err tokenize(const char *source, struct token_list *tokens) {
	size_t i = 0;
	while (source[i] != '\0') {
		if (isalpha((unsigned char)source[i]) || source[i] == '_') {
			size_t start = i;
			while (isalnum((unsigned char)source[i]) || source[i] == '_') i++;
			size_t len = i - start;
			char *word = malloc(len + 1);
			if (!word) return ERR_NO_MEMORY;
			memcpy(word, source + start, len);
			word[len] = '\0';

			int kw = keyword_lookup(word);
			if (kw >= 0) {
				token t = { .kind = TOK_KEYWORD, .as.kw = kw };
				token_list_push(tokens, t);
				free(word);
			} else {
				token t = { .kind = TOK_IDENTIFIER, .as.ident = word };
				token_list_push(tokens, t);
			}
		} else if (isdigit((unsigned char)source[i])) {
			size_t start = i;
			while (isdigit((unsigned char)source[i])) i++;
			size_t len = i - start;
			char *lit = malloc(len + 1);
			if (!lit) return ERR_NO_MEMORY;
			memcpy(lit, source + start, len);
			lit[len] = '\0';

			token t = { .kind = TOK_INT_LITERAL, .as.literal = lit };
			token_list_push(tokens, t);
		} else if (isspace((unsigned char)source[i])) {
			i++;
		} else {
			size_t advance = 0;
			int op = operator_lookup(source, i, &advance);
			if (op >= 0) {
				token t = { .kind = TOK_OPERATOR, .as.op = op };
				token_list_push(tokens, t);
				i += advance;
			} else {
				int sep = separator_lookup(source[i]);
				if (sep >= 0) {
					token t = { .kind = TOK_SEPARATOR, .as.sep = sep };
					token_list_push(tokens, t);
					i++;
				} else {
					return ERR_UNEXPECTED_CHAR;
				}
			}
		}
	}
	return ERR_OK;
}
