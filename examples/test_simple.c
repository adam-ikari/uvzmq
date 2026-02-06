#include "../include/uvzmq.h"
#include <zmq.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

int main(void)
{
    printf("[TEST] Starting simple test\n");
    fflush(stdout);
    
    // Step 1: Create loop
    printf("[TEST] Creating libuv loop...\n");
    fflush(stdout);
    uv_loop_t loop;
    int rc = uv_loop_init(&loop);
    if (rc != 0) {
        fprintf(stderr, "[ERROR] uv_loop_init failed: %d\n", rc);
        return 1;
    }
    printf("[TEST] uv_loop_init succeeded\n");
    fflush(stdout);
    
    // Step 2: Create ZMQ socket
    printf("[TEST] Creating ZMQ socket...\n");
    fflush(stdout);
    void *zmq_ctx = zmq_ctx_new();
    if (!zmq_ctx) {
        fprintf(stderr, "[ERROR] zmq_ctx_new failed\n");
        uv_loop_close(&loop);
        return 1;
    }
    printf("[TEST] zmq_ctx_new succeeded\n");
    fflush(stdout);
    
    void *zmq_sock = zmq_socket(zmq_ctx, ZMQ_REP);
    if (!zmq_sock) {
        fprintf(stderr, "[ERROR] zmq_socket failed\n");
        zmq_ctx_term(zmq_ctx);
        uv_loop_close(&loop);
        return 1;
    }
    printf("[TEST] zmq_socket succeeded\n");
    fflush(stdout);
    
    // Step 3: Bind socket
    printf("[TEST] Binding socket...\n");
    fflush(stdout);
    rc = zmq_bind(zmq_sock, "tcp://*:5999");
    if (rc != 0) {
        fprintf(stderr, "[ERROR] zmq_bind failed: %d\n", rc);
        zmq_close(zmq_sock);
        zmq_ctx_term(zmq_ctx);
        uv_loop_close(&loop);
        return 1;
    }
    printf("[TEST] zmq_bind succeeded\n");
    fflush(stdout);
    
    // Step 3.5: Check socket FD
    printf("[TEST] Getting socket FD...\n");
    fflush(stdout);
    int fd;
    size_t fd_size = sizeof(fd);
    rc = zmq_getsockopt(zmq_sock, ZMQ_FD, &fd, &fd_size);
    if (rc != 0) {
        fprintf(stderr, "[ERROR] zmq_getsockopt ZMQ_FD failed: %d\n", rc);
        zmq_close(zmq_sock);
        zmq_ctx_term(zmq_ctx);
        uv_loop_close(&loop);
        return 1;
    }
    printf("[TEST] Socket FD: %d\n", fd);
    fflush(stdout);
    
    // Check FD flags
    int flags = fcntl(fd, F_GETFL, 0);
    printf("[TEST] FD flags: %d (errno: %d)\n", flags, errno);
    fflush(stdout);
    
    // Step 4: Create uvzmq socket
    printf("[TEST] Creating uvzmq socket...\n");
    fflush(stdout);
    uvzmq_socket_t *uvzmq_sock = NULL;
    int received = 0;
    
    void on_recv(uvzmq_socket_t *socket, zmq_msg_t *msg, void *user_data)
    {
        (void)socket;
        (void)msg;
        (*(int *)user_data)++;
        printf("[TEST] on_recv called, total: %d\n", *(int *)user_data);
        fflush(stdout);
    }
    
    printf("[TEST] About to call uvzmq_socket_new...\n");
    fflush(stdout);
    rc = uvzmq_socket_new(&loop, zmq_sock, on_recv, NULL, &received, &uvzmq_sock);
    if (rc != UVZMQ_OK) {
        fprintf(stderr, "[ERROR] uvzmq_socket_new failed: %s\n", uvzmq_strerror_last());
        zmq_close(zmq_sock);
        zmq_ctx_term(zmq_ctx);
        uv_loop_close(&loop);
        return 1;
    }
    printf("[TEST] uvzmq_socket_new succeeded\n");
    fflush(stdout);
    
    // Step 5: Run loop for a short time
    printf("[TEST] Running loop for 2 seconds...\n");
    fflush(stdout);
    
    // Use UV_RUN_ONCE with a short sleep to avoid busy waiting
    // This is the correct way to run libuv with active poll handles
    int iterations = 0;
    time_t start_time = time(NULL);
    while (time(NULL) - start_time < 2) {
        rc = uv_run(&loop, UV_RUN_ONCE);
        iterations++;
        if (iterations <= 5) {
            printf("[TEST] Iteration %d, rc=%d, received=%d\n", iterations, rc, received);
            fflush(stdout);
        }
        usleep(10000);  // 10ms sleep to prevent busy waiting
    }
    
    printf("[TEST] Total iterations: %d\n", iterations);
    printf("[TEST] Total received: %d\n", received);
    fflush(stdout);
    
    // Step 6: Cleanup
    printf("[TEST] Cleaning up...\n");
    fflush(stdout);
    uvzmq_socket_free(uvzmq_sock);
    printf("[TEST] uvzmq_socket_free succeeded\n");
    fflush(stdout);
    
    zmq_close(zmq_sock);
    printf("[TEST] zmq_close succeeded\n");
    fflush(stdout);
    
    zmq_ctx_term(zmq_ctx);
    printf("[TEST] zmq_ctx_term succeeded\n");
    fflush(stdout);
    
    uv_loop_close(&loop);
    printf("[TEST] uv_loop_close succeeded\n");
    fflush(stdout);
    
    printf("\n[TEST] All tests passed!\n");
    fflush(stdout);
    
    return 0;
}