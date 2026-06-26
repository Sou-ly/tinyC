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
		case TOK_LPAR:			return "(";
		case TOK_RPAR:      	return ")";
		case TOK_LBRACE:    	return "{";
		case TOK_RBRACE:    	return "}";
		case TOK_COMMA:     	return ",";
		case TOK_SEMICOLON: 	return ";";
		case TOK_COLON:			return ":";
		case TOK_QUESTION_MARK: return "?";
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
		case TOK_DECR:      return "--";
		case TOK_INCR:      return "++";
		case TOK_NOT:       return "~";
		case TOK_AND:       return "&";
		case TOK_OR:        return "|";
		case TOK_XOR:       return "^";
		case TOK_ASSIGN:	return "=";
		case TOK_LSHIFT:    return "<<";
		case TOK_RSHIFT:    return ">>";
		case TOK_LNOT:		return "!";
		case TOK_LAND:		return "&&";
		case TOK_LOR:		return "||";
		case TOK_EQ:        return "==";
		case TOK_NEQ:       return "!=";
		case TOK_LESS:		return "<";
		case TOK_GREATER:	return ">";
		case TOK_LEQ:		return "<=";
		case TOK_GEQ:		return ">=";
		case TOK_PLUS_EQ:	return "+=";
		case TOK_MINUS_EQ:	return "-=";
		case TOK_MUL_EQ:	return "*=";
		case TOK_DIV_EQ:	return "/=";
		case TOK_MOD_EQ:	return "%=";
		case TOK_AND_EQ:	return "&=";
		case TOK_OR_EQ:		return "|=";
		case TOK_XOR_EQ:	return "^=";
		case TOK_RSHIFT_EQ:	return ">>=";
		case TOK_LSHIFT_EQ:	return "<<=";
	}
	return "<unknown>";
}

const char *keyword_name(TokenKeyword k) {
	switch (k) {
		case TOK_IF:     return "if";
		case TOK_ELSE:   return "else";
		case TOK_INT:    return "int";
		case TOK_RETURN: return "return";
		case TOK_VOID:   return "void";
	}
	return "<unknown>";
}

static int keyword_lookup(const char *word, size_t len) {
	static const struct { const char *name; size_t len; TokenKeyword kw; } table[] = {
		{ "if",     2, TOK_IF     },
		{ "else",   4, TOK_ELSE   },
		{ "int",    3, TOK_INT    },
		{ "return", 6, TOK_RETURN },
		{ "void",   4, TOK_VOID   },
	};
	for (size_t i = 0; i < sizeof table / sizeof table[0]; i++) {
		if (table[i].len == len && strncmp(word, table[i].name, len) == 0)
			return table[i].kw;
	}
	return -1;
}

typedef struct {
	const char *text;
	size_t len;
	TokenKind kind;
	int value;
} PunctEntry;

#define OP(s, v)  { s, sizeof(s) - 1, TOK_OPERATOR, v }
#define SEP(s, v) { s, sizeof(s) - 1, TOK_SEPARATOR, v }

// Longer lexemes must precede their prefixes (">>=" before ">>" before ">").
static const PunctEntry punct_table[] = {
	OP(">>=", TOK_RSHIFT_EQ),	OP("<<=", TOK_LSHIFT_EQ),
	OP("++", TOK_INCR),			OP("--", TOK_DECR),
	OP("<<", TOK_LSHIFT),		OP(">>", TOK_RSHIFT),
	OP("==", TOK_EQ),			OP("!=", TOK_NEQ),
	OP("<=", TOK_LEQ),			OP(">=", TOK_GEQ),
	OP("&&", TOK_LAND),			OP("||", TOK_LOR),
	OP("+=", TOK_PLUS_EQ),		OP("-=", TOK_MINUS_EQ),
	OP("*=", TOK_MUL_EQ),		OP("/=", TOK_DIV_EQ),
	OP("%=", TOK_MOD_EQ),		OP("&=", TOK_AND_EQ),
	OP("|=", TOK_OR_EQ),		OP("^=", TOK_XOR_EQ),
	OP("+",  TOK_PLUS),			OP("-",  TOK_MINUS),
	OP("*",  TOK_STAR),			OP("/",  TOK_FSLASH),
	OP("%",  TOK_PERCENT),		OP("~",  TOK_NOT),
	OP("&",  TOK_AND),			OP("|",  TOK_OR),
	OP("^",  TOK_XOR),			OP("!",  TOK_LNOT),
	OP("<",  TOK_LESS),			OP(">",  TOK_GREATER),
	OP("=", TOK_ASSIGN),		SEP("(", TOK_LPAR),
	SEP(")", TOK_RPAR),			SEP("{", TOK_LBRACE), 
	SEP("}", TOK_RBRACE),		SEP(",", TOK_COMMA),
	SEP(";", TOK_SEMICOLON),	SEP(":", TOK_COLON),
	SEP("?", TOK_QUESTION_MARK)
};

#undef OP
#undef SEP

static const PunctEntry *punct_lookup(const char *source) {
	for (size_t i = 0; i < sizeof punct_table / sizeof punct_table[0]; i++) {
		if (strncmp(source, punct_table[i].text, punct_table[i].len) == 0)
			return &punct_table[i];
	}
	return NULL;
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

			int kw = keyword_lookup(source + start, len);
			if (kw >= 0) {
				Token t = { .kind = TOK_KEYWORD, .kw = kw };
				token_list_push(tokens, t);
			} else {
				char *word = malloc(len + 1);
				if (!word) return ERR_NO_MEMORY;
				memcpy(word, source + start, len);
				word[len] = '\0';
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
			const PunctEntry *p = punct_lookup(source + i);
			if (!p) return ERR_UNEXPECTED_CHAR;
			Token t = { .kind = p->kind };
			if (p->kind == TOK_OPERATOR)
				t.op = p->value;
			else
				t.sep = p->value;
			token_list_push(tokens, t);
			i += p->len;
		}
	}
	return ERR_OK;
}
