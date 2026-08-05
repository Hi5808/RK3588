#include <iostream>
#include <cmath>
#include <cstring>
#include <chrono>
#include <omp.h>
#include <arm_neon.h>

#define VERSION "1.0.0"
#define ARRAY_SIZE (1024 * 1024)  /* 1M elements */
#define ITERATIONS 100

using namespace std;
using namespace std::chrono;

class CPUBenchmark {
private:
    double *data_a, *data_b, *data_result;
    size_t size;

public:
    CPUBenchmark(size_t sz = ARRAY_SIZE) : size(sz) {
        data_a = new double[size];
        data_b = new double[size];
        data_result = new double[size];

        for (size_t i = 0; i < size; i++) {
            data_a[i] = (double)i * 0.5;
            data_b[i] = (double)i * 0.3;
        }
    }

    ~CPUBenchmark() {
        delete[] data_a;
        delete[] data_b;
        delete[] data_result;
    }

    double benchmark_fp64_scalar() {
        auto start = high_resolution_clock::now();
        long long ops = 0;

        for (int iter = 0; iter < ITERATIONS; iter++) {
            for (size_t i = 0; i < size; i++) {
                data_result[i] = data_a[i] * data_b[i] + sin(data_a[i]) * cos(data_b[i]);
                ops += 5;  /* mul, add, sin, cos, mul */
            }
        }

        auto end = high_resolution_clock::now();
        auto duration = duration_cast<nanoseconds>(end - start);
        double gflops = (ops / 1e9) / (duration.count() / 1e9);
        return gflops;
    }

    double benchmark_simd_neon() {
        auto start = high_resolution_clock::now();
        long long ops = 0;

        for (int iter = 0; iter < ITERATIONS; iter++) {
            for (size_t i = 0; i < size; i += 2) {
                float32x2_t va = vcvt_f32_f64(vld1_f64(&data_a[i]));
                float32x2_t vb = vcvt_f32_f64(vld1_f64(&data_b[i]));
                float32x2_t vresult = vmul_f32(va, vb);
                float32x2_t result_f = vmul_f32(vresult, vresult);
                ops += 4;
            }
        }

        auto end = high_resolution_clock::now();
        auto duration = duration_cast<nanoseconds>(end - start);
        double gflops = (ops / 1e9) / (duration.count() / 1e9);
        return gflops;
    }

    double benchmark_parallel() {
        auto start = high_resolution_clock::now();
        long long ops = 0;

        #pragma omp parallel for reduction(+:ops)
        for (size_t i = 0; i < size; i++) {
            data_result[i] = data_a[i] * data_b[i] + sqrt(fabs(data_a[i]));
            ops += 4;
        }

        auto end = high_resolution_clock::now();
        auto duration = duration_cast<nanoseconds>(end - start);
        double gflops = (ops / 1e9) / (duration.count() / 1e9);
        return gflops;
    }

    int get_core_count() {
        return omp_get_max_threads();
    }
};

class GPUInfo {
public:
    void print_info() {
        cout << "\nGPU Information:\n";
        cout << "  GPU Model:       Mali-G610 (estimated)\n";
        cout << "  GPU Cores:       6 cores\n";
        cout << "  Max Clock:       ~900 MHz (estimated)\n";
        cout << "  Peak FP32:       ~2.16 TFLOPS (estimated)\n";
        cout << "  Peak FP16:       ~4.32 TFLOPS (estimated)\n";
        cout << "  Note:            GPU benchmarking requires Mali GPU libraries\n";
    }
};

void print_cpu_info(int cores) {
    cout << "CPU Information:\n";
    cout << "  Processor:       Rockchip RK3588\n";
    cout << "  Cores:           " << cores << " (detected)\n";
    cout << "  Architecture:    ARMv8-A (64-bit)\n";
    cout << "  Extensions:      NEON, VFPv4, ASIMD\n";
}

int main() {
    cout << "RK3588 CPU/GPU Benchmark v" << VERSION << "\n\n";

    CPUBenchmark bench;
    int cores = bench.get_core_count();

    print_cpu_info(cores);
    cout << "\nRunning CPU Benchmarks (" << ITERATIONS << " iterations)...\n";

    double scalar_gflops = bench.benchmark_fp64_scalar();
    cout << "  FP64 Scalar:     " << scalar_gflops << " GFLOPS\n";

    double simd_gflops = bench.benchmark_simd_neon();
    cout << "  NEON SIMD:       " << simd_gflops << " GFLOPS\n";

    double parallel_gflops = bench.benchmark_parallel();
    cout << "  Parallel OMP:    " << parallel_gflops << " GFLOPS\n";

    double speedup = parallel_gflops / scalar_gflops;
    cout << "  Speedup:         " << speedup << "x\n";

    GPUInfo gpu;
    gpu.print_info();

    cout << "\nBenchmark Complete!\n";

    return 0;
}
