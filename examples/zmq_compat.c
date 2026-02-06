#include "../include/uvzmq.h"
#include <stdio.h>
#include <string.h>

int main(void)
{
    printf("UVZMQ ZMQ Compatibility Example\n");
    printf("================================\n\n");

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

    printf("Using ZMQ-compatible socket options:\n");

    rc = uvzmq_setsockopt_int(sock, ZMQ_LINGER, 1000);
    printf("  Set ZMQ_LINGER to 1000: %s\n", uvzmq_strerror(rc));

    rc = uvzmq_setsockopt_int(sock, ZMQ_RCVTIMEO, 5000);
    printf("  Set ZMQ_RCVTIMEO to 5000: %s\n", uvzmq_strerror(rc));

    rc = uvzmq_setsockopt_int(sock, ZMQ_SNDTIMEO, 5000);
    printf("  Set ZMQ_SNDTIMEO to 5000: %s\n", uvzmq_strerror(rc));

    int linger = 0;
    rc = uvzmq_getsockopt_int(sock, ZMQ_LINGER, &linger);
    printf("  Get ZMQ_LINGER: %d (%s)\n", linger, uvzmq_strerror(rc));

    int64_t hwm = 0;
    rc = uvzmq_getsockopt_int64(sock, ZMQ_RCVHWM, &hwm);
    printf("  Get ZMQ_RCVHWM: %ld (%s)\n", hwm, uvzmq_strerror(rc));

    char identity[256];
    size_t identity_size = sizeof(identity);
    rc = uvzmq_getsockopt_bin(sock, ZMQ_IDENTITY, identity, &identity_size);
    printf("  Get ZMQ_IDENTITY: %s\n", uvzmq_strerror(rc));

    printf("\nSocket created successfully with UVZMQ but using ZMQ API!\n");
    printf("All ZMQ socket options (int, uint64, bin) are fully compatible.\n");

    uvzmq_socket_free(sock);
    uvzmq_context_free(ctx);
    uv_loop_close(&loop);

    return 0;
}