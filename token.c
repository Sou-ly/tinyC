#include "token.h"

const char *token_names[] = {
	[TOK_INT]         = "int",
	[TOK_MAIN]        = "main",
	[TOK_LPAR]        = "(",
	[TOK_VOID]        = "void",
	[TOK_RPAR]        = ")",
	[TOK_LBRACE]      = "{",
	[TOK_RETURN]      = "return",
	[TOK_INT_LITERAL] = "<int_literal>",
	[TOK_SEMICOLON]   = ";",
	[TOK_RBRACE]      = "}",
};
