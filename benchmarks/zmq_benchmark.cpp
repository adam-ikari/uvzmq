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
    
    zmq_pollitem_t items[1];
    items[0].socket = zmq_sock;
    items[0].events = ZMQ_POLLIN;
    
    while (!stop_flag.load() && data->received_count->load() < data->msg_count) {
        zmq_poll(items, 1, 100); // 100ms timeout
        
        if (items[0].revents & ZMQ_POLLIN) {
            zmq_msg_t msg;
            zmq_msg_init(&msg);
            zmq_msg_recv(&msg, zmq_sock, 0);
            
            // Echo back
            zmq_msg_send(&msg, zmq_sock, 0);
            zmq_msg_close(&msg);
            
            data->received_count->fetch_add(1);
        }
    }
    
    zmq_close(zmq_sock);
    zmq_ctx_term(zmq_ctx);
    
    return NULL;
}

static void *client_thread_func(void *arg)
{
    bench_data *data = (bench_data *)arg;
    
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
        zmq_send(zmq_sock, msg, data->msg_size, 0);
        
        zmq_msg_t recv_msg;
        zmq_msg_init(&recv_msg);
        zmq_msg_recv(&recv_msg, zmq_sock, 0);
        zmq_msg_close(&recv_msg);
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
    printf("=== ZMQ REQ/REP Benchmark ===\n");
    printf("Message Count: %d\n", msg_count);
    printf("Message Size: %d bytes\n", msg_size);
    printf("Press Ctrl+C to stop\n\n");
    
    const int port = 5557;
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
        printf("[ZMQ BENCHMARK] REQ/REP Round-trip: %lld us\n", result_us);
        printf("Received: %d / %d messages\n", received_count.load(), msg_count);
    } else {
        printf("[INFO] Benchmark interrupted\n");
    }
    
    printf("\n");
}

static void *push_pull_server_thread_func(void *arg)
{
    bench_data *data = (bench_data *)arg;
    
    void *zmq_ctx = zmq_ctx_new();
    void *zmq_sock = zmq_socket(zmq_ctx, ZMQ_PULL);
    
    char bind_addr[64];
    snprintf(bind_addr, sizeof(bind_addr), "tcp://*:%d", data->port);
    zmq_bind(zmq_sock, bind_addr);
    
    data->received_count->store(0);
    
    // Optimize for large messages: increase TCP buffer sizes
    int rcvbuf = 1024 * 1024;  // 1MB receive buffer
    zmq_setsockopt(zmq_sock, ZMQ_RCVBUF, &rcvbuf, sizeof(rcvbuf));
    
    zmq_pollitem_t items[1];
    items[0].socket = zmq_sock;
    items[0].events = ZMQ_POLLIN;
    
    while (!stop_flag.load() && data->received_count->load() < data->msg_count) {
        zmq_poll(items, 1, 100); // 100ms timeout
        
        if (items[0].revents & ZMQ_POLLIN) {
            zmq_msg_t msg;
            zmq_msg_init(&msg);
            zmq_msg_recv(&msg, zmq_sock, 0);
            zmq_msg_close(&msg);
            
            data->received_count->fetch_add(1);
        }
    }
    
    zmq_close(zmq_sock);
    zmq_ctx_term(zmq_ctx);
    
    return NULL;
}

static void *push_pull_client_thread_func(void *arg)
{
    bench_data *data = (bench_data *)arg;
    
    usleep(200000); // Wait for server
    
    void *zmq_ctx = zmq_ctx_new();
    void *zmq_sock = zmq_socket(zmq_ctx, ZMQ_PUSH);
    
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
    printf("=== ZMQ PUSH/PULL Benchmark ===\n");
    printf("Message Count: %d\n", msg_count);
    printf("Message Size: %d bytes\n", msg_size);
    printf("Press Ctrl+C to stop\n\n");
    
    const int port = 5558;
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
        printf("[ZMQ BENCHMARK] PUSH/PULL Send: %lld us\n", result_us);
        printf("Received: %d / %d messages\n", received_count.load(), msg_count);
    } else {
        printf("[INFO] Benchmark interrupted\n");
    }
    
    printf("\n");
}

int main(void)
{
    printf("========================================\n");
    printf("Native ZMQ Performance Benchmark\n");
    printf("(Press Ctrl+C to stop)\n");
    printf("========================================\n\n");
    
    // Setup signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    benchmark_req_rep(1000, 64);
    benchmark_req_rep(1000, 1024);
    benchmark_req_rep(100, 65536);
    
    benchmark_push_pull(1000, 64);
    benchmark_push_pull(1000, 1024);
    benchmark_push_pull(100, 65536);
    
    printf("========================================\n");
    printf("Benchmark Complete\n");
    printf("========================================\n");
    
    return 0;
}