#include <stdio.h>
#include <string.h>

#include "uvzmq.h"

void on_recv(uvzmq_socket_t* socket, zmq_msg_t* msg, void* user_data) {
    (void)socket;
    (void)user_data;
    size_t size = zmq_msg_size(msg);
    const char* data = (const char*)zmq_msg_data(msg);
    printf("Received: %.*s\n", (int)size, data);
    zmq_msg_close(msg);
}

int main(void) {
    uv_loop_t loop;
    uv_loop_init(&loop);

    // Create ZMQ socket
    void* zmq_sock = zmq_socket(zmq_ctx_new(), ZMQ_REQ);
    if (!zmq_sock) {
        fprintf(stderr, "Failed to create ZMQ socket\n");
        return 1;
    }

    // Create uvzmq socket
    uvzmq_socket_t* socket = NULL;
    int rc = uvzmq_socket_new(&loop, zmq_sock, on_recv, NULL, &socket);
    if (rc != 0) {
        fprintf(stderr, "Failed to create uvzmq socket: %d\n", rc);
        zmq_close(zmq_sock);
        return 1;
    }

    // Connect
    rc = zmq_connect(zmq_sock, "tcp://localhost:5555");
    if (rc != 0) {
        fprintf(stderr, "Failed to connect: %s\n", zmq_strerror(errno));
        uvzmq_socket_free(socket);
        zmq_close(zmq_sock);
        return 1;
    }

    // Send and receive messages
    for (int i = 0; i < 10; i++) {
        char msg[64];
        snprintf(msg, sizeof(msg), "Hello %d", i);

        rc = zmq_send(zmq_sock, msg, strlen(msg), 0);
        if (rc < 0) {
            fprintf(stderr, "Failed to send: %s\n", zmq_strerror(errno));
            break;
        }

        printf("Sent: %s\n", msg);

        // Receive in the event loop
        uv_run(&loop, UV_RUN_NOWAIT);
    }

    uvzmq_socket_free(socket);
    zmq_close(zmq_sock);
    uv_loop_close(&loop);

    return 0;
}