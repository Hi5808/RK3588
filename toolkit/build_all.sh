#!/bin/bash

set -e

echo "Building RK3588 Toolkit Suite..."
echo "================================"

# Colors
GREEN='\033[0;32m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

cd "$(dirname "$0")"

# Build disk speed benchmark
echo -e "${BLUE}Building Disk Speed Benchmark...${NC}"
cd rk3588/benchmarks/disk-speed
make clean
make
echo -e "${GREEN}✓ Disk benchmark built${NC}\n"
cd ../../..

# Build CPU/GPU benchmark
echo -e "${BLUE}Building CPU/GPU Benchmark...${NC}"
cd rk3588/benchmarks/cpu-gpu-bench
make clean
make
echo -e "${GREEN}✓ CPU/GPU benchmark built${NC}\n"
cd ../../..

echo -e "${GREEN}All toolkits built successfully!${NC}"
echo ""
echo "Binaries available at:"
echo "  - toolkit/rk3588/benchmarks/disk-speed/disk_bench"
echo "  - toolkit/rk3588/benchmarks/cpu-gpu-bench/cpu_gpu_bench"
echo ""
echo "Run 'make install' in each directory to install to /usr/local/bin/"
