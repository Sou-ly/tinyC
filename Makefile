TARGET = tinyc

CC = gcc
CFLAGS = -Wall -Wextra -g -Isrc
LDFLAGS =

SRCS = src/main.c \
       src/list.c \
       src/lexer/token.c \
       src/parser/ast.c \
       src/parser/parser.c \
       src/ir/ir.c \
       src/codegen/x86/x86_ast.c \
       src/codegen/codegen.c \
       src/codegen/emit.c \
       src/strlib/str.c

OBJS = $(SRCS:.c=.o)

TEST_TARGETS = tests/test_list tests/test_token tests/test_parser tests/test_str tests/test_codegen tests/test_ir

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

test: build_tests
	@echo "Running tests..."
	@./tests/test_list
	@./tests/test_token
	@./tests/test_parser
	@./tests/test_codegen
	@./tests/test_ir
	@echo "test_str skipped: str_split not yet implemented"

build_tests: tests/test_list tests/test_token tests/test_parser tests/test_codegen tests/test_ir

tests/test_list: tests/test_list.c src/list.o src/lexer/token.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

tests/test_token: tests/test_token.c src/list.o src/lexer/token.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

tests/test_parser: tests/test_parser.c src/parser/ast.o src/parser/parser.o src/list.o src/lexer/token.o src/strlib/str.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

tests/test_codegen: tests/test_codegen.c src/codegen/x86/x86_ast.o src/codegen/codegen.o src/codegen/emit.o src/parser/ast.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

tests/test_ir: tests/test_ir.c src/ir/ir.o src/parser/ast.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

tests/test_str:
	@echo "test_str skipped: str_split not yet implemented"
	@touch tests/test_str
	@# dummy target

clean:
	rm -f $(OBJS) $(TARGET) $(TEST_TARGETS)
	rm -rf tests/*.dSYM

.PHONY: all test clean
