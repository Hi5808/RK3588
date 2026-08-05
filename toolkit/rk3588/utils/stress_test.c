#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <signal.h>
#include <math.h>
#include <sys/sysinfo.h>

#define VERSION "1.0.0"

volatile int running = 1;

typedef struct {
    int thread_id;
    unsigned long long operations;
    double duration;
} thread_stats;

double get_time_seconds() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

void signal_handler(int sig) {
    running = 0;
}

void *cpu_stress(void *arg) {
    thread_stats *stats = (thread_stats *)arg;
    volatile double result = 0.0;

    double start = get_time_seconds();

    while (running) {
        for (int i = 0; i < 1000; i++) {
            result = sin(result + i) * cos(result - i);
            result = sqrt(fabs(result));
        }
        stats->operations += 1000;
    }

    stats->duration = get_time_seconds() - start;
    pthread_exit(NULL);
}

void *memory_stress(void *arg) {
    thread_stats *stats = (thread_stats *)arg;
    size_t alloc_size = 10 * 1024 * 1024;  /* 10MB per iteration */

    double start = get_time_seconds();

    while (running) {
        char *buffer = malloc(alloc_size);
        if (buffer) {
            memset(buffer, 0xAA, alloc_size);
            free(buffer);
            stats->operations++;
        }
    }

    stats->duration = get_time_seconds() - start;
    pthread_exit(NULL);
}

void run_cpu_stress(int duration, int num_threads) {
    printf("CPU Stress Test (Duration: %d seconds, Threads: %d)\n", duration, num_threads);
    printf("====================================================\n\n");

    pthread_t *threads = malloc(num_threads * sizeof(pthread_t));
    thread_stats *stats = malloc(num_threads * sizeof(thread_stats));

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    for (int i = 0; i < num_threads; i++) {
        stats[i].thread_id = i;
        stats[i].operations = 0;
        pthread_create(&threads[i], NULL, cpu_stress, &stats[i]);
    }

    /* Print progress */
    for (int elapsed = 0; elapsed < duration && running; elapsed++) {
        sleep(1);
        printf("\rElapsed: %d/%d seconds", elapsed, duration);
        fflush(stdout);
    }

    running = 0;
    printf("\n\nWaiting for threads to finish...\n");

    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }

    printf("\nResults:\n");
    printf("%-10s %-15s %-15s\n", "Thread", "Operations", "Duration");
    printf("%-10s %-15s %-15s\n", "------", "-----------", "--------");

    unsigned long long total_ops = 0;
    for (int i = 0; i < num_threads; i++) {
        printf("%-10d %-15llu %-15.2f\n", i, stats[i].operations, stats[i].duration);
        total_ops += stats[i].operations;
    }

    printf("\nTotal operations: %llu\n", total_ops);
    printf("Avg operations/thread: %.0f\n", (double)total_ops / num_threads);

    free(threads);
    free(stats);
}

void run_memory_stress(int duration, int num_threads) {
    printf("Memory Stress Test (Duration: %d seconds, Threads: %d)\n", duration, num_threads);
    printf("======================================================\n\n");

    pthread_t *threads = malloc(num_threads * sizeof(pthread_t));
    thread_stats *stats = malloc(num_threads * sizeof(thread_stats));

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    for (int i = 0; i < num_threads; i++) {
        stats[i].thread_id = i;
        stats[i].operations = 0;
        pthread_create(&threads[i], NULL, memory_stress, &stats[i]);
    }

    for (int elapsed = 0; elapsed < duration && running; elapsed++) {
        sleep(1);
        printf("\rElapsed: %d/%d seconds", elapsed, duration);
        fflush(stdout);
    }

    running = 0;
    printf("\n\nWaiting for threads to finish...\n");

    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }

    printf("\nResults:\n");
    printf("%-10s %-15s %-15s\n", "Thread", "Allocations", "Duration");
    printf("%-10s %-15s %-15s\n", "------", "-----------", "--------");

    unsigned long long total_allocs = 0;
    for (int i = 0; i < num_threads; i++) {
        printf("%-10d %-15llu %-15.2f\n", i, stats[i].operations, stats[i].duration);
        total_allocs += stats[i].operations;
    }

    printf("\nTotal allocations: %llu\n", total_allocs);
    printf("Avg allocations/thread: %.0f\n", (double)total_allocs / num_threads);

    free(threads);
    free(stats);
}

int main(int argc, char *argv[]) {
    int duration = 10;
    int num_threads = get_nprocs();
    const char *test_type = "cpu";

    printf("RK3588 Stress Test v%s\n", VERSION);
    printf("=======================\n\n");

    if (argc > 1) {
        test_type = argv[1];
    }
    if (argc > 2) {
        duration = atoi(argv[2]);
    }
    if (argc > 3) {
        num_threads = atoi(argv[3]);
    }

    if (strcmp(test_type, "cpu") == 0) {
        run_cpu_stress(duration, num_threads);
    } else if (strcmp(test_type, "memory") == 0) {
        run_memory_stress(duration, num_threads);
    } else {
        printf("Unknown test type: %s\n", test_type);
        printf("Valid types: cpu, memory\n");
        return 1;
    }

    printf("\nStress test completed!\n");
    return 0;
}
