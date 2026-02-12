/**
 * @file test_io_threads_perf.c
 * @brief Performance comparison between ZMQ_IO_THREADS=0 and ZMQ_IO_THREADS=1
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <zmq.h>
#include <uv.h>
#include "uvzmq.h"

#define TEST_ITERATIONS 1000

typedef struct {
    int io_threads;
    double create_time;
    double close_time;
    double total_time;
} perf_result_t;

void on_recv(uvzmq_socket_t* socket, zmq_msg_t* msg, void* user_data) {
    (void)socket;
    (void)user_data;
    zmq_msg_close(msg);
}

double get_time_ms() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0;
}

perf_result_t test_perf(int io_threads) {
    perf_result_t result;
    result.io_threads = io_threads;

    printf("\nTesting with ZMQ_IO_THREADS=%d\n", io_threads);
    printf("--------------------------------\n");

    double start = get_time_ms();

    uv_loop_t loop;
    uv_loop_init(&loop);

    void* zmq_ctx = zmq_ctx_new();
    zmq_ctx_set(zmq_ctx, ZMQ_IO_THREADS, io_threads);

    double create_start = get_time_ms();

    // Create sockets
    void* zmq_socks[TEST_ITERATIONS];
    uvzmq_socket_t* uvzmq_socks[TEST_ITERATIONS];

    for (int i = 0; i < TEST_ITERATIONS; i++) {
        zmq_socks[i] = zmq_socket(zmq_ctx, ZMQ_REP);
        if (!zmq_socks[i]) {
            fprintf(stderr, "Failed to create socket %d\n", i);
            exit(1);
        }

        if (uvzmq_socket_new(&loop, zmq_socks[i], on_recv, NULL, &uvzmq_socks[i]) != 0) {
            fprintf(stderr, "Failed to create uvzmq socket %d\n", i);
            exit(1);
        }
    }

    double create_end = get_time_ms();
    result.create_time = create_end - create_start;

    printf("Created %d sockets in %.2f ms\n", TEST_ITERATIONS, result.create_time);

    // Small delay to allow initialization
    uv_run(&loop, UV_RUN_NOWAIT);

    double close_start = get_time_ms();

    // Close sockets
    for (int i = 0; i < TEST_ITERATIONS; i++) {
        uvzmq_socket_free(uvzmq_socks[i]);
        zmq_close(zmq_socks[i]);
    }

    double close_end = get_time_ms();
    result.close_time = close_end - close_start;

    printf("Closed %d sockets in %.2f ms\n", TEST_ITERATIONS, result.close_time);

    uv_run(&loop, UV_RUN_NOWAIT);
    zmq_ctx_term(zmq_ctx);
    uv_loop_close(&loop);

    double end = get_time_ms();
    result.total_time = end - start;

    printf("Total time: %.2f ms\n", result.total_time);

    return result;
}

int main() {
    printf("=== ZMQ_IO_THREADS Performance Comparison ===\n");
    printf("Iterations: %d\n\n", TEST_ITERATIONS);

    perf_result_t result0 = test_perf(0);
    perf_result_t result1 = test_perf(1);

    printf("\n=== Performance Comparison ===\n");
    printf("%-20s %12s %12s %12s\n", "Configuration", "Create (ms)", "Close (ms)", "Total (ms)");
    printf("%-20s %12.2f %12.2f %12.2f\n", "ZMQ_IO_THREADS=0", result0.create_time, result0.close_time, result0.total_time);
    printf("%-20s %12.2f %12.2f %12.2f\n", "ZMQ_IO_THREADS=1", result1.create_time, result1.close_time, result1.total_time);

    printf("\n=== Performance Difference ===\n");
    printf("Create: %.2fx %s\n", result0.create_time / result1.create_time, result0.create_time < result1.create_time ? "faster" : "slower");
    printf("Close:  %.2fx %s\n", result0.close_time / result1.close_time, result0.close_time < result1.close_time ? "faster" : "slower");
    printf("Total:  %.2fx %s\n", result0.total_time / result1.total_time, result0.total_time < result1.total_time ? "faster" : "slower");

    return 0;
}