CFLAGS=-O2 -g -std=c17 -Wall -Wextra -pedantic -fsanitize=address -static-libasan -fno-omit-frame-pointer -g -msse4.1 -mssse3
SRC_DIR=src

.PHONY: all
all: main
main: $(SRC_DIR)/main.c $(SRC_DIR)/file_parsing.c $(SRC_DIR)/scale.c $(SRC_DIR)/timing.c $(SRC_DIR)/test.c $(SRC_DIR)/util.c
	$(CC) $(CFLAGS) -o $@ $^

.PHONY: clean
clean:
	rm -f main
	rm -f test/out/*.ppm
