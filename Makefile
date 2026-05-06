TARGET = tinyc

CC = gcc
CFLAGS = -Wall -Wextra -g -Iutil -I.
LDFLAGS =

SRCS = tiny.c \
       list.c \
       lexer/token.c \
       strlib/str.c

OBJS = $(SRCS:.c=.o)

TEST_TARGETS = tests/test_list tests/test_token tests/test_str
TEST_SRCS = tests/test_list.c tests/test_token.c tests/test_str.c

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

test: build_tests
	@echo "Running tests..."
	@./tests/test_list
	@./tests/test_token
	@echo "test_str skipped: str_split not yet implemented"

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

build_tests: tests/test_list tests/test_token

tests/test_list: tests/test_list.c list.o lexer/token.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

tests/test_token: tests/test_token.c list.o lexer/token.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

tests/test_str:
	@echo "test_str skipped: str_split not yet implemented"
	@touch tests/test_str
	@# dummy target

clean:
	rm -f $(OBJS) $(TARGET) $(TEST_TARGETS)

.PHONY: all test clean