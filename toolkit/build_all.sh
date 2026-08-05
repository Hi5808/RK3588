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
cd ../../../

# Build random I/O benchmark
echo -e "${BLUE}Building Random I/O Benchmark...${NC}"
cd rk3588/benchmarks/random-io
make clean
make
echo -e "${GREEN}✓ Random I/O benchmark built${NC}\n"
cd ../../../

# Build memory benchmark
echo -e "${BLUE}Building Memory Bandwidth Benchmark...${NC}"
cd rk3588/benchmarks/memory-bench
make clean
make
echo -e "${GREEN}✓ Memory benchmark built${NC}\n"
cd ../../../

# Build CPU/GPU benchmark
echo -e "${BLUE}Building CPU/GPU Benchmark...${NC}"
cd rk3588/benchmarks/cpu-gpu-bench
make clean
make
echo -e "${GREEN}✓ CPU/GPU benchmark built${NC}\n"
cd ../../../

# Build stress test
echo -e "${BLUE}Building Stress Test Tool...${NC}"
cd rk3588/utils
make clean
make
echo -e "${GREEN}✓ Stress test tool built${NC}\n"
cd ../../

echo -e "${GREEN}All toolkits built successfully!${NC}"
echo ""
echo "Binaries available at:"
echo "  - toolkit/rk3588/benchmarks/disk-speed/disk_bench"
echo "  - toolkit/rk3588/benchmarks/random-io/random_io_bench"
echo "  - toolkit/rk3588/benchmarks/memory-bench/memory_bench"
echo "  - toolkit/rk3588/benchmarks/cpu-gpu-bench/cpu_gpu_bench"
echo "  - toolkit/rk3588/utils/stress_test"
echo ""
echo "Scripts available at:"
echo "  - toolkit/rk3588/hardware/thermal_monitor.sh"
echo "  - toolkit/rk3588/hardware/sysinfo.sh"
echo ""
echo "Run 'make install' in each directory to install to /usr/local/bin/"
echo ""
echo "Quick usage:"
echo "  # Disk performance"
echo "  toolkit/rk3588/benchmarks/disk-speed/disk_bench"
echo "  # Memory bandwidth"
echo "  toolkit/rk3588/benchmarks/memory-bench/memory_bench"
echo "  # CPU/GPU capabilities"
echo "  toolkit/rk3588/benchmarks/cpu-gpu-bench/cpu_gpu_bench"
echo "  # System info"
echo "  toolkit/rk3588/hardware/sysinfo.sh"
echo "  # Thermal monitoring"
echo "  toolkit/rk3588/hardware/thermal_monitor.sh watch"
echo "  # Stress test"
echo "  toolkit/rk3588/utils/stress_test cpu 30"
