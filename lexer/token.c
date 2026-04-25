#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "token.h"
#include "../list.h"


lexer_err str_to_token(const char *str, token_kind *token) {
	for (token_kind kind = TOK_INT; kind <= TOK_RBRACE; kind++) {
		if (!strcmp(token_names[kind], str)) {
			*token = (token_kind)kind;
			return ERR_OK;
		}
	}
	return -1;
}

lexer_err tokenize(const char *str, token_list * tokens) {
    if (str == NULL) {
        fprintf(stderr, "str is NULL");
        return -1;
    }

    if (str == NULL) {
        fprintf(stderr, "tokens is NULL");
        return -1;
    }

	char *copy = strdup(str);
	if G(!copy) return -1;

	char *word = strtok(copy, " \n\t\r");
	while (word != NULL) {
		token_kind token;
		if ((lexer_err err_code = str_to_token(word, &token)) != 0) {
			free(copy);
			return err_code;
		}
		token_list_push(tokens, token);
		word = strtok(NULL, " \n\t\r");
	}

	free(copy);
	return ERR_OK;
}

lexer_err tokenize(const FILE* src, token_list * tokens) {
    if (src == NULL) {
        fprintf(stderr, "src is NULL");
        return -1;
    } 

    if (token_list == NULL) {
        fprintf(stderr, "tokens is NULL");
        return -1;
    }
   
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char * buf = malloc(size + 1);
    fread(buf, 1, size, f);
    buf[size] = '\0';

    int line = 1;
    int col = 1;
    int word_start = 0;
    for (int word_end = 0; word_end < size; word_end++) {
        switch(buf[word_end]) {
            // literal separator
            case '\n': 
                line++;
                break;
            case ';' : 
                token_list_push(tokens, TOK_SEMICOLON);  
                break;
            case '(' : 
                token_list_push(tokens, TOK_LPAR);       
                break;
            case ')' : 
                token_list_push(tokens, TOK_RPAR);       
                break;
            case '{' : 
                token_list_push(tokens, TOK_LBRACE);     
                break;
            case '}' : 
                token_list_push(tokens, TOK_RBRACE);
                break;
            case ' ' : 
            case '\t': 
                // do nothing
                break;
            default:
                // not a separator == literal
                if (isalnum(buf[word_end]) != 0) {
                    fprintf(stderr, "Unrecognized character at line %d: %c", line, buf[word_end]);
                    free(buf);
                    return ERR_UNRECOGNIZED_CHARACTER;
                }        
        }
        if (isalnum(buf[word_end]) != 0) {
            size_t word_len = word_end - word_start;
            if (word_len > 0) {
                char * word = malloc(word_len + 1);
                word[word_len] = '\0';
                strncpy(word, &buf[word_start], word_len);
                if (strcmp(word, "void") == 0) {
                    token_list_push(tokens, TOK_VOID);
                } else if (strcmp(word, "main") == 0) {
                    token_list_push(tokens, TOK_MAIN);
                } else if (strcmp(word, "int") == 0) {
                    token_list_push(
                } else if (strcmp(word, "return") == 0) {
                    
                } else if (isdigit(word)) {
                } 
            }
        } 
        col++;
    }

    free(buf);
    return ERR_OK;
}

