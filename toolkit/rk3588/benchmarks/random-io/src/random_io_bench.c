#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/stat.h>

#define VERSION "1.0.0"
#define BLOCK_SIZE 4096
#define NUM_OPERATIONS 10000

double get_time_seconds() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

typedef struct {
    double iops;
    double latency_ms;
    double duration_sec;
} io_result;

io_result random_read_benchmark(const char *filepath) {
    io_result result = {0};
    char buffer[BLOCK_SIZE];

    int fd = open(filepath, O_RDONLY);
    if (fd < 0) {
        perror("open failed");
        return result;
    }

    struct stat st;
    fstat(fd, &st);
    off_t file_size = st.st_size;

    double start = get_time_seconds();

    for (int i = 0; i < NUM_OPERATIONS; i++) {
        off_t offset = (random() % (file_size - BLOCK_SIZE));
        if (lseek(fd, offset, SEEK_SET) < 0) {
            perror("lseek failed");
            break;
        }

        ssize_t nread = read(fd, buffer, BLOCK_SIZE);
        if (nread < 0) {
            perror("read failed");
            break;
        }
    }

    double end = get_time_seconds();
    result.duration_sec = end - start;
    result.iops = NUM_OPERATIONS / result.duration_sec;
    result.latency_ms = (result.duration_sec / NUM_OPERATIONS) * 1000;

    close(fd);
    return result;
}

io_result random_write_benchmark(const char *filepath) {
    io_result result = {0};
    char buffer[BLOCK_SIZE];
    memset(buffer, 0xAA, BLOCK_SIZE);

    int fd = open(filepath, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        perror("open failed");
        return result;
    }

    /* Pre-allocate file */
    lseek(fd, 1024 * BLOCK_SIZE - 1, SEEK_SET);
    write(fd, "", 1);

    double start = get_time_seconds();

    for (int i = 0; i < NUM_OPERATIONS; i++) {
        off_t offset = (random() % (1024 * BLOCK_SIZE - BLOCK_SIZE));
        if (lseek(fd, offset, SEEK_SET) < 0) {
            perror("lseek failed");
            break;
        }

        ssize_t nwritten = write(fd, buffer, BLOCK_SIZE);
        if (nwritten < 0) {
            perror("write failed");
            break;
        }
    }

    double end = get_time_seconds();
    result.duration_sec = end - start;
    result.iops = NUM_OPERATIONS / result.duration_sec;
    result.latency_ms = (result.duration_sec / NUM_OPERATIONS) * 1000;

    close(fd);
    return result;
}

void print_result(const char *test_name, io_result *result) {
    printf("%s:\n", test_name);
    printf("  IOPS:            %.2f\n", result->iops);
    printf("  Latency:         %.3f ms\n", result->latency_ms);
    printf("  Total Time:      %.3f sec\n\n", result->duration_sec);
}

int main(int argc, char *argv[]) {
    const char *test_file = "/tmp/random_io_test.bin";

    if (argc > 1) {
        test_file = argv[1];
    }

    printf("RK3588 Random I/O Benchmark v%s\n", VERSION);
    printf("Test file: %s\n", test_file);
    printf("Block size: %d bytes\n", BLOCK_SIZE);
    printf("Operations: %d\n\n", NUM_OPERATIONS);

    io_result write_result = random_write_benchmark(test_file);
    print_result("Random Write", &write_result);

    io_result read_result = random_read_benchmark(test_file);
    print_result("Random Read", &read_result);

    printf("I/O Performance Summary:\n");
    printf("  Read/Write Ratio: %.2fx\n", read_result.iops / write_result.iops);
    printf("  Write Latency:    %.3f ms\n", write_result.latency_ms);
    printf("  Read Latency:     %.3f ms\n", read_result.latency_ms);

    unlink(test_file);
    return 0;
}
