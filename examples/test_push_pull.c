#define UVZMQ_IMPLEMENTATION
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <zmq.h>

#include "../include/uvzmq.h"

struct test_data {
    int port;
    int received_count;
};

static void server_on_recv(uvzmq_socket_t* socket,
                           zmq_msg_t* msg,
                           void* user_data) {
    (void)socket;
    int* count = (int*)user_data;
    size_t size = zmq_msg_size(msg);
    const char* content = (const char*)zmq_msg_data(msg);
    printf("[SERVER] Received message %d: %.*s\n", *count, (int)size, content);
    zmq_msg_close(msg);
    (*count)++;
}

static void* server_thread_func(void* arg) {
    struct test_data* data = (struct test_data*)arg;

    printf("[SERVER] Starting PULL server on port %d\n", data->port);

    uv_loop_t loop;
    uv_loop_init(&loop);

    void* zmq_ctx = zmq_ctx_new();
    void* zmq_sock = zmq_socket(zmq_ctx, ZMQ_PULL);

    char bind_addr[64];
    snprintf(bind_addr, sizeof(bind_addr), "tcp://*:%d", data->port);
    zmq_bind(zmq_sock, bind_addr);

    data->received_count = 0;

    uvzmq_socket_t* uvzmq_sock = NULL;
    int rc = uvzmq_socket_new(
        &loop, zmq_sock, server_on_recv, &data->received_count, &uvzmq_sock);
    if (rc != 0) {
        fprintf(stderr, "[ERROR] uvzmq_socket_new failed: %d\n", rc);
        return NULL;
    }

    printf("[SERVER] Ready to receive messages\n");

    // Run event loop
    for (int i = 0; i < 100 && data->received_count < 5; i++) {
        uv_run(&loop, UV_RUN_ONCE);
        usleep(10000);
        if (i % 10 == 0) {
            printf("[SERVER] Loop iteration %d, received: %d\n",
                   i,
                   data->received_count);
        }
    }

    printf("[SERVER] Received %d messages\n", data->received_count);

    uvzmq_socket_free(uvzmq_sock);
    zmq_close(zmq_sock);
    zmq_ctx_term(zmq_ctx);
    uv_loop_close(&loop);

    return NULL;
}

static void* client_thread_func(void* arg) {
    struct test_data* data = (struct test_data*)arg;

    printf("[CLIENT] Starting PUSH client, waiting for server...\n");
    usleep(200000);  // Wait for server

    void* zmq_ctx = zmq_ctx_new();
    void* zmq_sock = zmq_socket(zmq_ctx, ZMQ_PUSH);

    char connect_addr[64];
    snprintf(
        connect_addr, sizeof(connect_addr), "tcp://127.0.0.1:%d", data->port);
    zmq_connect(zmq_sock, connect_addr);

    printf("[CLIENT] Sending 5 messages...\n");
    for (int i = 0; i < 5; i++) {
        char msg[64];
        snprintf(msg, sizeof(msg), "Message %d", i);
        int send_rc = zmq_send(zmq_sock, msg, strlen(msg), 0);
        if (send_rc < 0) {
            fprintf(stderr,
                    "[CLIENT ERROR] zmq_send failed at i=%d: %d (errno=%d)\n",
                    i,
                    send_rc,
                    errno);
        } else {
            printf("[CLIENT] Sent: %s\n", msg);
        }
        usleep(100000);  // 100ms between messages
    }

    printf("[CLIENT] Finished\n");

    zmq_close(zmq_sock);
    zmq_ctx_term(zmq_ctx);

    return NULL;
}

int main(void) {
    printf("========================================\n");
    printf("Simple PUSH/PULL Test\n");
    printf("========================================\n\n");

    struct test_data data = {.port = 5559, .received_count = 0};

    pthread_t server_thread, client_thread;
    pthread_create(&server_thread, NULL, server_thread_func, &data);
    pthread_create(&client_thread, NULL, client_thread_func, &data);

    pthread_join(server_thread, NULL);
    pthread_join(client_thread, NULL);

    printf("========================================\n");
    printf("Test Complete\n");
    printf("========================================\n");

    return 0;
}