#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <zmq.h>

int main(void) {
    printf("Testing ZMQ PUB/SUB without UVZMQ");
    printf("====================================\n\n");

    void* zmq_ctx = zmq_ctx_new();

    // Create PUB socket
    void* pub_sock = zmq_socket(zmq_ctx, ZMQ_PUB);
    zmq_bind(pub_sock, "tcp://*:5557");

    // Create SUB socket
    void* sub_sock = zmq_socket(zmq_ctx, ZMQ_SUB);
    zmq_setsockopt(sub_sock, ZMQ_SUBSCRIBE, "", 0);
    zmq_connect(sub_sock, "tcp://127.0.0.1:5557");

    printf("Waiting for connection...\n");
    usleep(500000);  // 500ms

    printf("Sending messages...\n");

    // Send a message
    zmq_send(pub_sock, "Hello", 5, 0);
    printf("Sent: Hello\n");

    // Try to receive
    zmq_msg_t msg;
    zmq_msg_init(&msg);

    int rc = zmq_msg_recv(&msg, sub_sock, 1000);  // 1 second timeout
    if (rc >= 0) {
        printf("Received: %.*s\n",
               (int)zmq_msg_size(&msg),
               (char*)zmq_msg_data(&msg));
    } else {
        printf("Failed to receive: %d\n", errno);
    }

    zmq_msg_close(&msg);
    zmq_close(pub_sock);
    zmq_close(sub_sock);
    zmq_ctx_term(zmq_ctx);

    return 0;
}