# RK3588 Benchmarking Tools

High-performance benchmarking suite for Rockchip RK3588 SoC.

## Tools Overview

### 1. Disk Speed Benchmark (`disk-speed/`)

**Language:** C (optimized for performance)

Comprehensive disk I/O performance testing tool with sequential read/write operations.

**Features:**
- Sequential read/write performance
- Configurable block sizes
- Latency and throughput metrics
- Minimal OS overhead

**Building:**
```bash
cd disk-speed
make
```

**Usage:**
```bash
./disk_bench                    # Default: 1MB blocks
./disk_bench /path/to/test 512  # Custom path, 512KB blocks
```

**Output Example:**
```
Sequential Write Results:
  Duration:        10.00 seconds
  Total Data:      1024.50 MB
  Iterations:      1024
  Avg Speed:       102.45 MB/s
  Min Speed:       95.32 MB/s
  Max Speed:       110.87 MB/s

Sequential Read Results:
  Duration:        10.00 seconds
  Total Data:      2048.75 MB
  Iterations:      2048
  Avg Speed:       204.88 MB/s
  Min Speed:       198.45 MB/s
  Max Speed:       215.32 MB/s
```

### 2. Random I/O Benchmark (`random-io/`)

**Language:** C (optimized for I/O operations)

Random access I/O performance testing with pseudo-random seek patterns.

**Features:**
- Random read/write IOPS measurement
- Latency per operation
- Read/write ratio analysis
- Pre-allocated file for consistent testing

**Building:**
```bash
cd random-io
make
```

**Usage:**
```bash
./random_io_bench                    # Default: /tmp/random_io_test.bin
./random_io_bench /custom/path       # Custom file path
```

**Output Example:**
```
Random Write:
  IOPS:            2500.45
  Latency:         0.400 ms
  Total Time:      4.000 sec

Random Read:
  IOPS:            5600.30
  Latency:         0.179 ms
  Total Time:      1.786 sec

I/O Performance Summary:
  Read/Write Ratio: 2.24x
  Write Latency:    0.400 ms
  Read Latency:     0.179 ms
```

### 3. Memory Bandwidth Benchmark (`memory-bench/`)

**Language:** C (optimized SIMD operations)

Comprehensive memory hierarchy performance testing.

**Features:**
- Sequential read performance
- Random access patterns
- Memcpy performance
- Memory latency analysis
- L1/L2 cache characterization

**Building:**
```bash
cd memory-bench
make
```

**Usage:**
```bash
./memory_bench              # Default: 64MB test
./memory_bench 256          # 256MB test
./memory_bench 1024         # 1GB test
```

**Output Example:**
```
Sequential Read:
  Bandwidth:       18.45 GB/s
  Latency:         5.42 ns
  Duration:        0.347 sec

Random Access:
  Bandwidth:       8.12 GB/s
  Latency:         12.31 ns
  Duration:        0.787 sec

Memcpy:
  Bandwidth:       22.67 GB/s
  Latency:         0.00 ns
  Duration:        0.281 sec

Memory Hierarchy Analysis:
  L1/L2 Cache Latency:  ~12.31 ns
  Sequential vs Random: 2.27x degradation
```

### 4. CPU/GPU Benchmark (`cpu-gpu-bench/`)

**Language:** C++ (optimized SIMD/NEON)

Comprehensive CPU and GPU performance testing with multi-core analysis.

**Features:**
- FP64 scalar performance
- NEON SIMD optimization
- OpenMP parallel performance
- Multi-core speedup metrics
- GPU capability detection
- Thermal considerations

**Building:**
```bash
cd cpu-gpu-bench
make
```

**Usage:**
```bash
./cpu_gpu_bench
```

**Output Example:**
```
CPU Information:
  Processor:       Rockchip RK3588
  Cores:           8 (detected)
  Architecture:    ARMv8-A (64-bit)
  Extensions:      NEON, VFPv4, ASIMD

Running CPU Benchmarks (100 iterations)...
  FP64 Scalar:     2.45 GFLOPS
  NEON SIMD:       8.92 GFLOPS
  Parallel OMP:    18.54 GFLOPS
  Speedup:         7.57x

GPU Information:
  GPU Model:       Mali-G610 (estimated)
  GPU Cores:       6 cores
  Max Clock:       ~900 MHz (estimated)
  Peak FP32:       ~2.16 TFLOPS (estimated)
  Peak FP16:       ~4.32 TFLOPS (estimated)
  Note:            GPU benchmarking requires Mali GPU libraries
```

## Benchmark Workflow

### Complete Performance Profile
```bash
# System information
../hardware/sysinfo.sh

# Memory test
memory-bench/memory_bench

# CPU test
cpu-gpu-bench/cpu_gpu_bench

# Disk sequential
disk-speed/disk_bench

# Disk random
random-io/random_io_bench

# Thermal baseline
../hardware/thermal_monitor.sh snapshot
```

### Storage Analysis
```bash
# Test on specific storage
disk-speed/disk_bench /mnt/ssd 512    # SSD with 512KB blocks
disk-speed/disk_bench /mnt/emmc 1024  # eMMC with 1MB blocks
random-io/random_io_bench /mnt/ssd
```

### Load Testing
```bash
# Stress during benchmarking
# Terminal 1: Start stress test
../utils/stress_test cpu 120

# Terminal 2: Run benchmarks during load
memory-bench/memory_bench 256
cpu-gpu-bench/cpu_gpu_bench
disk-speed/disk_bench

# Monitor thermal in Terminal 3
../hardware/thermal_monitor.sh watch
```

## Performance Interpretation

### Disk Performance
- **Sequential MB/s**: Affected by interface (SD, eMMC, USB, NVMe)
- **Random IOPS**: Affected by filesystem, storage type, queue depth
- **Typical Orange Pi 5+ eMMC**: 100-200 MB/s seq, 2000-3000 IOPS random

### Memory Performance
- **Sequential Bandwidth**: ~15-20 GB/s on LPDDR4/5
- **Random Access Latency**: ~5-10ns L1, ~50-100ns L3
- **Memcpy**: Often hardware-limited by bus

### CPU Performance
- **Scalar FP64**: 2-5 GFLOPS per core
- **SIMD Boost**: 3-4x improvement with NEON
- **Multi-core Scaling**: 6-8x with 8 cores (diminishing returns)

### GPU Performance
- **Mali-G610**: ~2.16 TFLOPS FP32 peak
- **Actual Workload**: 30-60% peak depending on algorithm

## Optimization Notes

- All tools compiled with `-O3 -march=native` for maximum performance
- NEON intrinsics used where applicable for SIMD
- OpenMP pragmas for automatic parallelization
- Memory tests designed to stress L1/L2/L3 hierarchy
- Disk tests minimize filesystem caching interference

## Future Enhancements

- GPU vendor library integration (Mali, Panfrost)
- Real-time result logging to CSV
- Comparative analysis tools
- Thermal throttling detection
- Network bandwidth testing
- AI/ML workload benchmarks
- Power consumption correlation
