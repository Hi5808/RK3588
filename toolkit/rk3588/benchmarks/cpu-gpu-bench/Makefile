CXX = g++
CXXFLAGS = -O3 -march=native -Wall -Wextra -fopenmp -std=c++17
LDFLAGS = -lm -fopenmp

TARGET = cpu_gpu_bench
SRC = src/cpu_gpu_bench.cpp
OBJ = $(SRC:.cpp=.o)

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRC) $(LDFLAGS)

clean:
	rm -f $(TARGET) $(OBJ)

run: $(TARGET)
	./$(TARGET)

install: $(TARGET)
	install -m 755 $(TARGET) /usr/local/bin/

.PHONY: help
help:
	@echo "RK3588 CPU/GPU Benchmark"
	@echo "Usage: make [target]"
	@echo ""
	@echo "Targets:"
	@echo "  all     - Build the benchmark tool"
	@echo "  clean   - Remove build artifacts"
	@echo "  run     - Build and run benchmark"
	@echo "  install - Install to /usr/local/bin/"
	@echo ""
	@echo "Running manually:"
	@echo "  ./cpu_gpu_bench"
