#include <stdio.h>
#include <string.h>
#include <zmq.h>

#include "uvzmq.h"

int main(void) {
    printf("UVZMQ Direct ZMQ API Access Example\n");
    printf("====================================\n\n");

    uv_loop_t loop;
    uv_loop_init(&loop);

    // Create ZMQ socket directly
    void* zmq_ctx = zmq_ctx_new();
    void* zmq_sock = zmq_socket(zmq_ctx, ZMQ_REQ);
    if (!zmq_sock) {
        fprintf(stderr, "Failed to create ZMQ socket\n");
        return 1;
    }

    // Create uvzmq socket
    uvzmq_socket_t* socket = NULL;
    int rc = uvzmq_socket_new(&loop, zmq_sock, NULL, NULL, &socket);
    if (rc != 0) {
        fprintf(stderr, "Failed to create uvzmq socket: %d\n", rc);
        zmq_close(zmq_sock);
        zmq_ctx_term(zmq_ctx);
        return 1;
    }

    printf("Direct access to underlying ZMQ socket:\n");

    // Get ZMQ socket pointer
    void* retrieved_zmq_sock = uvzmq_get_zmq_socket(socket);
    printf("  ZMQ socket pointer: %p\n", retrieved_zmq_sock);

    // Use ZMQ API directly
    int linger = 0;
    size_t size = sizeof(linger);
    rc = zmq_getsockopt(retrieved_zmq_sock, ZMQ_LINGER, &linger, &size);
    printf("  Using zmq_getsockopt directly: %d (error: %s)\n",
           linger,
           (rc == 0) ? "none" : zmq_strerror(errno));

    rc = zmq_setsockopt(
        retrieved_zmq_sock, ZMQ_LINGER, &(int){2000}, sizeof(int));
    printf("  Using zmq_setsockopt directly: %s\n",
           (rc == 0) ? "Success" : "Failed");

    printf("\nYou can mix UVZMQ and ZMQ APIs freely:\n");
    printf("  - Use uvzmq_get_zmq_socket() to get the underlying ZMQ socket\n");
    printf("  - Then use zmq_setsockopt/zmq_getsockopt directly\n");
    printf("  - All ZMQ socket options work the same way\n");

    uvzmq_socket_free(socket);
    zmq_close(zmq_sock);
    zmq_ctx_term(zmq_ctx);
    uv_loop_close(&loop);

    return 0;
}