#pragma once

#include <stdio.h>
#include <stdlib.h>

// Internal Compiler Error: signals a bug in the compiler itself — an invariant
// an earlier phase was supposed to guarantee (e.g. the lexer only emits
// operators the parser knows, the parser only builds IR the codegen handles).
// It is NOT for malformed user input; that should be reported as a diagnostic.
// Prints the C source location and aborts, so the failure surfaces in a
// debugger and fails tests loudly instead of looking like a clean exit(1).
#define ICE(...) \
	do { \
		fprintf(stderr, "internal compiler error %s:%d: ", __FILE__, __LINE__); \
		fprintf(stderr, __VA_ARGS__); \
		fprintf(stderr, "\n"); \
		abort(); \
	} while (0)
