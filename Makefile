CC := gcc
CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -Iinclude -D_POSIX_C_SOURCE=200809L
LDFLAGS :=
SRC := $(wildcard src/*.c)
OBJ := $(SRC:.c=.o)
TARGET := shellforge

.PHONY: all clean run test

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) $(LDFLAGS) -o $@

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET)

test: $(TARGET)
	bash tests/test_shellforge.sh

clean:
	rm -f $(OBJ) $(TARGET)
