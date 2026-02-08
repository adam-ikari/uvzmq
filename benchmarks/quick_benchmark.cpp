#define UVZMQ_IMPLEMENTATION
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <zmq.h>

#include <atomic>

#include "../include/uvzmq.h"

// ============================================================================
// Configuration Constants
// ============================================================================

// Receive timeout in milliseconds for ZMQ sockets
// Prevents indefinite blocking if server is unresponsive
static const int ZMQ_RECV_TIMEOUT_MS = 5000;

// Send high water mark for PUSH sockets
// Controls how many messages can be queued before backpressure
static const int ZMQ_SEND_HWM = 10000;

// Maximum number of event loop iterations without progress before timeout
// Prevents infinite loops if no messages are received
// Increased to 1,000,000 to handle large message counts
static const int MAX_LOOP_ITERATIONS = 1000000;

// Delay between client start and first send (microseconds)
// Allows server time to initialize and bind to socket
static const int CLIENT_START_DELAY_US = 200000;

// ============================================================================
// Global State
// ============================================================================

static std::atomic<bool> stop_flag(false);

// ============================================================================
// Signal Handler
// ============================================================================

/**
 * Signal handler for graceful shutdown
 * @param sig Signal number (SIGINT or SIGTERM)
 */
static void signal_handler(int sig) {
    (void)sig;
    stop_flag.store(true);
    printf("\n[INFO] Received signal, stopping...\n");
}

// ============================================================================
// Benchmark Data Structure
// ============================================================================

/**
 * Benchmark data passed to server and client threads
 *
 * @note result_us is written only once after thread completion,
 *       so no synchronization is needed when reading after pthread_join()
 */
struct bench_data {
    const char* ipc_path;  // IPC socket path
    int msg_count;         // Number of messages to send/receive
    int msg_size;          // Size of each message in bytes
    long long* result_us;  // Pointer to store duration (microseconds)
    std::atomic<int>* received_count;  // Atomic counter for received messages
};

// ============================================================================
// UVZMQ REQ/REP Benchmark Functions
// ============================================================================

/**
 * REP server thread function
 * Receives messages and echoes them back using uvzmq event-driven I/O
 *
 * @param arg Pointer to bench_data structure
 * @return NULL (unused)
 */
static void* uvzmq_rep_server_thread_func(void* arg) {
    bench_data* data = (bench_data*)arg;

    printf("[UVZMQ REP SERVER] Starting on IPC: %s\n", data->ipc_path);

    uv_loop_t loop;
    uv_loop_init(&loop);

    void* zmq_ctx = zmq_ctx_new();
    void* zmq_sock = zmq_socket(zmq_ctx, ZMQ_REP);

    if (zmq_bind(zmq_sock, data->ipc_path) != 0) {
        fprintf(stderr, "[ERROR] Failed to bind to %s\n", data->ipc_path);
        zmq_close(zmq_sock);
        zmq_ctx_term(zmq_ctx);
        uv_loop_close(&loop);
        return NULL;
    }

    data->received_count->store(0);

    // Callback: echo received message back to sender
    auto on_recv = [](uvzmq_socket_t* socket, zmq_msg_t* msg, void* user_data) {
        std::atomic<int>* count = (std::atomic<int>*)user_data;
        zmq_msg_send(msg, uvzmq_get_zmq_socket(socket), 0);
        zmq_msg_close(msg);
        count->fetch_add(1);
    };

    uvzmq_socket_t* uvzmq_sock = NULL;
    if (uvzmq_socket_new(
            &loop, zmq_sock, on_recv, data->received_count, &uvzmq_sock) != 0) {
        fprintf(stderr, "[ERROR] Failed to create uvzmq socket\n");
        zmq_close(zmq_sock);
        zmq_ctx_term(zmq_ctx);
        uv_loop_close(&loop);
        return NULL;
    }

    // Run event loop with safety timeout
    int iteration = 0;
    while (!stop_flag.load() &&
           data->received_count->load() < data->msg_count) {
        uv_run(&loop, UV_RUN_ONCE);

        // Safety check: prevent infinite loop if no progress
        if (iteration++ > MAX_LOOP_ITERATIONS) {
            printf("[SERVER] Timeout after %d iterations (received: %d)\n",
                   iteration,
                   data->received_count->load());
            break;
        }
    }

    uvzmq_socket_free(uvzmq_sock);
    zmq_close(zmq_sock);
    zmq_ctx_term(zmq_ctx);
    uv_loop_close(&loop);

    return NULL;
}

/**
 * REQ client thread function
 * Sends messages and waits for responses
 *
 * @param arg Pointer to bench_data structure
 * @return NULL (unused)
 */
static void* uvzmq_req_client_thread_func(void* arg) {
    bench_data* data = (bench_data*)arg;

    printf("[UVZMQ REQ CLIENT] Starting\n");
    usleep(CLIENT_START_DELAY_US);

    void* zmq_ctx = zmq_ctx_new();
    void* zmq_sock = zmq_socket(zmq_ctx, ZMQ_REQ);

    // Set receive timeout to prevent indefinite blocking
    int timeout = ZMQ_RECV_TIMEOUT_MS;
    if (zmq_setsockopt(zmq_sock, ZMQ_RCVTIMEO, &timeout, sizeof(timeout)) !=
        0) {
        fprintf(stderr, "[ERROR] Failed to set ZMQ_RCVTIMEO\n");
    }

    if (zmq_connect(zmq_sock, data->ipc_path) != 0) {
        fprintf(stderr, "[ERROR] Failed to connect to %s\n", data->ipc_path);
        zmq_close(zmq_sock);
        zmq_ctx_term(zmq_ctx);
        return NULL;
    }

    // Allocate message buffer
    char* msg = (char*)malloc(data->msg_size + 1);
    if (!msg) {
        fprintf(stderr,
                "[ERROR] Failed to allocate %d bytes for message\n",
                data->msg_size + 1);
        zmq_close(zmq_sock);
        zmq_ctx_term(zmq_ctx);
        return NULL;
    }
    memset(msg, 'A', data->msg_size);
    msg[data->msg_size] = '\0';

    // Measure send/receive time
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    for (int i = 0; i < data->msg_count && !stop_flag.load(); i++) {
        int send_rc = zmq_send(zmq_sock, msg, data->msg_size, 0);
        if (send_rc < 0) {
            fprintf(stderr, "[ERROR] zmq_send failed at iteration %d\n", i);
            break;
        }

        zmq_msg_t recv_msg;
        zmq_msg_init(&recv_msg);
        int recv_rc = zmq_msg_recv(&recv_msg, zmq_sock, 0);
        if (recv_rc < 0) {
            fprintf(stderr, "[ERROR] zmq_msg_recv failed at iteration %d\n", i);
            zmq_msg_close(&recv_msg);
            break;
        }
        zmq_msg_close(&recv_msg);
    }

    clock_gettime(CLOCK_MONOTONIC, &end);

    // Calculate duration in microseconds
    long long duration_us = (end.tv_sec - start.tv_sec) * 1000000LL +
                            (end.tv_nsec - start.tv_nsec) / 1000LL;

    *(data->result_us) = duration_us;

    free(msg);
    zmq_close(zmq_sock);
    zmq_ctx_term(zmq_ctx);

    return NULL;
}

/**
 * Run REQ/REP benchmark (round-trip latency test)
 *
 * @param name Benchmark name
 * @param msg_count Number of messages to send
 * @param msg_size Size of each message in bytes
 */
static void benchmark_uvzmq_req_rep(const char* name,
                                    int msg_count,
                                    int msg_size) {
    printf("\n");
    printf("========================================\n");
    printf("UVZMQ IPC REQ/REP: %s\n", name);
    printf("========================================\n");

    const char* ipc_path = "ipc:///tmp/uvzmq-benchmark-req-rep";
    long long result_us = 0;
    std::atomic<int> received_count(0);

    bench_data data = {.ipc_path = ipc_path,
                       .msg_count = msg_count,
                       .msg_size = msg_size,
                       .result_us = &result_us,
                       .received_count = &received_count};

    pthread_t server_thread, client_thread;
    pthread_create(&server_thread, NULL, uvzmq_rep_server_thread_func, &data);
    pthread_create(&client_thread, NULL, uvzmq_req_client_thread_func, &data);

    pthread_join(server_thread, NULL);
    pthread_join(client_thread, NULL);

    if (!stop_flag.load() && received_count.load() > 0) {
        printf("\n[RESULTS]\n");
        printf("  Total Time: %.3f seconds\n", result_us / 1000000.0);
        printf("  Messages: %d / %d\n", received_count.load(), msg_count);
        printf("  Throughput: %.2f msg/sec\n",
               (double)received_count.load() / (result_us / 1000000.0));
        printf("  Avg Latency: %.3f ms\n",
               (double)result_us / received_count.load() / 1000.0);
    } else {
        printf("\n[INFO] Benchmark interrupted or failed\n");
    }

    printf("\n");
    usleep(500000);
}

// ============================================================================
// UVZMQ PUSH/PULL Benchmark Functions
// ============================================================================

/**
 * PULL server thread function
 * Receives messages using uvzmq event-driven I/O (one-way)
 *
 * @param arg Pointer to bench_data structure
 * @return NULL (unused)
 */
static void* uvzmq_pull_server_thread_func(void* arg) {
    bench_data* data = (bench_data*)arg;

    printf("[UVZMQ PULL SERVER] Starting on IPC: %s\n", data->ipc_path);

    uv_loop_t loop;
    uv_loop_init(&loop);

    void* zmq_ctx = zmq_ctx_new();
    void* zmq_sock = zmq_socket(zmq_ctx, ZMQ_PULL);

    if (zmq_bind(zmq_sock, data->ipc_path) != 0) {
        fprintf(stderr, "[ERROR] Failed to bind to %s\n", data->ipc_path);
        zmq_close(zmq_sock);
        zmq_ctx_term(zmq_ctx);
        uv_loop_close(&loop);
        return NULL;
    }

    data->received_count->store(0);

    // Callback: count received messages
    auto on_recv = [](uvzmq_socket_t* socket, zmq_msg_t* msg, void* user_data) {
        (void)socket;
        std::atomic<int>* count = (std::atomic<int>*)user_data;
        zmq_msg_close(msg);
        count->fetch_add(1);
    };

    uvzmq_socket_t* uvzmq_sock = NULL;
    if (uvzmq_socket_new(
            &loop, zmq_sock, on_recv, data->received_count, &uvzmq_sock) != 0) {
        fprintf(stderr, "[ERROR] Failed to create uvzmq socket\n");
        zmq_close(zmq_sock);
        zmq_ctx_term(zmq_ctx);
        uv_loop_close(&loop);
        return NULL;
    }

    // Run event loop with safety timeout
    int iteration = 0;
    while (!stop_flag.load() &&
           data->received_count->load() < data->msg_count) {
        uv_run(&loop, UV_RUN_ONCE);

        // Safety check: prevent infinite loop if no progress
        if (iteration++ > MAX_LOOP_ITERATIONS) {
            printf("[SERVER] Timeout after %d iterations (received: %d)\n",
                   iteration,
                   data->received_count->load());
            break;
        }
    }

    uvzmq_socket_free(uvzmq_sock);
    zmq_close(zmq_sock);
    zmq_ctx_term(zmq_ctx);
    uv_loop_close(&loop);

    return NULL;
}

/**
 * PUSH client thread function
 * Sends messages without waiting for response (one-way)
 *
 * @param arg Pointer to bench_data structure
 * @return NULL (unused)
 */
static void* uvzmq_push_client_thread_func(void* arg) {
    bench_data* data = (bench_data*)arg;

    printf("[UVZMQ PUSH CLIENT] Starting\n");
    usleep(CLIENT_START_DELAY_US);

    void* zmq_ctx = zmq_ctx_new();
    void* zmq_sock = zmq_socket(zmq_ctx, ZMQ_PUSH);

    // Set high water mark to control queuing
    int hwm = ZMQ_SEND_HWM;
    if (zmq_setsockopt(zmq_sock, ZMQ_SNDHWM, &hwm, sizeof(hwm)) != 0) {
        fprintf(stderr, "[ERROR] Failed to set ZMQ_SNDHWM\n");
    }

    if (zmq_connect(zmq_sock, data->ipc_path) != 0) {
        fprintf(stderr, "[ERROR] Failed to connect to %s\n", data->ipc_path);
        zmq_close(zmq_sock);
        zmq_ctx_term(zmq_ctx);
        return NULL;
    }

    // Allocate message buffer
    char* msg = (char*)malloc(data->msg_size + 1);
    if (!msg) {
        fprintf(stderr,
                "[ERROR] Failed to allocate %d bytes for message\n",
                data->msg_size + 1);
        zmq_close(zmq_sock);
        zmq_ctx_term(zmq_ctx);
        return NULL;
    }
    memset(msg, 'A', data->msg_size);
    msg[data->msg_size] = '\0';

    // Measure send time
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    for (int i = 0; i < data->msg_count && !stop_flag.load(); i++) {
        int send_rc = zmq_send(zmq_sock, msg, data->msg_size, 0);
        if (send_rc < 0) {
            fprintf(stderr, "[ERROR] zmq_send failed at iteration %d\n", i);
            break;
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &end);

    // Calculate duration in microseconds
    long long duration_us = (end.tv_sec - start.tv_sec) * 1000000LL +
                            (end.tv_nsec - start.tv_nsec) / 1000LL;

    *(data->result_us) = duration_us;

    free(msg);
    zmq_close(zmq_sock);
    zmq_ctx_term(zmq_ctx);

    return NULL;
}

/**
 * Run PUSH/PULL benchmark (one-way throughput test)
 *
 * @param name Benchmark name
 * @param msg_count Number of messages to send
 * @param msg_size Size of each message in bytes
 */
static void benchmark_uvzmq_push_pull(const char* name,
                                      int msg_count,
                                      int msg_size) {
    printf("\n");
    printf("========================================\n");
    printf("UVZMQ IPC PUSH/PULL: %s\n", name);
    printf("========================================\n");

    const char* ipc_path = "ipc:///tmp/uvzmq-benchmark-push-pull";
    long long result_us = 0;
    std::atomic<int> received_count(0);

    bench_data data = {.ipc_path = ipc_path,
                       .msg_count = msg_count,
                       .msg_size = msg_size,
                       .result_us = &result_us,
                       .received_count = &received_count};

    pthread_t server_thread, client_thread;
    pthread_create(&server_thread, NULL, uvzmq_pull_server_thread_func, &data);
    pthread_create(&client_thread, NULL, uvzmq_push_client_thread_func, &data);

    pthread_join(server_thread, NULL);
    pthread_join(client_thread, NULL);

    if (!stop_flag.load() && received_count.load() > 0) {
        printf("\n[RESULTS]\n");
        printf("  Total Time: %.3f seconds\n", result_us / 1000000.0);
        printf("  Messages: %d / %d\n", received_count.load(), msg_count);
        printf("  Send Throughput: %.2f msg/sec\n",
               (double)msg_count / (result_us / 1000000.0));
    } else {
        printf("\n[INFO] Benchmark interrupted or failed\n");
    }

    printf("\n");
    usleep(500000);
}

// ============================================================================
// Main Function
// ============================================================================

/**
 * Main entry point for UVZMQ performance benchmark suite
 *
 * Benchmarks run in this order:
 * 1. REQ/REP (round-trip) - measures latency and request/response throughput
 * 2. PUSH/PULL (one-way) - measures maximum send throughput
 *
 * Press Ctrl+C to gracefully stop all benchmarks.
 */
int main(void) {
    printf("========================================\n");
    printf("UVZMQ Performance Benchmark Suite\n");
    printf("========================================\n");

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // REQ/REP (round-trip) benchmarks
    if (!stop_flag.load()) {
        benchmark_uvzmq_req_rep("Small Messages (64B)", 10000, 64);
    }
    if (!stop_flag.load()) {
        benchmark_uvzmq_req_rep("Medium Messages (1KB)", 5000, 1024);
    }

    // PUSH/PULL (one-way) benchmarks - faster than REQ/REP
    if (!stop_flag.load()) {
        benchmark_uvzmq_push_pull("Small Messages (64B)", 100000, 64);
    }
    if (!stop_flag.load()) {
        benchmark_uvzmq_push_pull("Medium Messages (1KB)", 50000, 1024);
    }
    if (!stop_flag.load()) {
        benchmark_uvzmq_push_pull("Large Messages (64KB)", 10000, 65536);
    }

    printf("\n========================================\n");
    printf("Benchmark Complete!\n");
    printf("========================================\n");

    return 0;
}