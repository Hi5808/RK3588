#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <omp.h>
#include <math.h>

#define VERSION "1.0.0"
#define MB (1024 * 1024)
#define ITERATIONS 5

typedef struct {
    double bandwidth_gbps;
    double latency_ns;
    double duration_sec;
} memory_result;

double get_time_seconds() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

memory_result sequential_memory_read(size_t size_mb) {
    memory_result result = {0};
    size_t size = size_mb * MB;
    char *buffer = malloc(size);
    if (!buffer) {
        perror("malloc failed");
        return result;
    }

    memset(buffer, 0xAA, size);
    volatile long sum = 0;

    double start = get_time_seconds();

    for (int iter = 0; iter < ITERATIONS; iter++) {
        for (size_t i = 0; i < size; i++) {
            sum += buffer[i];
        }
    }

    double end = get_time_seconds();
    result.duration_sec = end - start;

    long long total_bytes = (long long)size * ITERATIONS;
    result.bandwidth_gbps = (total_bytes / 1e9) / result.duration_sec;
    result.latency_ns = (result.duration_sec / (size * ITERATIONS)) * 1e9;

    free(buffer);
    return result;
}

memory_result random_memory_access(size_t size_mb) {
    memory_result result = {0};
    size_t size = size_mb * MB;
    char *buffer = malloc(size);
    if (!buffer) {
        perror("malloc failed");
        return result;
    }

    memset(buffer, 0xAA, size);

    size_t *indices = malloc(size * sizeof(size_t));
    for (size_t i = 0; i < size; i++) {
        indices[i] = (i * 7919) % size;  /* Prime multiplier for pseudo-random */
    }

    volatile long sum = 0;
    double start = get_time_seconds();

    for (int iter = 0; iter < ITERATIONS; iter++) {
        for (size_t i = 0; i < size; i++) {
            sum += buffer[indices[i]];
        }
    }

    double end = get_time_seconds();
    result.duration_sec = end - start;

    long long total_bytes = (long long)size * ITERATIONS;
    result.bandwidth_gbps = (total_bytes / 1e9) / result.duration_sec;
    result.latency_ns = (result.duration_sec / (size * ITERATIONS)) * 1e9;

    free(buffer);
    free(indices);
    return result;
}

memory_result memcpy_performance(size_t size_mb) {
    memory_result result = {0};
    size_t size = size_mb * MB;
    char *src = malloc(size);
    char *dst = malloc(size);

    if (!src || !dst) {
        perror("malloc failed");
        return result;
    }

    memset(src, 0xAA, size);
    memset(dst, 0, size);

    double start = get_time_seconds();

    for (int iter = 0; iter < ITERATIONS; iter++) {
        memcpy(dst, src, size);
    }

    double end = get_time_seconds();
    result.duration_sec = end - start;

    long long total_bytes = (long long)size * ITERATIONS;
    result.bandwidth_gbps = (total_bytes / 1e9) / result.duration_sec;

    free(src);
    free(dst);
    return result;
}

void print_result(const char *test_name, memory_result *result) {
    printf("%s:\n", test_name);
    printf("  Bandwidth:       %.2f GB/s\n", result->bandwidth_gbps);
    printf("  Latency:         %.2f ns\n", result->latency_ns);
    printf("  Duration:        %.3f sec\n\n", result->duration_sec);
}

int main(int argc, char *argv[]) {
    size_t test_size_mb = 64;  /* Default 64MB */

    if (argc > 1) {
        test_size_mb = atol(argv[1]);
    }

    printf("RK3588 Memory Bandwidth Benchmark v%s\n", VERSION);
    printf("Test size: %zu MB\n", test_size_mb);
    printf("Iterations: %d\n\n", ITERATIONS);

    memory_result seq_read = sequential_memory_read(test_size_mb);
    print_result("Sequential Read", &seq_read);

    memory_result rand_access = random_memory_access(test_size_mb);
    print_result("Random Access", &rand_access);

    memory_result memcpy = memcpy_performance(test_size_mb);
    print_result("Memcpy", &memcpy);

    printf("Memory Hierarchy Analysis:\n");
    printf("  L1/L2 Cache Latency:  ~%.1f ns\n", rand_access.latency_ns);
    printf("  Sequential vs Random: %.2fx degradation\n", seq_read.bandwidth_gbps / rand_access.bandwidth_gbps);

    return 0;
}
