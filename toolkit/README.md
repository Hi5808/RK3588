# RK3588 Toolkit Suite

Comprehensive benchmarking and hardware testing toolkits for Rockchip RK3588 SoC and derivative boards.

## Structure

```
toolkit/
├── rk3588/              # Generic RK3588 tools
│   ├── benchmarks/      # Performance benchmarking tools
│   │   ├── disk-speed/  # Disk I/O performance testing
│   │   └── cpu-gpu-bench/ # CPU and GPU benchmarking
│   ├── hardware/        # Hardware interface utilities
│   └── utils/           # General utilities
└── boards/              # Board-specific implementations
    └── orange-pi-5-plus/ # Orange Pi 5 Plus specific tools
        ├── benchmarks/
        ├── drivers/
        └── utils/
```

## Available Toolkits

### Benchmarking Tools

#### Disk Speed Test (`rk3588/benchmarks/disk-speed/`)
- Sequential read/write performance
- Random I/O performance
- Block size analysis
- Latency measurements

#### CPU/GPU Benchmark (`rk3588/benchmarks/cpu-gpu-bench/`)
- CPU performance metrics
- GPU acceleration benchmarks
- NEON/SIMD utilization testing
- Mali GPU (G610) specific tests

## Building

Each toolkit has its own build system:

```bash
cd toolkit/rk3588/benchmarks/disk-speed
make

cd toolkit/rk3588/benchmarks/cpu-gpu-bench
make
```

## Language Selection

- **C**: High-performance benchmarking tools (disk, CPU)
- **C++**: GPU benchmarking with complex computations
- **Python**: Utility scripts and analysis tools

## Board-Specific Customization

Tools can be overridden per board in `toolkit/boards/<board-name>/`

## Future Additions

- Thermal management tools
- Power consumption profiler
- Memory benchmarking
- Network performance testing
- Hardware monitoring dashboard
