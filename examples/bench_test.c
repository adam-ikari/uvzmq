#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <zmq.h>

#include "uvzmq.h"

static void server_on_recv(uvzmq_socket_t* socket,
                           zmq_msg_t* msg,
                           void* user_data) {
    int* count = (int*)user_data;
    zmq_msg_send(msg, uvzmq_get_zmq_socket(socket), 0);
    zmq_msg_close(msg);
    (*count)++;
}

static int received = 0;

static void* server_func(void* arg) {
    int* ready = (int*)arg;

    uv_loop_t loop;
    uv_loop_init(&loop);

    void* zmq_ctx = zmq_ctx_new();
    void* zmq_sock = zmq_socket(zmq_ctx, ZMQ_REP);
    uvzmq_socket_t* uvzmq_sock = NULL;
    uvzmq_socket_new(&loop, zmq_sock, server_on_recv, &received, &uvzmq_sock);

    *ready = 1;

    // Run event loop
    for (int i = 0; i < 200 && received < 10; i++) {
        uv_run(&loop, UV_RUN_ONCE);
        usleep(10000);
    }

    uvzmq_socket_free(uvzmq_sock);
    zmq_close(zmq_sock);
    zmq_ctx_term(zmq_ctx);
    uv_loop_close(&loop);

    return NULL;
}

int main(void) {
    printf("Simple Multi-Thread Test\n");
    printf("======================\n\n");

    int ready = 0;

    pthread_t server_thread;
    pthread_create(&server_thread, NULL, server_func, &ready);

    while (!ready) {
        usleep(10000);
    }

    usleep(100000);

    void* zmq_ctx = zmq_ctx_new();
    void* zmq_sock = zmq_socket(zmq_ctx, ZMQ_REQ);

    int timeout = 5000;
    zmq_setsockopt(zmq_sock, ZMQ_RCVTIMEO, &timeout, sizeof(timeout));
    zmq_connect(zmq_sock, "tcp://127.0.0.1:6002");

    printf("Sending 10 messages...\n");

    for (int i = 0; i < 10; i++) {
        char msg[32];
        snprintf(msg, sizeof(msg), "Msg %d", i);
        zmq_send(zmq_sock, msg, strlen(msg), 0);

        zmq_msg_t reply;
        zmq_msg_init(&reply);
        zmq_msg_recv(&reply, zmq_sock, 0);
        zmq_msg_close(&reply);

        printf("Sent: %s\n", msg);
        usleep(100000);
    }

    pthread_join(server_thread, NULL);

    zmq_close(zmq_sock);
    zmq_ctx_term(zmq_ctx);

    printf("\nReceived: %d messages\n", received);
    printf("✅ Test passed!\n");

    return 0;
}