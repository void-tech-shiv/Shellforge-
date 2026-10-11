CC := gcc
CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -O2
TARGET := shellforge
SRC := src/main.c

.PHONY: all clean test run

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET)

run: $(TARGET)
	./$(TARGET)

test: $(TARGET)
	bash tests/test_shellforge.sh

clean:
	rm -f $(TARGET)
