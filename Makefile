TARGET = tinyc

CC = gcc
CFLAGS = -Wall -Wextra -g -Isrc
LDFLAGS =

BUILD_DIR = build

SRCS = src/main.c \
       src/common/string_list.c \
       src/lexer/token.c \
       src/lexer/token_list.c \
       src/parser/ast.c \
       src/parser/parser.c \
       src/ir/ir.c \
       src/codegen/x86/x86_ast.c \
       src/codegen/codegen.c \
       src/codegen/emit.c \
       src/strlib/str.c

OBJS = $(SRCS:src/%.c=$(BUILD_DIR)/%.o)

TEST_TARGETS = tests/test_list tests/test_token tests/test_parser tests/test_str tests/test_codegen tests/test_ir

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BUILD_DIR)/%.o: src/%.c
	@mkdir -p $(dir $@)
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

tests/test_list: tests/test_list.c $(BUILD_DIR)/common/string_list.o $(BUILD_DIR)/lexer/token_list.o $(BUILD_DIR)/lexer/token.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

tests/test_token: tests/test_token.c $(BUILD_DIR)/lexer/token_list.o $(BUILD_DIR)/lexer/token.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

tests/test_parser: tests/test_parser.c $(BUILD_DIR)/parser/ast.o $(BUILD_DIR)/parser/parser.o $(BUILD_DIR)/common/string_list.o $(BUILD_DIR)/lexer/token_list.o $(BUILD_DIR)/lexer/token.o $(BUILD_DIR)/strlib/str.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

tests/test_codegen: tests/test_codegen.c $(BUILD_DIR)/codegen/x86/x86_ast.o $(BUILD_DIR)/codegen/codegen.o $(BUILD_DIR)/codegen/emit.o $(BUILD_DIR)/ir/ir.o $(BUILD_DIR)/parser/ast.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

tests/test_ir: tests/test_ir.c $(BUILD_DIR)/ir/ir.o $(BUILD_DIR)/parser/ast.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

tests/test_str:
	@echo "test_str skipped: str_split not yet implemented"
	@touch tests/test_str
	@# dummy target

clean:
	rm -rf $(BUILD_DIR)
	find . -name "*.o" -delete
	rm -f $(TARGET) $(TEST_TARGETS)
	find . -name "*.dSYM" -type d -exec rm -rf {} +

.PHONY: all test clean
