#define UVZMQ_IMPLEMENTATION
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <zmq.h>

#include "../include/uvzmq.h"

struct perf_data {
    int msg_count;
    int msg_size;
    long long total_time_us;
    int poll_callback_count;
    int messages_per_callback;
};

static struct perf_data g_perf_data;

static void on_recv(uvzmq_socket_t* socket, zmq_msg_t* msg, void* user_data) {
    struct perf_data* data = (struct perf_data*)user_data;
    data->messages_per_callback++;

    zmq_msg_send(msg, uvzmq_get_zmq_socket(socket), 0);
    zmq_msg_close(msg);
}

static void* server_thread(void* arg) {
    (void)arg;

    uv_loop_t loop;
    uv_loop_init(&loop);

    void* zmq_ctx = zmq_ctx_new();
    void* zmq_sock = zmq_socket(zmq_ctx, ZMQ_REP);

    // Optimize TCP settings
    int rcvbuf = 1024 * 1024;
    int sndbuf = 1024 * 1024;
    zmq_setsockopt(zmq_sock, ZMQ_RCVBUF, &rcvbuf, sizeof(rcvbuf));
    zmq_setsockopt(zmq_sock, ZMQ_SNDBUF, &sndbuf, sizeof(sndbuf));

    zmq_bind(zmq_sock, "tcp://*:5901");

    g_perf_data.poll_callback_count = 0;
    g_perf_data.messages_per_callback = 0;

    uvzmq_socket_t* uvzmq_sock = NULL;
    uvzmq_socket_new(&loop, zmq_sock, on_recv, &g_perf_data, &uvzmq_sock);

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    int received = 0;
    int callback_count = 0;
    while (received < g_perf_data.msg_count) {
        int prev_messages = g_perf_data.messages_per_callback;
        uv_run(&loop, UV_RUN_ONCE);

        if (g_perf_data.messages_per_callback > prev_messages) {
            callback_count++;
            received = g_perf_data.messages_per_callback;
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &end);
    g_perf_data.total_time_us = (end.tv_sec - start.tv_sec) * 1000000LL +
                                (end.tv_nsec - start.tv_nsec) / 1000LL;
    g_perf_data.poll_callback_count = callback_count;

    uvzmq_socket_free(uvzmq_sock);
    zmq_close(zmq_sock);
    zmq_ctx_term(zmq_ctx);
    uv_loop_close(&loop);

    return NULL;
}

static void* client_thread(void* arg) {
    (void)arg;
    usleep(100000);

    void* zmq_ctx = zmq_ctx_new();
    void* zmq_sock = zmq_socket(zmq_ctx, ZMQ_REQ);

    int timeout = 5000;
    zmq_setsockopt(zmq_sock, ZMQ_RCVTIMEO, &timeout, sizeof(timeout));

    int rcvbuf = 1024 * 1024;
    int sndbuf = 1024 * 1024;
    zmq_setsockopt(zmq_sock, ZMQ_RCVBUF, &rcvbuf, sizeof(rcvbuf));
    zmq_setsockopt(zmq_sock, ZMQ_SNDBUF, &sndbuf, sizeof(sndbuf));

    zmq_connect(zmq_sock, "tcp://127.0.0.1:5901");

    char* msg = malloc(g_perf_data.msg_size + 1);
    memset(msg, 'A', g_perf_data.msg_size);
    msg[g_perf_data.msg_size] = '\0';

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    for (int i = 0; i < g_perf_data.msg_count; i++) {
        zmq_send(zmq_sock, msg, g_perf_data.msg_size, 0);

        zmq_msg_t reply;
        zmq_msg_init(&reply);
        zmq_msg_recv(&reply, zmq_sock, 0);
        zmq_msg_close(&reply);
    }

    clock_gettime(CLOCK_MONOTONIC, &end);
    long long client_time = (end.tv_sec - start.tv_sec) * 1000000LL +
                            (end.tv_nsec - start.tv_nsec) / 1000LL;

    free(msg);
    zmq_close(zmq_sock);
    zmq_ctx_term(zmq_ctx);

    printf("[CLIENT] Total time: %lld us\n", client_time);

    return NULL;
}

int main(void) {
    printf("========================================\n");
    printf("UVZMQ Performance Analysis\n");
    printf("========================================\n\n");

    // Test different message sizes
    int msg_sizes[] = {64, 1024, 65536};
    int msg_counts[] = {1000, 1000, 100};

    for (int test = 0; test < 3; test++) {
        g_perf_data.msg_count = msg_counts[test];
        g_perf_data.msg_size = msg_sizes[test];
        g_perf_data.total_time_us = 0;
        g_perf_data.poll_callback_count = 0;
        g_perf_data.messages_per_callback = 0;

        printf("=== Test %d: %d bytes x %d messages ===\n",
               test + 1,
               g_perf_data.msg_size,
               g_perf_data.msg_count);

        pthread_t server, client;
        pthread_create(&server, NULL, server_thread, NULL);
        pthread_create(&client, NULL, client_thread, NULL);

        pthread_join(server, NULL);
        pthread_join(client, NULL);

        printf("[SERVER] Total time: %lld us\n", g_perf_data.total_time_us);
        printf("[SERVER] Poll callbacks: %d\n",
               g_perf_data.poll_callback_count);
        printf("[SERVER] Avg messages per callback: %.2f\n",
               (double)g_perf_data.msg_count / g_perf_data.poll_callback_count);
        printf("[SERVER] Time per message: %.2f us\n",
               (double)g_perf_data.total_time_us / g_perf_data.msg_count);
        printf("[SERVER] Time per callback: %.2f us\n",
               (double)g_perf_data.total_time_us /
                   g_perf_data.poll_callback_count);
        printf("\n");

        usleep(500000);  // Wait for ZMQ to clean up
    }

    return 0;
}