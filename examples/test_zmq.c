#include <stdio.h>
#include <zmq.h>

int main(void) {
    printf("Before zmq_ctx_new\n");
    fflush(stdout);

    void* ctx = zmq_ctx_new();

    printf("After zmq_ctx_new\n");
    fflush(stdout);

    void* sock = zmq_socket(ctx, ZMQ_REQ);

    printf("After zmq_socket\n");
    fflush(stdout);

    zmq_connect(sock, "tcp://127.0.0.1:5555");

    printf("After zmq_connect\n");
    fflush(stdout);

    zmq_close(sock);
    zmq_ctx_term(ctx);

    printf("Done!\n");
    return 0;
}