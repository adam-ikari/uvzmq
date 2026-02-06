#include "../include/uvzmq.h"
#include <stdio.h>
#include <string.h>

int main(void)
{
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

    rc = uvzmq_setsockopt_int(sock, ZMQ_RCVTIMEO, 5000);
    if (rc != UVZMQ_OK) {
        fprintf(stderr, "Failed to set RCVTIMEO: %s\n", uvzmq_strerror(rc));
    }

    rc = uvzmq_connect(sock, "tcp://localhost:5555");
    if (rc != UVZMQ_OK) {
        fprintf(stderr, "Failed to connect: %s\n", uvzmq_strerror(rc));
        uvzmq_socket_free(sock);
        uvzmq_context_free(ctx);
        return 1;
    }

    for (int i = 0; i < 10; i++) {
        char msg[64];
        snprintf(msg, sizeof(msg), "Hello %d", i);

        rc = uvzmq_send_string(sock, msg, 0);
        if (rc != UVZMQ_OK) {
            fprintf(stderr, "Failed to send: %s\n", uvzmq_strerror(rc));
            break;
        }

        printf("Sent: %s\n", msg);

        char *reply = NULL;
        rc = uvzmq_recv_string(sock, &reply, 0);
        if (rc != UVZMQ_OK) {
            fprintf(stderr, "Failed to receive: %s\n", uvzmq_strerror(rc));
            break;
        }

        printf("Received: %s\n", reply);
        UVZMQ_FREE(reply);
    }

    uvzmq_socket_free(sock);
    uvzmq_context_free(ctx);
    uv_loop_close(&loop);

    return 0;
}