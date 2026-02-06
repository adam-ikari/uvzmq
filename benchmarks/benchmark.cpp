#include "../include/uvzmq.h"
#include <zmq.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <signal.h>
#include <atomic>

static std::atomic<bool> stop_flag(false);

static void signal_handler(int sig)
{
    (void)sig;
    stop_flag.store(true);
    printf("\n[INFO] Received signal, stopping...\n");
}

struct bench_data {
    int port;
    int msg_count;
    int msg_size;
    long long *result_us;
    std::atomic<int> *received_count;
};

static void *server_thread_func(void *arg)
{
    bench_data *data = (bench_data *)arg;
    
    printf("[SERVER] Starting server on port %d\n", data->port);
    
    uv_loop_t loop;
    uv_loop_init(&loop);
    
    void *zmq_ctx = zmq_ctx_new();
    void *zmq_sock = zmq_socket(zmq_ctx, ZMQ_REP);
    
    char bind_addr[64];
    snprintf(bind_addr, sizeof(bind_addr), "tcp://*:%d", data->port);
    zmq_bind(zmq_sock, bind_addr);
    
    data->received_count->store(0);
    
    // Optimize for large messages: increase TCP buffer sizes
    int rcvbuf = 1024 * 1024;  // 1MB receive buffer
    int sndbuf = 1024 * 1024;  // 1MB send buffer
    zmq_setsockopt(zmq_sock, ZMQ_RCVBUF, &rcvbuf, sizeof(rcvbuf));
    zmq_setsockopt(zmq_sock, ZMQ_SNDBUF, &sndbuf, sizeof(sndbuf));
    
    auto on_recv = [](uvzmq_socket_t *socket, zmq_msg_t *msg, void *user_data) {
        std::atomic<int> *count = (std::atomic<int> *)user_data;
        int send_rc = zmq_msg_send(msg, uvzmq_get_zmq_socket(socket), 0);
        if (send_rc < 0) {
            fprintf(stderr, "[ERROR] zmq_msg_send failed: %d (errno=%d)\n", send_rc, errno);
        }
        zmq_msg_close(msg);
        count->fetch_add(1);
    };
    
    uvzmq_socket_t *uvzmq_sock = NULL;
    uvzmq_socket_new(&loop, zmq_sock, on_recv, NULL, data->received_count, &uvzmq_sock);
    
    printf("[SERVER] Ready to receive messages\n");
    
    // Run event loop
    int iteration = 0;
    while (!stop_flag.load() && data->received_count->load() < data->msg_count) {
        uv_run(&loop, UV_RUN_ONCE);
        
        if (iteration++ % 100 == 0) {
            printf("[SERVER] Loop iteration %d, received: %d/%d\n", 
                   iteration, data->received_count->load(), data->msg_count);
        }
    }
    
    uvzmq_socket_free(uvzmq_sock);
    zmq_close(zmq_sock);
    zmq_ctx_term(zmq_ctx);
    uv_loop_close(&loop);
    
    return NULL;
}

static void *client_thread_func(void *arg)
{
    bench_data *data = (bench_data *)arg;
    
    printf("[CLIENT] Starting client, waiting for server...\n");
    usleep(200000); // Wait for server
    
    void *zmq_ctx = zmq_ctx_new();
    void *zmq_sock = zmq_socket(zmq_ctx, ZMQ_REQ);
    
    int timeout = 5000;
    zmq_setsockopt(zmq_sock, ZMQ_RCVTIMEO, &timeout, sizeof(timeout));
    
    // Optimize for large messages: increase TCP buffer sizes
    int rcvbuf = 1024 * 1024;  // 1MB receive buffer
    int sndbuf = 1024 * 1024;  // 1MB send buffer
    zmq_setsockopt(zmq_sock, ZMQ_RCVBUF, &rcvbuf, sizeof(rcvbuf));
    zmq_setsockopt(zmq_sock, ZMQ_SNDBUF, &sndbuf, sizeof(sndbuf));
    
    char connect_addr[64];
    snprintf(connect_addr, sizeof(connect_addr), "tcp://127.0.0.1:%d", data->port);
    zmq_connect(zmq_sock, connect_addr);
    
    char *msg = (char*)malloc(data->msg_size + 1);
    memset(msg, 'A', data->msg_size);
    msg[data->msg_size] = '\0';
    
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    for (int i = 0; i < data->msg_count && !stop_flag.load(); i++) {
        int send_rc = zmq_send(zmq_sock, msg, data->msg_size, 0);
        if (send_rc < 0) {
            fprintf(stderr, "[CLIENT ERROR] zmq_send failed at i=%d: %d (errno=%d)\n", i, send_rc, errno);
            break;
        }
        
        zmq_msg_t recv_msg;
        zmq_msg_init(&recv_msg);
        int recv_rc = zmq_msg_recv(&recv_msg, zmq_sock, 0);
        if (recv_rc < 0) {
            fprintf(stderr, "[CLIENT ERROR] zmq_msg_recv failed at i=%d: %d (errno=%d)\n", i, recv_rc, errno);
            zmq_msg_close(&recv_msg);
            break;
        }
        zmq_msg_close(&recv_msg);
        
        if (i % 100 == 0) {
            printf("[CLIENT] Progress: %d/%d\n", i, data->msg_count);
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

static void benchmark_req_rep(int msg_count, int msg_size)
{
    printf("=== REQ/REP Benchmark ===\n");
    printf("Message Count: %d\n", msg_count);
    printf("Message Size: %d bytes\n", msg_size);
    printf("Press Ctrl+C to stop\n\n");
    
    const int port = 5555;
    long long result_us = 0;
    std::atomic<int> received_count(0);
    
    bench_data data = {
        .port = port,
        .msg_count = msg_count,
        .msg_size = msg_size,
        .result_us = &result_us,
        .received_count = &received_count
    };
    
    pthread_t server_thread, client_thread;
    pthread_create(&server_thread, NULL, server_thread_func, &data);
    pthread_create(&client_thread, NULL, client_thread_func, &data);
    
    pthread_join(server_thread, NULL);
    pthread_join(client_thread, NULL);
    
    if (!stop_flag.load()) {
        printf("[BENCHMARK] REQ/REP Round-trip: %lld us\n", result_us);
        printf("Received: %d / %d messages\n", received_count.load(), msg_count);
    } else {
        printf("[INFO] Benchmark interrupted\n");
    }
    
    printf("\n");
    usleep(1000000); // Wait 1s for ZMQ sockets to close and threads to exit
}

static void *push_pull_server_thread_func(void *arg)
{
    bench_data *data = (bench_data *)arg;
    
    printf("[SERVER] Starting PUSH/PULL server on port %d\n", data->port);
    
    uv_loop_t loop;
    uv_loop_init(&loop);
    
    void *zmq_ctx = zmq_ctx_new();
    void *zmq_sock = zmq_socket(zmq_ctx, ZMQ_PULL);
    
    char bind_addr[64];
    snprintf(bind_addr, sizeof(bind_addr), "tcp://*:%d", data->port);
    zmq_bind(zmq_sock, bind_addr);
    
    data->received_count->store(0);
    
    // Optimize for large messages: increase TCP buffer sizes
    int rcvbuf = 1024 * 1024;  // 1MB receive buffer
    zmq_setsockopt(zmq_sock, ZMQ_RCVBUF, &rcvbuf, sizeof(rcvbuf));
    
    auto on_recv = [](uvzmq_socket_t *socket, zmq_msg_t *msg, void *user_data) {
        std::atomic<int> *count = (std::atomic<int> *)user_data;
        zmq_msg_close(msg);
        count->fetch_add(1);
    };
    
    uvzmq_socket_t *uvzmq_sock = NULL;
    uvzmq_socket_new(&loop, zmq_sock, on_recv, NULL, data->received_count, &uvzmq_sock);
    
// Run event loop
    int iteration = 0;
    while (!stop_flag.load() && data->received_count->load() < data->msg_count) {
        uv_run(&loop, UV_RUN_ONCE);
        
        if (iteration++ % 100 == 0) {
            printf("[SERVER] Loop iteration %d, received: %d/%d\n", 
                   iteration, data->received_count->load(), data->msg_count);
        }
        
        // Safety timeout to prevent infinite loop
        if (iteration > 10000) {
            printf("[SERVER] Timeout after %d iterations\n", iteration);
            break;
        }
    }
    
    uvzmq_socket_free(uvzmq_sock);
    zmq_close(zmq_sock);
    zmq_ctx_term(zmq_ctx);
    uv_loop_close(&loop);
    
    return NULL;
}

static void *push_pull_client_thread_func(void *arg)
{
    bench_data *data = (bench_data *)arg;
    
    printf("[CLIENT] Starting PUSH/PULL client, waiting for server...\n");
    usleep(200000); // Wait for server
    
    void *zmq_ctx = zmq_ctx_new();
    void *zmq_sock = zmq_socket(zmq_ctx, ZMQ_PUSH);
    
    // Set high water mark to handle fast sending
    int hwm = 10000;
    zmq_setsockopt(zmq_sock, ZMQ_SNDHWM, &hwm, sizeof(hwm));
    
    // Optimize for large messages: increase TCP buffer sizes
    int sndbuf = 1024 * 1024;  // 1MB send buffer
    zmq_setsockopt(zmq_sock, ZMQ_SNDBUF, &sndbuf, sizeof(sndbuf));
    
    char connect_addr[64];
    snprintf(connect_addr, sizeof(connect_addr), "tcp://127.0.0.1:%d", data->port);
    zmq_connect(zmq_sock, connect_addr);
    
    char *msg = (char*)malloc(data->msg_size + 1);
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

static void benchmark_push_pull(int msg_count, int msg_size)
{
    printf("=== PUSH/PULL Benchmark ===\n");
    printf("Message Count: %d\n", msg_count);
    printf("Message Size: %d bytes\n", msg_size);
    printf("Press Ctrl+C to stop\n\n");
    
    const int port = 5556;
    long long result_us = 0;
    std::atomic<int> received_count(0);
    
    bench_data data = {
        .port = port,
        .msg_count = msg_count,
        .msg_size = msg_size,
        .result_us = &result_us,
        .received_count = &received_count
    };
    
    pthread_t server_thread, client_thread;
    pthread_create(&server_thread, NULL, push_pull_server_thread_func, &data);
    pthread_create(&client_thread, NULL, push_pull_client_thread_func, &data);
    
    pthread_join(server_thread, NULL);
    pthread_join(client_thread, NULL);
    
    if (!stop_flag.load()) {
        printf("[BENCHMARK] PUSH/PULL Send: %lld us\n", result_us);
        printf("Received: %d / %d messages\n", received_count.load(), msg_count);
    } else {
        printf("[INFO] Benchmark interrupted\n");
    }
    
    printf("\n");
}

int main(void)
{
    printf("========================================\n");
    printf("UVZMQ Performance Benchmark\n");
    printf("(Press Ctrl+C to stop)\n");
    printf("========================================\n\n");
    
    // Setup signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    benchmark_req_rep(1000, 64);
    benchmark_req_rep(1000, 1024);
    benchmark_req_rep(100, 65536);
    
    printf("========================================\n");
    printf("Benchmark Complete\n");
    printf("========================================\n");
    
    return 0;
}