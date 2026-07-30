CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -O2 -Iinclude
LDLIBS = -lm

SRC = src/bmp.c src/filters.c
MAIN_SRC = src/main.c
TEST_SRC = tests/test_pixelforge.c

BIN = pixelforge
TEST_BIN = pixelforge_tests

.PHONY: all test clean

all: $(BIN)

$(BIN): $(SRC) $(MAIN_SRC)
	$(CC) $(CFLAGS) -o $(BIN) $(SRC) $(MAIN_SRC) $(LDLIBS)

$(TEST_BIN): $(SRC) $(TEST_SRC)
	$(CC) $(CFLAGS) -Itests -o $(TEST_BIN) $(SRC) $(TEST_SRC) $(LDLIBS)

test: $(TEST_BIN)
	./$(TEST_BIN)

clean:
	rm -f $(BIN) $(TEST_BIN) *.o pf_test_roundtrip_tmp.bmp
