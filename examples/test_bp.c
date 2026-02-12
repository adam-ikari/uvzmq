#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <zmq.h>

#include "uvzmq.h"

static volatile int keep_running = 1;

static void signal_handler(int sig) {
    (void)sig;
    printf("[SIGNAL] Received\n");
    fflush(stdout);
    keep_running = 0;
}

int main(void) {
    printf("Test: Starting\n");
    fflush(stdout);

    printf("Test: Installing signal handlers\n");
    fflush(stdout);
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    printf("Test: Signal handlers installed\n");
    fflush(stdout);

    uv_loop_t loop;
    uv_loop_init(&loop);
    printf("Test: Loop initialized\n");
    fflush(stdout);

    void* zmq_ctx = zmq_ctx_new();
    printf("Test: ZMQ context created\n");
    fflush(stdout);

    void* zmq_sock = zmq_socket(zmq_ctx, ZMQ_REP);
    printf("Test: ZMQ socket created\n");
    fflush(stdout);

    int timeout = 5000;
    printf("Test: Setting ZMQ_RCVTIMEO\n");
    fflush(stdout);
    zmq_setsockopt(zmq_sock, ZMQ_RCVTIMEO, &timeout, sizeof(timeout));
    printf("Test: ZMQ_RCVTIMEO set\n");
    fflush(stdout);

    int rcvbuf = 1024 * 1024;
    printf("Test: Setting ZMQ_RCVBUF\n");
    fflush(stdout);
    zmq_setsockopt(zmq_sock, ZMQ_RCVBUF, &rcvbuf, sizeof(rcvbuf));
    printf("Test: ZMQ_RCVBUF set\n");
    fflush(stdout);

    int sndbuf = 1024 * 1024;
    printf("Test: Setting ZMQ_SNDBUF\n");
    fflush(stdout);
    zmq_setsockopt(zmq_sock, ZMQ_SNDBUF, &sndbuf, sizeof(sndbuf));
    printf("Test: ZMQ_SNDBUF set\n");
    fflush(stdout);

    printf("Test: Binding socket\n");
    fflush(stdout);
    zmq_bind(zmq_sock, "tcp://*:6003");
    printf("Test: Socket bound\n");
    fflush(stdout);

    void on_recv(uvzmq_socket_t * socket, zmq_msg_t * msg, void* user_data) {
        (void)socket;
        (void)msg;
        (void)user_data;
        printf("[RECV] Message received\n");
        fflush(stdout);
    }

    printf("Test: Creating UVZMQ socket\n");
    fflush(stdout);
    uvzmq_socket_t* uvzmq_sock = NULL;
    int rc = uvzmq_socket_new(&loop, zmq_sock, on_recv, NULL, &uvzmq_sock);
    printf("Test: uvzmq_socket_new returned %d\n", rc);
    fflush(stdout);

    printf("Test: Starting event loop\n");
    fflush(stdout);
    for (int i = 0; i < 3; i++) {
        uv_run(&loop, UV_RUN_ONCE);
        printf("Test: Loop iteration %d\n", i);
        fflush(stdout);
        usleep(100000);
    }

    printf("Test: Stopping event loop\n");
    fflush(stdout);
    uvzmq_socket_free(uvzmq_sock);
    zmq_close(zmq_sock);
    zmq_ctx_term(zmq_ctx);
    uv_loop_close(&loop);

    printf("Test: Done!\n");
    return 0;
}
