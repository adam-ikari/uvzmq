#define UVZMQ_IMPLEMENTATION
#include <stdio.h>
#include <zmq.h>

#include "../include/uvzmq.h"

int main(void) {
    printf("Step 1: Init loop\n");
    fflush(stdout);

    uv_loop_t loop;
    uv_loop_init(&loop);

    printf("Step 2: Create ZMQ context\n");
    fflush(stdout);

    void* zmq_ctx = zmq_ctx_new();

    printf("Step 3: Create ZMQ socket\n");
    fflush(stdout);

    void* zmq_sock = zmq_socket(zmq_ctx, ZMQ_REP);

    printf("Step 4: Bind socket\n");
    fflush(stdout);

    zmq_bind(zmq_sock, "tcp://*:6000");

    printf("Step 5: Get ZMQ FD\n");
    fflush(stdout);

    int fd;
    size_t fd_size = sizeof(fd);
    zmq_getsockopt(zmq_sock, ZMQ_FD, &fd, &fd_size);
    printf("ZMQ FD: %d\n", fd);
    fflush(stdout);

    printf("Step 6: Create poll handle\n");
    fflush(stdout);

    uv_poll_t poll_handle;
    int rc = uv_poll_init(&loop, &poll_handle, fd);
    printf("uv_poll_init returned: %d\n", rc);
    fflush(stdout);

    if (rc != 0) {
        printf("uv_poll_init failed!\n");
        return 1;
    }

    printf("Step 7: Start poll\n");
    fflush(stdout);

    void poll_callback(uv_poll_t * handle, int status, int events) {
        (void)handle;
        (void)status;
        (void)events;
        printf("Poll callback called!\n");
        fflush(stdout);
    }

    rc = uv_poll_start(&poll_handle, UV_READABLE, poll_callback);
    printf("uv_poll_start returned: %d\n", rc);
    fflush(stdout);

    if (rc != 0) {
        printf("uv_poll_start failed!\n");
        return 1;
    }

    printf("Step 8: Run loop once\n");
    fflush(stdout);

    rc = uv_run(&loop, UV_RUN_ONCE);
    printf("uv_run returned: %d\n", rc);
    fflush(stdout);

    printf("Step 9: Cleanup\n");
    fflush(stdout);

    uv_poll_stop(&poll_handle);
    zmq_close(zmq_sock);
    zmq_ctx_term(zmq_ctx);
    uv_loop_close(&loop);

    printf("Done!\n");
    return 0;
}