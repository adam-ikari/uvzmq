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

static std::atomic<bool> stop_flag(false);

static void signal_handler(int sig) {
    (void)sig;
    stop_flag.store(true);
    printf("\n[INFO] Received signal, stopping...\n");
}

struct bench_data {
    int port;
    int msg_count;
    int msg_size;
    long long* result_us;
    std::atomic<int>* received_count;
    const char* mode;
};

// ============================================================================
// UVZMQ Benchmarks
// ============================================================================

static void* uvzmq_server_thread_func(void* arg) {
    bench_data* data = (bench_data*)arg;

    printf("[UVZMQ SERVER] Starting %s server on port %d\n",
           data->mode,
           data->port);

    uv_loop_t loop;
    uv_loop_init(&loop);

    void* zmq_ctx = zmq_ctx_new();
    void* zmq_sock = zmq_socket(zmq_ctx, ZMQ_REP);

    char bind_addr[64];
    snprintf(bind_addr, sizeof(bind_addr), "tcp://*:%d", data->port);
    zmq_bind(zmq_sock, bind_addr);

    data->received_count->store(0);

    // Optimize TCP buffer sizes
    int rcvbuf = 1024 * 1024;
    int sndbuf = 1024 * 1024;
    zmq_setsockopt(zmq_sock, ZMQ_RCVBUF, &rcvbuf, sizeof(rcvbuf));
    zmq_setsockopt(zmq_sock, ZMQ_SNDBUF, &sndbuf, sizeof(sndbuf));

    auto on_recv = [](uvzmq_socket_t* socket, zmq_msg_t* msg, void* user_data) {
        std::atomic<int>* count = (std::atomic<int>*)user_data;
        int send_rc = zmq_msg_send(msg, uvzmq_get_zmq_socket(socket), 0);
        if (send_rc < 0) {
            fprintf(stderr, "[ERROR] zmq_msg_send failed: %d\n", send_rc);
        }
        zmq_msg_close(msg);
        count->fetch_add(1);
    };

    uvzmq_socket_t* uvzmq_sock = NULL;
    uvzmq_socket_new(
        &loop, zmq_sock, on_recv, data->received_count, &uvzmq_sock);

    printf("[UVZMQ SERVER] Ready to receive messages\n");

    while (!stop_flag.load() &&
           data->received_count->load() < data->msg_count) {
        uv_run(&loop, UV_RUN_ONCE);
    }

    uvzmq_socket_free(uvzmq_sock);
    zmq_close(zmq_sock);
    zmq_ctx_term(zmq_ctx);
    uv_loop_close(&loop);

    return NULL;
}

static void* uvzmq_client_thread_func(void* arg) {
    bench_data* data = (bench_data*)arg;

    printf("[UVZMQ CLIENT] Starting %s client\n", data->mode);
    usleep(200000);

    void* zmq_ctx = zmq_ctx_new();
    void* zmq_sock = zmq_socket(zmq_ctx, ZMQ_REQ);

    int timeout = 5000;
    zmq_setsockopt(zmq_sock, ZMQ_RCVTIMEO, &timeout, sizeof(timeout));

    int rcvbuf = 1024 * 1024;
    int sndbuf = 1024 * 1024;
    zmq_setsockopt(zmq_sock, ZMQ_RCVBUF, &rcvbuf, sizeof(rcvbuf));
    zmq_setsockopt(zmq_sock, ZMQ_SNDBUF, &sndbuf, sizeof(sndbuf));

    char connect_addr[64];
    snprintf(
        connect_addr, sizeof(connect_addr), "tcp://127.0.0.1:%d", data->port);
    zmq_connect(zmq_sock, connect_addr);

    char* msg = (char*)malloc(data->msg_size + 1);
    memset(msg, 'A', data->msg_size);
    msg[data->msg_size] = '\0';

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    for (int i = 0; i < data->msg_count && !stop_flag.load(); i++) {
        int send_rc = zmq_send(zmq_sock, msg, data->msg_size, 0);
        if (send_rc < 0) {
            fprintf(stderr, "[ERROR] zmq_send failed at i=%d\n", i);
            break;
        }

        zmq_msg_t recv_msg;
        zmq_msg_init(&recv_msg);
        int recv_rc = zmq_msg_recv(&recv_msg, zmq_sock, 0);
        if (recv_rc < 0) {
            fprintf(stderr, "[ERROR] zmq_msg_recv failed at i=%d\n", i);
            zmq_msg_close(&recv_msg);
            break;
        }
        zmq_msg_close(&recv_msg);

        if (i % 10000 == 0) {
            printf("[UVZMQ CLIENT] Progress: %d/%d\n", i, data->msg_count);
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &end);

    long long duration_us = (end.tv_sec - start.tv_sec) * 1000000LL +
                            (end.tv_nsec - start.tv_nsec) / 1000LL;

    *(data->result_us) = duration_us;

    free(msg);
    zmq_close(zmq_sock);
    zmq_ctx_term(zmq_ctx);

    return NULL;
}

static void benchmark_uvzmq(const char* name, int msg_count, int msg_size) {
    printf("\n");
    printf("========================================\n");
    printf("UVZMQ %s Benchmark\n", name);
    printf("========================================\n");
    printf("Message Count: %d\n", msg_count);
    printf("Message Size: %d bytes\n", msg_size);
    printf("Press Ctrl+C to stop\n\n");

    const int port = 5555;
    long long result_us = 0;
    std::atomic<int> received_count(0);

    bench_data data = {.port = port,
                       .msg_count = msg_count,
                       .msg_size = msg_size,
                       .result_us = &result_us,
                       .received_count = &received_count,
                       .mode = name};

    pthread_t server_thread, client_thread;
    pthread_create(&server_thread, NULL, uvzmq_server_thread_func, &data);
    pthread_create(&client_thread, NULL, uvzmq_client_thread_func, &data);

    pthread_join(server_thread, NULL);
    pthread_join(client_thread, NULL);

    if (!stop_flag.load()) {
        printf("\n");
        printf("[RESULTS] UVZMQ %s\n", name);
        printf("  Total Time: %.3f seconds\n", result_us / 1000000.0);
        printf(
            "  Messages Received: %d / %d\n", received_count.load(), msg_count);
        printf("  Throughput: %.2f messages/second\n",
               (double)received_count.load() / (result_us / 1000000.0));
        printf("  Avg Latency: %.3f ms\n",
               (double)result_us / received_count.load() / 1000.0);
    } else {
        printf("\n[INFO] Benchmark interrupted\n");
    }

    printf("\n");
    usleep(1000000);
}

// ============================================================================
// Pure ZMQ Benchmark (for comparison)
// ============================================================================

static void* pure_zmq_server_thread_func(void* arg) {
    bench_data* data = (bench_data*)arg;

    printf("[PURE ZMQ SERVER] Starting server on port %d\n", data->port);

    void* zmq_ctx = zmq_ctx_new();
    void* zmq_sock = zmq_socket(zmq_ctx, ZMQ_REP);

    char bind_addr[64];
    snprintf(bind_addr, sizeof(bind_addr), "tcp://*:%d", data->port);
    zmq_bind(zmq_sock, bind_addr);

    data->received_count->store(0);

    int rcvbuf = 1024 * 1024;
    int sndbuf = 1024 * 1024;
    zmq_setsockopt(zmq_sock, ZMQ_RCVBUF, &rcvbuf, sizeof(rcvbuf));
    zmq_setsockopt(zmq_sock, ZMQ_SNDBUF, &sndbuf, sizeof(sndbuf));

    while (!stop_flag.load() &&
           data->received_count->load() < data->msg_count) {
        zmq_msg_t msg;
        zmq_msg_init(&msg);
        zmq_msg_recv(&msg, zmq_sock, 0);
        zmq_msg_send(&msg, zmq_sock, 0);
        zmq_msg_close(&msg);
        data->received_count->fetch_add(1);
    }

    zmq_close(zmq_sock);
    zmq_ctx_term(zmq_ctx);

    return NULL;
}

static void* pure_zmq_client_thread_func(void* arg) {
    bench_data* data = (bench_data*)arg;

    printf("[PURE ZMQ CLIENT] Starting client\n");
    usleep(200000);

    void* zmq_ctx = zmq_ctx_new();
    void* zmq_sock = zmq_socket(zmq_ctx, ZMQ_REQ);

    int timeout = 5000;
    zmq_setsockopt(zmq_sock, ZMQ_RCVTIMEO, &timeout, sizeof(timeout));

    int rcvbuf = 1024 * 1024;
    int sndbuf = 1024 * 1024;
    zmq_setsockopt(zmq_sock, ZMQ_RCVBUF, &rcvbuf, sizeof(rcvbuf));
    zmq_setsockopt(zmq_sock, ZMQ_SNDBUF, &sndbuf, sizeof(sndbuf));

    char connect_addr[64];
    snprintf(
        connect_addr, sizeof(connect_addr), "tcp://127.0.0.1:%d", data->port);
    zmq_connect(zmq_sock, connect_addr);

    char* msg = (char*)malloc(data->msg_size + 1);
    memset(msg, 'A', data->msg_size);
    msg[data->msg_size] = '\0';

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    for (int i = 0; i < data->msg_count && !stop_flag.load(); i++) {
        int send_rc = zmq_send(zmq_sock, msg, data->msg_size, 0);
        if (send_rc < 0) {
            fprintf(stderr, "[ERROR] zmq_send failed at i=%d\n", i);
            break;
        }

        zmq_msg_t recv_msg;
        zmq_msg_init(&recv_msg);
        int recv_rc = zmq_msg_recv(&recv_msg, zmq_sock, 0);
        if (recv_rc < 0) {
            fprintf(stderr, "[ERROR] zmq_msg_recv failed at i=%d\n", i);
            zmq_msg_close(&recv_msg);
            break;
        }
        zmq_msg_close(&recv_msg);

        if (i % 10000 == 0) {
            printf("[PURE ZMQ CLIENT] Progress: %d/%d\n", i, data->msg_count);
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &end);

    long long duration_us = (end.tv_sec - start.tv_sec) * 1000000LL +
                            (end.tv_nsec - start.tv_nsec) / 1000LL;

    *(data->result_us) = duration_us;

    free(msg);
    zmq_close(zmq_sock);
    zmq_ctx_term(zmq_ctx);

    return NULL;
}

static void benchmark_pure_zmq(const char* name, int msg_count, int msg_size) {
    printf("\n");
    printf("========================================\n");
    printf("Pure ZMQ %s Benchmark\n", name);
    printf("========================================\n");
    printf("Message Count: %d\n", msg_count);
    printf("Message Size: %d bytes\n", msg_size);
    printf("Press Ctrl+C to stop\n\n");

    const int port = 5556;
    long long result_us = 0;
    std::atomic<int> received_count(0);

    bench_data data = {.port = port,
                       .msg_count = msg_count,
                       .msg_size = msg_size,
                       .result_us = &result_us,
                       .received_count = &received_count,
                       .mode = name};

    pthread_t server_thread, client_thread;
    pthread_create(&server_thread, NULL, pure_zmq_server_thread_func, &data);
    pthread_create(&client_thread, NULL, pure_zmq_client_thread_func, &data);

    pthread_join(server_thread, NULL);
    pthread_join(client_thread, NULL);

    if (!stop_flag.load()) {
        printf("\n");
        printf("[RESULTS] Pure ZMQ %s\n", name);
        printf("  Total Time: %.3f seconds\n", result_us / 1000000.0);
        printf(
            "  Messages Received: %d / %d\n", received_count.load(), msg_count);
        printf("  Throughput: %.2f messages/second\n",
               (double)received_count.load() / (result_us / 1000000.0));
        printf("  Avg Latency: %.3f ms\n",
               (double)result_us / received_count.load() / 1000.0);
    } else {
        printf("\n[INFO] Benchmark interrupted\n");
    }

    printf("\n");
    usleep(1000000);
}

// ============================================================================
// PUSH/PULL Throughput Benchmark
// ============================================================================

static void* push_pull_server_thread_func(void* arg) {
    bench_data* data = (bench_data*)arg;

    printf("[PULL SERVER] Starting PULL server on port %d\n", data->port);

    uv_loop_t loop;
    uv_loop_init(&loop);

    void* zmq_ctx = zmq_ctx_new();
    void* zmq_sock = zmq_socket(zmq_ctx, ZMQ_PULL);

    char bind_addr[64];
    snprintf(bind_addr, sizeof(bind_addr), "tcp://*:%d", data->port);
    zmq_bind(zmq_sock, bind_addr);

    data->received_count->store(0);

    int rcvbuf = 1024 * 1024;
    zmq_setsockopt(zmq_sock, ZMQ_RCVBUF, &rcvbuf, sizeof(rcvbuf));

    auto on_recv = [](uvzmq_socket_t* socket, zmq_msg_t* msg, void* user_data) {
        std::atomic<int>* count = (std::atomic<int>*)user_data;
        zmq_msg_close(msg);
        count->fetch_add(1);
    };

    uvzmq_socket_t* uvzmq_sock = NULL;
    uvzmq_socket_new(
        &loop, zmq_sock, on_recv, data->received_count, &uvzmq_sock);

    while (!stop_flag.load() &&
           data->received_count->load() < data->msg_count) {
        uv_run(&loop, UV_RUN_ONCE);
    }

    uvzmq_socket_free(uvzmq_sock);
    zmq_close(zmq_sock);
    zmq_ctx_term(zmq_ctx);
    uv_loop_close(&loop);

    return NULL;
}

static void* push_pull_client_thread_func(void* arg) {
    bench_data* data = (bench_data*)arg;

    printf("[PUSH CLIENT] Starting PUSH client\n");
    usleep(200000);

    void* zmq_ctx = zmq_ctx_new();
    void* zmq_sock = zmq_socket(zmq_ctx, ZMQ_PUSH);

    int hwm = 10000;
    zmq_setsockopt(zmq_sock, ZMQ_SNDHWM, &hwm, sizeof(hwm));

    int sndbuf = 1024 * 1024;
    zmq_setsockopt(zmq_sock, ZMQ_SNDBUF, &sndbuf, sizeof(sndbuf));

    char connect_addr[64];
    snprintf(
        connect_addr, sizeof(connect_addr), "tcp://127.0.0.1:%d", data->port);
    zmq_connect(zmq_sock, connect_addr);

    char* msg = (char*)malloc(data->msg_size + 1);
    memset(msg, 'A', data->msg_size);
    msg[data->msg_size] = '\0';

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    for (int i = 0; i < data->msg_count && !stop_flag.load(); i++) {
        zmq_send(zmq_sock, msg, data->msg_size, 0);
    }

    clock_gettime(CLOCK_MONOTONIC, &end);

    long long duration_us = (end.tv_sec - start.tv_sec) * 1000000LL +
                            (end.tv_nsec - start.tv_nsec) / 1000LL;

    *(data->result_us) = duration_us;

    free(msg);
    zmq_close(zmq_sock);
    zmq_ctx_term(zmq_ctx);

    return NULL;
}

static void benchmark_push_pull(const char* name, int msg_count, int msg_size) {
    printf("\n");
    printf("========================================\n");
    printf("PUSH/PULL %s Benchmark\n", name);
    printf("========================================\n");
    printf("Message Count: %d\n", msg_count);
    printf("Message Size: %d bytes\n", msg_size);
    printf("Press Ctrl+C to stop\n\n");

    const int port = 5557;
    long long result_us = 0;
    std::atomic<int> received_count(0);

    bench_data data = {.port = port,
                       .msg_count = msg_count,
                       .msg_size = msg_size,
                       .result_us = &result_us,
                       .received_count = &received_count,
                       .mode = name};

    pthread_t server_thread, client_thread;
    pthread_create(&server_thread, NULL, push_pull_server_thread_func, &data);
    pthread_create(&client_thread, NULL, push_pull_client_thread_func, &data);

    pthread_join(server_thread, NULL);
    pthread_join(client_thread, NULL);

    if (!stop_flag.load()) {
        printf("\n");
        printf("[RESULTS] PUSH/PULL %s\n", name);
        printf("  Total Time: %.3f seconds\n", result_us / 1000000.0);
        printf(
            "  Messages Received: %d / %d\n", received_count.load(), msg_count);
        printf("  Send Throughput: %.2f messages/second\n",
               (double)msg_count / (result_us / 1000000.0));
    } else {
        printf("\n[INFO] Benchmark interrupted\n");
    }

    printf("\n");
    usleep(1000000);
}

// ============================================================================
// Main
// ============================================================================

int main(void) {
    printf("========================================\n");
    printf("UVZMQ Performance Benchmark Suite\n");
    printf("(Press Ctrl+C to stop)\n");
    printf("========================================\n");

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // UVZMQ Benchmarks
    benchmark_uvzmq("Small Messages (64B)", 100000, 64);
    if (!stop_flag.load()) {
        benchmark_uvzmq("Medium Messages (1KB)", 50000, 1024);
    }
    if (!stop_flag.load()) {
        benchmark_uvzmq("Large Messages (64KB)", 10000, 65536);
    }

    // Pure ZMQ Benchmarks (for comparison)
    if (!stop_flag.load()) {
        benchmark_pure_zmq("Small Messages (64B)", 100000, 64);
    }
    if (!stop_flag.load()) {
        benchmark_pure_zmq("Medium Messages (1KB)", 50000, 1024);
    }

    // PUSH/PULL Throughput Benchmarks
    if (!stop_flag.load()) {
        benchmark_push_pull("Small Messages (64B)", 100000, 64);
    }
    if (!stop_flag.load()) {
        benchmark_push_pull("Medium Messages (1KB)", 50000, 1024);
    }

    printf("\n");
    printf("========================================\n");
    printf("Benchmark Suite Complete\n");
    printf("========================================\n");

    return 0;
}
