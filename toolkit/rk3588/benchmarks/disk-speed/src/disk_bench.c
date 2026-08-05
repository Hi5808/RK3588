#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <math.h>

#define VERSION "1.0.0"
#define BUFFER_SIZE (1024 * 1024)  /* 1MB buffer */
#define TEST_DURATION 10            /* seconds */

typedef struct {
    double total_bytes;
    double min_speed;
    double max_speed;
    double avg_speed;
    unsigned long iterations;
    double duration;
} benchmark_result;

double get_time_seconds() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

benchmark_result run_sequential_write(const char *filepath, size_t block_size, int duration) {
    benchmark_result result = {0};
    char *buffer = malloc(block_size);
    if (!buffer) {
        perror("malloc failed");
        return result;
    }

    memset(buffer, 0xAA, block_size);

    int fd = open(filepath, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        perror("open failed");
        free(buffer);
        return result;
    }

    double start = get_time_seconds();
    double elapsed = 0;
    double min_speed = 1e9, max_speed = 0;
    unsigned long iterations = 0;
    double total_bytes = 0;

    while (elapsed < duration) {
        ssize_t written = write(fd, buffer, block_size);
        if (written < 0) {
            perror("write failed");
            break;
        }

        double now = get_time_seconds();
        elapsed = now - start;
        total_bytes += written;
        iterations++;

        double speed_mbps = (written / (1024.0 * 1024.0)) / (elapsed / iterations);
        if (speed_mbps > 0) {
            min_speed = fmin(min_speed, speed_mbps);
            max_speed = fmax(max_speed, speed_mbps);
        }
    }

    close(fd);

    result.total_bytes = total_bytes;
    result.duration = elapsed;
    result.iterations = iterations;
    result.avg_speed = (total_bytes / (1024.0 * 1024.0)) / elapsed;
    result.min_speed = min_speed;
    result.max_speed = max_speed;

    free(buffer);
    return result;
}

benchmark_result run_sequential_read(const char *filepath, size_t block_size, int duration) {
    benchmark_result result = {0};
    char *buffer = malloc(block_size);
    if (!buffer) {
        perror("malloc failed");
        return result;
    }

    int fd = open(filepath, O_RDONLY);
    if (fd < 0) {
        perror("open failed");
        free(buffer);
        return result;
    }

    double start = get_time_seconds();
    double elapsed = 0;
    double min_speed = 1e9, max_speed = 0;
    unsigned long iterations = 0;
    double total_bytes = 0;

    while (elapsed < duration) {
        ssize_t nread = read(fd, buffer, block_size);
        if (nread <= 0) {
            lseek(fd, 0, SEEK_SET);
            nread = read(fd, buffer, block_size);
        }

        if (nread < 0) {
            perror("read failed");
            break;
        }

        double now = get_time_seconds();
        elapsed = now - start;
        total_bytes += nread;
        iterations++;

        double speed_mbps = (nread / (1024.0 * 1024.0)) / (elapsed / iterations);
        if (speed_mbps > 0) {
            min_speed = fmin(min_speed, speed_mbps);
            max_speed = fmax(max_speed, speed_mbps);
        }
    }

    close(fd);

    result.total_bytes = total_bytes;
    result.duration = elapsed;
    result.iterations = iterations;
    result.avg_speed = (total_bytes / (1024.0 * 1024.0)) / elapsed;
    result.min_speed = min_speed;
    result.max_speed = max_speed;

    free(buffer);
    return result;
}

void print_result(const char *test_name, benchmark_result *result) {
    printf("\n%s Results:\n", test_name);
    printf("  Duration:        %.2f seconds\n", result->duration);
    printf("  Total Data:      %.2f MB\n", result->total_bytes / (1024.0 * 1024.0));
    printf("  Iterations:      %lu\n", result->iterations);
    printf("  Avg Speed:       %.2f MB/s\n", result->avg_speed);
    printf("  Min Speed:       %.2f MB/s\n", result->min_speed);
    printf("  Max Speed:       %.2f MB/s\n", result->max_speed);
}

int main(int argc, char *argv[]) {
    const char *test_file = "/tmp/disk_bench_test.bin";
    size_t block_size = 1024 * 1024;  /* 1MB default */

    if (argc > 1) {
        test_file = argv[1];
    }
    if (argc > 2) {
        block_size = atol(argv[2]) * 1024;  /* KB to bytes */
    }

    printf("RK3588 Disk Benchmark Tool v%s\n", VERSION);
    printf("Test file: %s\n", test_file);
    printf("Block size: %zu KB\n\n", block_size / 1024);

    benchmark_result write_result = run_sequential_write(test_file, block_size, TEST_DURATION);
    print_result("Sequential Write", &write_result);

    benchmark_result read_result = run_sequential_read(test_file, block_size, TEST_DURATION);
    print_result("Sequential Read", &read_result);

    unlink(test_file);

    return 0;
}
