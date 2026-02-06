#define UVZMQ_IMPLEMENTATION
#include "../include/uvzmq.h"
#include <zmq.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static void on_recv(uvzmq_socket_t *socket, zmq_msg_t *msg, void *user_data)
{
    (void)socket;
    (void)user_data;
    size_t size = zmq_msg_size(msg);
    const char *data = (const char *)zmq_msg_data(msg);
    printf("Received: %.*s\n", (int)size, data);
    zmq_msg_close(msg);
    uv_loop_t *loop = uvzmq_get_loop(socket);
    uv_stop(loop);
}

int main(void)
{
    printf("UVZMQ Simplified API Example\n");
    printf("==============================\n\n");
    fflush(stdout);

    uv_loop_t loop;
    uv_loop_init(&loop);

    // Create ZMQ context and socket using standard ZMQ APIs
    void *zmq_ctx = zmq_ctx_new();
    void *zmq_sock = zmq_socket(zmq_ctx, ZMQ_REQ);

    // Configure ZMQ socket using standard ZMQ APIs
    int timeout = 5000;
    zmq_setsockopt(zmq_sock, ZMQ_RCVTIMEO, &timeout, sizeof(timeout));
    zmq_connect(zmq_sock, "tcp://127.0.0.1:5555");

    // Integrate with libuv using UVZMQ
    uvzmq_socket_t *uvzmq_sock = NULL;
    int rc = uvzmq_socket_new(&loop, zmq_sock, on_recv, NULL, NULL, &uvzmq_sock);
    if (rc != 0) {
        fprintf(stderr, "Failed to create UVZMQ socket: %s\n", uvzmq_strerror(rc));
        return 1;
    }

    printf("Connected to tcp://127.0.0.1:5555\n");
    printf("Sending message...\n");
    fflush(stdout);

    // Send message using standard ZMQ API
    const char *msg = "Hello from UVZMQ!";
    zmq_send(zmq_sock, msg, strlen(msg), 0);

    printf("Waiting for reply...\n\n");
    fflush(stdout);

    // Run libuv event loop
    uv_run(&loop, UV_RUN_DEFAULT);

    // Cleanup
    uvzmq_socket_free(uvzmq_sock);
    zmq_close(zmq_sock);
    zmq_ctx_term(zmq_ctx);
    uv_loop_close(&loop);

    printf("\nDone!\n");
    return 0;
}