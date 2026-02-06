#define UVZMQ_IMPLEMENTATION
#include "../include/uvzmq.h"
#include <zmq.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    printf("UVZMQ Direct ZMQ API Access Example\n");
    printf("====================================\n\n");

    uv_loop_t loop;
    uv_loop_init(&loop);

    uvzmq_context_t *ctx = NULL;
    int rc = uvzmq_context_new(&loop, &ctx);
    if (rc != UVZMQ_OK) {
        fprintf(stderr, "Failed to create context: %s\n", uvzmq_strerror(rc));
        return 1;
    }

    uvzmq_socket_t *sock = NULL;
    rc = uvzmq_socket_new(ctx, UVZMQ_REQ, &sock);
    if (rc != UVZMQ_OK) {
        fprintf(stderr, "Failed to create socket: %s\n", uvzmq_strerror(rc));
        uvzmq_context_free(ctx);
        return 1;
    }

    printf("Direct access to underlying ZMQ socket:\n");

    void *zmq_sock = uvzmq_socket_get_zmq_socket(sock);
    printf("  ZMQ socket pointer: %p\n", zmq_sock);

    int linger = 0;
    size_t size = sizeof(linger);
    rc = zmq_getsockopt(zmq_sock, ZMQ_LINGER, &linger, &size);
    printf("  Using zmq_getsockopt directly: %d\n", linger);

    rc = zmq_setsockopt(zmq_sock, ZMQ_LINGER, &(int){2000}, sizeof(int));
    printf("  Using zmq_setsockopt directly: %s\n", (rc == 0) ? "Success" : "Failed");

    printf("\nYou can mix UVZMQ and ZMQ APIs freely:\n");
    printf("  - Use uvzmq_setsockopt_int for convenience\n");
    printf("  - Or use zmq_setsockopt directly for full ZMQ compatibility\n");
    printf("  - All ZMQ socket options work the same way\n");
    printf("  - Error codes are mapped automatically\n");

    uvzmq_socket_free(sock);
    uvzmq_context_free(ctx);
    uv_loop_close(&loop);

    return 0;
}