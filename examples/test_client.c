#include <stdio.h>
#include <string.h>
#include <zmq.h>

int main(void) {
    void* ctx = zmq_ctx_new();
    void* sock = zmq_socket(ctx, ZMQ_REQ);

    printf("Connecting to server...\n");
    zmq_connect(sock, "tcp://127.0.0.1:5999");

    printf("Sending message...\n");
    zmq_send(sock, "Hello from client", 17, 0);

    printf("Waiting for reply...\n");
    zmq_msg_t reply;
    zmq_msg_init(&reply);
    zmq_msg_recv(&reply, sock, 0);

    printf("Received reply: %.*s\n",
           (int)zmq_msg_size(&reply),
           (char*)zmq_msg_data(&reply));
    zmq_msg_close(&reply);

    zmq_close(sock);
    zmq_ctx_term(ctx);

    return 0;
}