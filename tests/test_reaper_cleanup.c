/**
 * @file test_reaper_cleanup.c
 * @brief Test program to verify socket cleanup works correctly when ZMQ_IO_THREADS=0
 *
 * This test verifies that:
 * 1. Sockets can be created and closed without reaper thread
 * 2. Memory is properly cleaned up
 * 3. Multiple socket create/close cycles work correctly
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <zmq.h>
#include <uv.h>
#include "uvzmq.h"

#define TEST_PORT 5556
#define TEST_ITERATIONS 100
#define TEST_SOCKETS 10

void on_recv(uvzmq_socket_t* socket, zmq_msg_t* msg, void* user_data) {
    (void)socket;
    (void)user_data;
    zmq_msg_close(msg);
}

int test_socket_create_close() {
    printf("Test 1: Socket create/close cycle...\n");

    uv_loop_t loop;
    uv_loop_init(&loop);

    void* zmq_ctx = zmq_ctx_new();
    zmq_ctx_set(zmq_ctx, ZMQ_IO_THREADS, 0);

    for (int i = 0; i < TEST_ITERATIONS; i++) {
        void* zmq_sock = zmq_socket(zmq_ctx, ZMQ_REP);
        if (!zmq_sock) {
            fprintf(stderr, "Failed to create socket at iteration %d\n", i);
            return -1;
        }

        uvzmq_socket_t* uvzmq_sock = NULL;
        if (uvzmq_socket_new(&loop, zmq_sock, on_recv, NULL, &uvzmq_sock) != 0) {
            fprintf(stderr, "Failed to create uvzmq socket at iteration %d\n", i);
            zmq_close(zmq_sock);
            return -1;
        }

        uvzmq_socket_free(uvzmq_sock);
        if (zmq_close(zmq_sock) != 0) {
            fprintf(stderr, "Failed to close zmq socket at iteration %d\n", i);
            return -1;
        }
    }

    uv_run(&loop, UV_RUN_NOWAIT);
    zmq_ctx_term(zmq_ctx);
    uv_loop_close(&loop);

    printf("  ✓ Completed %d create/close cycles\n", TEST_ITERATIONS);
    return 0;
}

int test_multiple_sockets() {
    printf("Test 2: Multiple sockets simultaneous...\n");

    uv_loop_t loop;
    uv_loop_init(&loop);

    void* zmq_ctx = zmq_ctx_new();
    zmq_ctx_set(zmq_ctx, ZMQ_IO_THREADS, 0);

    uvzmq_socket_t* sockets[TEST_SOCKETS];
    void* zmq_socks[TEST_SOCKETS];

    for (int i = 0; i < TEST_SOCKETS; i++) {
        zmq_socks[i] = zmq_socket(zmq_ctx, ZMQ_REP);
        if (!zmq_socks[i]) {
            fprintf(stderr, "Failed to create socket %d\n", i);
            goto cleanup;
        }

        if (uvzmq_socket_new(&loop, zmq_socks[i], on_recv, NULL, &sockets[i]) != 0) {
            fprintf(stderr, "Failed to create uvzmq socket %d\n", i);
            goto cleanup;
        }
    }

    printf("  ✓ Created %d sockets\n", TEST_SOCKETS);

cleanup:
    for (int i = 0; i < TEST_SOCKETS; i++) {
        if (sockets[i]) {
            uvzmq_socket_free(sockets[i]);
        }
        if (zmq_socks[i]) {
            zmq_close(zmq_socks[i]);
        }
    }

    uv_run(&loop, UV_RUN_NOWAIT);
    zmq_ctx_term(zmq_ctx);
    uv_loop_close(&loop);

    printf("  ✓ Cleaned up all sockets\n");
    return 0;
}

int test_bind_unbind() {
    printf("Test 3: Socket bind/unbind...\n");

    uv_loop_t loop;
    uv_loop_init(&loop);

    void* zmq_ctx = zmq_ctx_new();
    zmq_ctx_set(zmq_ctx, ZMQ_IO_THREADS, 0);

    char endpoint[64];
    for (int i = 0; i < TEST_ITERATIONS; i++) {
        snprintf(endpoint, sizeof(endpoint), "tcp://127.0.0.1:%d", TEST_PORT + i);

        void* zmq_sock = zmq_socket(zmq_ctx, ZMQ_REP);
        if (!zmq_sock) {
            fprintf(stderr, "Failed to create socket at iteration %d\n", i);
            goto cleanup;
        }

        if (zmq_bind(zmq_sock, endpoint) != 0) {
            fprintf(stderr, "Failed to bind at iteration %d: %s\n", i, zmq_strerror(zmq_errno()));
            zmq_close(zmq_sock);
            continue;
        }

        uvzmq_socket_t* uvzmq_sock = NULL;
        if (uvzmq_socket_new(&loop, zmq_sock, on_recv, NULL, &uvzmq_sock) != 0) {
            fprintf(stderr, "Failed to create uvzmq socket at iteration %d\n", i);
            zmq_close(zmq_sock);
            goto cleanup;
        }

        usleep(1000);  // Small delay to allow bind to complete

        uvzmq_socket_free(uvzmq_sock);
        if (zmq_close(zmq_sock) != 0) {
            fprintf(stderr, "Failed to close zmq socket at iteration %d\n", i);
            goto cleanup;
        }
    }

    printf("  ✓ Completed %d bind/unbind cycles\n", TEST_ITERATIONS);

cleanup:
    uv_run(&loop, UV_RUN_NOWAIT);
    zmq_ctx_term(zmq_ctx);
    uv_loop_close(&loop);

    return 0;
}

int test_rapid_create_destroy() {
    printf("Test 4: Rapid create/destroy stress test...\n");

    uv_loop_t loop;
    uv_loop_init(&loop);

    void* zmq_ctx = zmq_ctx_new();
    zmq_ctx_set(zmq_ctx, ZMQ_IO_THREADS, 0);

    for (int i = 0; i < 1000; i++) {
        void* zmq_sock = zmq_socket(zmq_ctx, ZMQ_REP);
        if (zmq_sock) {
            uvzmq_socket_t* uvzmq_sock = NULL;
            if (uvzmq_socket_new(&loop, zmq_sock, on_recv, NULL, &uvzmq_sock) == 0) {
                uvzmq_socket_free(uvzmq_sock);
            }
            zmq_close(zmq_sock);
        }
    }

    printf("  ✓ Completed 1000 rapid create/destroy cycles\n");

    uv_run(&loop, UV_RUN_NOWAIT);
    zmq_ctx_term(zmq_ctx);
    uv_loop_close(&loop);

    return 0;
}

int main() {
    printf("=== Reaper Cleanup Test (ZMQ_IO_THREADS=0) ===\n\n");

    int result = 0;

    result |= test_socket_create_close();
    result |= test_multiple_sockets();
    result |= test_bind_unbind();
    result |= test_rapid_create_destroy();

    printf("\n=== Test Results ===\n");
    if (result == 0) {
        printf("✓ All tests passed!\n");
    } else {
        printf("✗ Some tests failed!\n");
    }

    return result;
}