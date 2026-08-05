CC = gcc
CFLAGS = -O3 -march=native -Wall -Wextra -lm
LDFLAGS = -lm

TARGET = disk_bench
SRC = src/disk_bench.c
OBJ = $(SRC:.c=.o)

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(LDFLAGS)

clean:
	rm -f $(TARGET) $(OBJ)

run: $(TARGET)
	./$(TARGET)

install: $(TARGET)
	install -m 755 $(TARGET) /usr/local/bin/

.PHONY: help
help:
	@echo "RK3588 Disk Speed Benchmark"
	@echo "Usage: make [target]"
	@echo ""
	@echo "Targets:"
	@echo "  all     - Build the benchmark tool"
	@echo "  clean   - Remove build artifacts"
	@echo "  run     - Build and run benchmark"
	@echo "  install - Install to /usr/local/bin/"
	@echo ""
	@echo "Running manually:"
	@echo "  ./disk_bench                    # Default: /tmp/disk_bench_test.bin, 1MB blocks"
	@echo "  ./disk_bench /path/to/test 512  # Custom path and 512KB blocks"
