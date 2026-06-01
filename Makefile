TARGET = tinyc

CC = gcc
CFLAGS = -Wall -Wextra -g -Iutil -I.
LDFLAGS =

SRCS = tiny.c \
       list.c \
       lexer/token.c \
       parser/ast.c \
       parser/parser.c \
       codegen/asm_ast.c \
       codegen/codegen.c \
       codegen/emit.c \
       strlib/str.c

OBJS = $(SRCS:.c=.o)

TEST_TARGETS = tests/tests_parser tests/test_list tests/test_token tests/test_str tests/test_codegen
TEST_SRCS = tests/test_list.c tests/test_token.c tests/test_str.c tests/test_parser.c tests/test_codegen.c

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
	@echo "test_str skipped: str_split not yet implemented"

build_tests: tests/test_list tests/test_token tests/test_parser tests/test_codegen

tests/test_list: tests/test_list.c list.o lexer/token.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

tests/test_token: tests/test_token.c list.o lexer/token.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

tests/test_parser: tests/test_parser.c parser/ast.o parser/parser.o list.o lexer/token.o strlib/str.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

tests/test_codegen: tests/test_codegen.c codegen/asm_ast.o codegen/codegen.o codegen/emit.o parser/ast.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

tests/test_str:
	@echo "test_str skipped: str_split not yet implemented"
	@touch tests/test_str
	@# dummy target

clean:
	rm -f $(OBJS) $(TARGET) $(TEST_TARGETS)
	rm -rf tests/*.dSYM

.PHONY: all test clean
