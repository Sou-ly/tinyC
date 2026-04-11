#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

#define C_FILE_EXTENSION ".c"

void display_help() {
	// TODO
	return;
}

size_t trim_leading_whitespace(char ** s, char ** tokens) {
	while (isspace((unsigned char)**s)) {
		(*s)++;
	}
	return 0;
}

size_t tokenize(FILE* source, char** tokens) {
	char *line = NULL;
	size_t len = 0;
	ssize_t nread;
	while ((nread = getline(&line, &len, source)) != -1) {
		trim_leading_whitespace(&line, tokens);
	}
	free(line);
	return 0;
}

int main(int argc, char * argv[]) {
	bool lex = false;
	bool parse = false;
	bool codegen = false;
	bool help = false;
	char* source_path = NULL;
	FILE* src = NULL;
	size_t srclen = 0; 

	// parse arguments
	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--lex") == 0 && !lex) {
			lex = true;
		} else if (strcmp(argv[i], "--parse") == 0 && !parse) {
			parse = true;
		} else if (strcmp(argv[i], "--codegen") == 0 && !codegen) {
			codegen = true;
		} else if (strcmp(argv[i], "--help") == 0 && !help) {
			display_help();
		} else if (source_path == NULL) {
			srclen = strlen(argv[i]);
			if (srclen > 2 && strncmp(&argv[i][srclen - 2], C_FILE_EXTENSION, 2) == 0) {
				source_path = argv[i];
			}
		} else {
			fprintf(stderr, "Unrecognized argument: %s.\n", argv[i]);
			display_help();
			return 2;
		}
	}

	if (source_path == NULL) {
		fprintf(stderr, "Input file is missing.");
		display_help();
		return 2;
	}

	src = fopen(source_path, "r");

	if (src == NULL) {
		fprintf(stderr, "Failed to open %s.\n", source_path);
		return 2;
	}

	fclose(src);
	
	// generate the executable
	
	char * executable_path = malloc(srclen - 1);
	memcpy(executable_path, source_path, srclen - 2);
	executable_path[srclen - 2] = '\0';

	FILE* exec_path = fopen(executable_path, "w");

	if (exec_path == NULL) {
		fprintf(stderr, "Failed to open %s.\n", executable_path);
		return 2;
	}

	fclose(exec_path);

	return 0;
}
