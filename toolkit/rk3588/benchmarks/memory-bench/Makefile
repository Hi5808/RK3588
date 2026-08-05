CC = gcc
CFLAGS = -O3 -march=native -Wall -Wextra -lm
LDFLAGS = -lm

TARGET = memory_bench
SRC = src/memory_bench.c
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
	@echo "RK3588 Memory Bandwidth Benchmark"
	@echo "Usage: make [target]"
	@echo ""
	@echo "Targets:"
	@echo "  all     - Build the benchmark tool"
	@echo "  clean   - Remove build artifacts"
	@echo "  run     - Build and run benchmark"
	@echo "  install - Install to /usr/local/bin/"
	@echo ""
	@echo "Running manually:"
	@echo "  ./memory_bench              # Default: 64MB test"
	@echo "  ./memory_bench 256          # 256MB test"
