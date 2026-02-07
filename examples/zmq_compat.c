#define UVZMQ_IMPLEMENTATION
#include <stdio.h>
#include <string.h>
#include <zmq.h>

#include "../include/uvzmq.h"

int main(void) {
    printf("UVZMQ ZMQ Compatibility Example\n");
    printf("================================\n\n");

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

    printf("Using ZMQ-compatible socket options:\n");

    // Use ZMQ API directly on the socket
    int linger = 1000;
    rc = zmq_setsockopt(zmq_sock, ZMQ_LINGER, &linger, sizeof(linger));
    printf("  Set ZMQ_LINGER to 1000: %s\n",
           (rc == 0) ? "Success" : zmq_strerror(errno));

    int rcv_timeout = 5000;
    rc = zmq_setsockopt(
        zmq_sock, ZMQ_RCVTIMEO, &rcv_timeout, sizeof(rcv_timeout));
    printf("  Set ZMQ_RCVTIMEO to 5000: %s\n",
           (rc == 0) ? "Success" : zmq_strerror(errno));

    int snd_timeout = 5000;
    rc = zmq_setsockopt(
        zmq_sock, ZMQ_SNDTIMEO, &snd_timeout, sizeof(snd_timeout));
    printf("  Set ZMQ_SNDTIMEO to 5000: %s\n",
           (rc == 0) ? "Success" : zmq_strerror(errno));

    // Get socket options
    rc = zmq_getsockopt(
        zmq_sock, ZMQ_LINGER, &linger, &(size_t){sizeof(linger)});
    printf("  Get ZMQ_LINGER: %d\n", linger);

    int64_t hwm = 0;
    rc = zmq_getsockopt(zmq_sock, ZMQ_RCVHWM, &hwm, &(size_t){sizeof(hwm)});
    printf("  Get ZMQ_RCVHWM: %ld\n", hwm);

    printf(
        "\nSocket created successfully with UVZMQ but using ZMQ API "
        "directly!\n");
    printf("All ZMQ socket options are fully compatible.\n");

    uvzmq_socket_free(socket);
    zmq_close(zmq_sock);
    zmq_ctx_term(zmq_ctx);
    uv_loop_close(&loop);

    return 0;
}