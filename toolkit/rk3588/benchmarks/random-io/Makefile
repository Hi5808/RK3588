CC = gcc
CFLAGS = -O3 -march=native -Wall -Wextra
LDFLAGS =

TARGET = random_io_bench
SRC = src/random_io_bench.c
OBJ = $(SRC:.c=.o)

.PHONY: all clean run install help

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(LDFLAGS)

clean:
	rm -f $(TARGET) $(OBJ)

run: $(TARGET)
	./$(TARGET)

install: $(TARGET)
	install -m 755 $(TARGET) /usr/local/bin/

help:
	@echo "RK3588 Random I/O Benchmark"
	@echo "Usage: make [target]"
	@echo ""
	@echo "Targets:"
	@echo "  all     - Build the benchmark tool"
	@echo "  clean   - Remove build artifacts"
	@echo "  run     - Build and run benchmark"
	@echo "  install - Install to /usr/local/bin/"
	@echo ""
	@echo "Running manually:"
	@echo "  ./random_io_bench                    # Default: /tmp/random_io_test.bin"
	@echo "  ./random_io_bench /path/to/test     # Custom path"
