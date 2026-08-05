# RK3588 Toolkit Suite

Comprehensive benchmarking, diagnostics, and hardware testing toolkits for Rockchip RK3588 SoC and derivative boards.

## Structure

```
toolkit/
├── README.md                    # This file
├── build_all.sh                 # Master build script
├── rk3588/                      # Generic RK3588 tools
│   ├── benchmarks/              # Performance benchmarking tools
│   │   ├── disk-speed/          # Disk I/O sequential performance
│   │   ├── random-io/           # Disk I/O random access performance
│   │   ├── memory-bench/        # Memory bandwidth testing
│   │   └── cpu-gpu-bench/       # CPU and GPU benchmarking
│   ├── hardware/                # Hardware interface utilities
│   │   ├── thermal_monitor.sh   # Thermal monitoring tool
│   │   └── sysinfo.sh           # System information diagnostic
│   └── utils/                   # General utilities
│       └── stress_test/         # CPU/Memory stress testing
└── boards/                      # Board-specific implementations
    └── orange-pi-5-plus/        # Orange Pi 5 Plus specific tools
        ├── benchmarks/
        ├── drivers/
        └── utils/
```

## Quick Start

```bash
# Build all toolkits
cd toolkit
./build_all.sh

# Or build individual tools
cd rk3588/benchmarks/disk-speed && make
cd rk3588/benchmarks/cpu-gpu-bench && make
cd rk3588/benchmarks/memory-bench && make
cd rk3588/benchmarks/random-io && make
cd rk3588/utils && make
```

## Available Tools

### Benchmarking Tools

| Tool | Language | Purpose |
|------|----------|---------|
| **disk-speed** | C | Sequential disk read/write performance (MB/s) |
| **random-io** | C | Random I/O operations performance (IOPS) |
| **memory-bench** | C | Memory bandwidth & latency testing (GB/s) |
| **cpu-gpu-bench** | C++ | CPU scalar, SIMD, parallel & GPU capabilities |

### Hardware Diagnostics

| Tool | Type | Purpose |
|------|------|---------|
| **sysinfo.sh** | Bash | Complete system information snapshot |
| **thermal_monitor.sh** | Bash | Real-time thermal zone monitoring |
| **stress_test** | C | CPU/Memory stress testing with statistics |

## Usage Examples

### Sequential Disk Performance
```bash
./rk3588/benchmarks/disk-speed/disk_bench
./rk3588/benchmarks/disk-speed/disk_bench /mnt/storage 1024  # 1MB blocks
```

### Random I/O Performance
```bash
./rk3588/benchmarks/random-io/random_io_bench
./rk3588/benchmarks/random-io/random_io_bench /custom/path
```

### Memory Bandwidth
```bash
./rk3588/benchmarks/memory-bench/memory_bench
./rk3588/benchmarks/memory-bench/memory_bench 256  # 256MB test
```

### CPU/GPU Benchmarking
```bash
./rk3588/benchmarks/cpu-gpu-bench/cpu_gpu_bench
```

### System Information
```bash
./rk3588/hardware/sysinfo.sh
```

### Thermal Monitoring
```bash
# Continuous monitoring (1s refresh)
./rk3588/hardware/thermal_monitor.sh watch

# With custom interval (5s)
./rk3588/hardware/thermal_monitor.sh watch 5

# Single snapshot
./rk3588/hardware/thermal_monitor.sh snapshot
```

### Stress Testing
```bash
# CPU stress: 30 seconds, all cores
./rk3588/utils/stress_test cpu 30

# CPU stress: 60 seconds, 4 threads
./rk3588/utils/stress_test cpu 60 4

# Memory stress: 30 seconds, all cores
./rk3588/utils/stress_test memory 30
```

## Build System

Each toolkit directory contains:
- **Makefile**: Standard make targets (all, clean, run, install)
- **src/**: Source code files
- **include/**: Header files (where applicable)

### Build Options

```bash
# Build
make

# Run immediately after build
make run

# Install to /usr/local/bin/
make install

# Clean build artifacts
make clean

# Show help
make help
```

## Language Optimization Strategy

- **C**: High-performance benchmarking (disk, memory, random I/O)
- **C++**: GPU/SIMD operations and complex computations
- **Bash**: System diagnostics and monitoring (minimal overhead)
- **Python**: Future analysis and visualization tools

## Performance Notes

- All C/C++ tools compiled with `-O3 -march=native` for RK3588
- Uses NEON SIMD intrinsics where applicable
- OpenMP parallelization for multi-core testing
- Minimal OS overhead for accurate benchmarking

## Board-Specific Customization

Board-specific tools and overrides can be placed in `toolkit/boards/<board-name>/`:
- Override generic benchmarks with board-specific versions
- Add board-specific drivers and utilities
- Maintain separate configurations per board

## Expected Performance Ranges (Reference)

### Orange Pi 5 Plus (RK3588)
- **Sequential Disk Read**: 200-400 MB/s (depends on storage)
- **Disk IOPS**: 1000-3000 IOPS (depends on storage)
- **Memory Bandwidth**: 15-20 GB/s
- **CPU Performance**: 2-5 GFLOPS per core
- **Multi-core Speedup**: 6-8x with OpenMP
- **GPU Peak**: ~2.16 TFLOPS FP32

## Contributing

To add new toolkits:
1. Create subdirectory in `rk3588/` or `boards/<board>/`
2. Include Makefile with standard targets
3. Add README.md with usage documentation
4. Update master `build_all.sh` if needed

## Future Additions

- [x] Disk speed benchmarking
- [x] CPU/GPU benchmarking
- [x] Memory benchmarking
- [x] Random I/O testing
- [x] Thermal monitoring
- [x] System diagnostics
- [x] Stress testing
- [ ] Power consumption profiling
- [ ] GPU vendor library integration (Panfrost, Mali)
- [ ] Network performance testing
- [ ] Real-time monitoring dashboard
- [ ] Comparative analysis tools
- [ ] AI/ML workload benchmarks
