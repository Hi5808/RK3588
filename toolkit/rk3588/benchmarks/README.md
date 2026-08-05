# RK3588 Benchmarking Tools

High-performance benchmarking suite for Rockchip RK3588 SoC.

## Tools

### 1. Disk Speed Benchmark (`disk-speed/`)

Comprehensive disk I/O performance testing tool.

**Language:** C (optimized for performance)

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

### 2. CPU/GPU Benchmark (`cpu-gpu-bench/`)

Comprehensive CPU and GPU performance testing.

**Language:** C++ (optimized SIMD/NEON)

**Features:**
- FP64 scalar performance
- NEON SIMD optimization
- OpenMP parallel performance
- GPU capability detection
- Multi-core speedup metrics

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

## Optimization Notes

- All tools compiled with `-O3 -march=native` for maximum performance
- Disk tool uses raw system calls for minimal overhead
- CPU tool uses NEON intrinsics and OpenMP for parallelization
- Results vary based on thermal conditions and system load

## Future Enhancements

- Memory bandwidth benchmarking
- GPU vendor library integration (Mali, Panfrost)
- Real-time result logging
- Comparative analysis tools
- Thermal throttling detection
