CC = gcc
CFLAGS = -O3 -march=native -Wall -Wextra -pthread
LDFLAGS = -lpthread -lm

TARGET = stress_test
SRC = stress_test.c
OBJ = $(SRC:.c=.o)

.PHONY: all clean run install help

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(LDFLAGS)

clean:
	rm -f $(TARGET) $(OBJ)

run: $(TARGET)
	./$(TARGET) cpu 10

install: $(TARGET)
	install -m 755 $(TARGET) /usr/local/bin/

help:
	@echo "RK3588 Stress Test Tool"
	@echo "Usage: make [target]"
	@echo ""
	@echo "Targets:"
	@echo "  all     - Build the stress test tool"
	@echo "  clean   - Remove build artifacts"
	@echo "  run     - Build and run CPU stress (10s)"
	@echo "  install - Install to /usr/local/bin/"
	@echo ""
	@echo "Running manually:"
	@echo "  ./stress_test cpu 30 8        # CPU stress, 30s, 8 threads"
	@echo "  ./stress_test memory 60 4     # Memory stress, 60s, 4 threads"
