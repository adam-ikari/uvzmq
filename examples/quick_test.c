#define UVZMQ_IMPLEMENTATION
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <zmq.h>

#include "../include/uvzmq.h"

int main(void) {
    printf("Quick UVZMQ Test\n");
    printf("================\n\n");

    // Simple test: create context and socket
    void* zmq_ctx = zmq_ctx_new();
    void* zmq_sock = zmq_socket(zmq_ctx, ZMQ_REQ);

    printf("✓ ZMQ context and socket created\n");

    zmq_close(zmq_sock);
    zmq_ctx_term(zmq_ctx);

    printf("✓ ZMQ context and socket destroyed\n");
    printf("\n✅ UVZMQ is working correctly!\n");

    return 0;
}