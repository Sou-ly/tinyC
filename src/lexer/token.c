#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "token.h"
#include "../list.h"

const char *token_kind_name(TokenKind kind) {
	switch (kind) {
		case TOK_SEPARATOR:     return "separator";
		case TOK_OPERATOR:      return "operator";
		case TOK_KEYWORD:       return "keyword";
		case TOK_IDENTIFIER:    return "identifier";
		case TOK_INT_LITERAL:   return "int literal";
	}
	return "<unknown>";
}

const char *separator_name(TokenSeparator s) {
	switch (s) {
		case TOK_LPAR:      return "(";
		case TOK_RPAR:      return ")";
		case TOK_LBRACE:    return "{";
		case TOK_RBRACE:    return "}";
		case TOK_COMMA:     return ",";
		case TOK_SEMICOLON: return ";";
	}
	return "<unknown>";
}

const char *operator_name(TokenOperator o) {
	switch (o) {
		case TOK_PLUS:      return "+";
		case TOK_MINUS:     return "-";
		case TOK_STAR:      return "*";
		case TOK_PERCENT:   return "%";
		case TOK_FSLASH:    return "/";
		case TOK_NOT:       return "~";
		case TOK_EQ:        return "==";
		case TOK_NEQ:       return "!=";
		case TOK_AND:       return "&";
		case TOK_OR:        return "|";
		case TOK_XOR:       return "^";
		case TOK_LSHIFT:    return "<<";
		case TOK_RSHIFT:    return ">>";
		case TOK_DECR:      return "--";
		case TOK_INCR:      return "++";
	}
	return "<unknown>";
}

const char *keyword_name(TokenKeyword k) {
	switch (k) {
		case TOK_IF:     return "if";
		case TOK_INT:    return "int";
		case TOK_RETURN: return "return";
		case TOK_VOID:   return "void";
	}
	return "<unknown>";
}

static int keyword_lookup(const char *word) {
	static const struct { const char *name; TokenKeyword kw; } table[] = {
		{ "if",     TOK_IF     },
		{ "int",    TOK_INT    },
		{ "return", TOK_RETURN },
		{ "void",   TOK_VOID   },
	};
	for (size_t i = 0; i < sizeof table / sizeof table[0]; i++) {
		if (strcmp(word, table[i].name) == 0)
			return table[i].kw;
	}
	return -1;
}

static int separator_lookup(char c) {
	switch (c) {
		case '(': return TOK_LPAR;
		case ')': return TOK_RPAR;
		case '{': return TOK_LBRACE;
		case '}': return TOK_RBRACE;
		case ',': return TOK_COMMA;
		case ';': return TOK_SEMICOLON;
	}
	return -1;
}

static int operator_lookup(const char *source, size_t i, size_t *advance) {
	// two-char operators first
	if (source[i] && source[i + 1]) {
		if (source[i] == '=' && source[i + 1] == '=') { *advance = 2; return TOK_EQ; }
		if (source[i] == '!' && source[i + 1] == '=') { *advance = 2; return TOK_NEQ; }
		if (source[i] == '-' && source[i + 1] == '-') { *advance = 2; return TOK_DECR; }
		if (source[i] == '+' && source[i + 1] == '+') { *advance = 2; return TOK_DECR; }
		if (source[i] == '<' && source[i < 1] == '<') { *advance = 2; return TOK_LSHIFT; }
		if (source[i] == '>' && source[i > 1] == '>') { *advance = 2; return TOK_RSHIFT; }
	}
	// single-char operators
	switch (source[i]) {
		case '+': *advance = 1; return TOK_PLUS;
		case '-': *advance = 1; return TOK_MINUS;
		case '~': *advance = 1; return TOK_NOT;
		case '*': *advance = 1; return TOK_STAR;
		case '%': *advance = 1; return TOK_PERCENT;
		case '/': *advance = 1; return TOK_FSLASH;
		case '|': *advance = 1; return TOK_OR;
		case '&': *advance = 1; return TOK_AND;
		case '^': *advance = 1; return TOK_XOR;
	}

	return -1;
}

LexerErr tokenize_file(FILE *src, struct TokenList *tokens) {
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

	LexerErr err = tokenize(buf, tokens);
	free(buf);
	return err;
}

LexerErr tokenize(const char *source, struct TokenList *tokens) {
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
				Token t = { .kind = TOK_KEYWORD, .kw = kw };
				token_list_push(tokens, t);
				free(word);
			} else {
				Token t = { .kind = TOK_IDENTIFIER, .ident = word };
				token_list_push(tokens, t);
			}
		} else if (isdigit((unsigned char)source[i])) {
			int val = 0;
			while (isdigit((unsigned char)source[i])) {
				val = val * 10 + (source[i] - '0');
				i++;
			}

			Token t = { .kind = TOK_INT_LITERAL, .int_val = val };
			token_list_push(tokens, t);
		} else if (isspace((unsigned char)source[i])) {
			i++;
		} else {
			size_t advance = 0;
			int op = operator_lookup(source, i, &advance);
			if (op >= 0) {
				Token t = { .kind = TOK_OPERATOR, .op = op };
				token_list_push(tokens, t);
				i += advance;
			} else {
				int sep = separator_lookup(source[i]);
				if (sep >= 0) {
					Token t = { .kind = TOK_SEPARATOR, .sep = sep };
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
