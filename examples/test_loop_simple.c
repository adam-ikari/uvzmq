#define UVZMQ_IMPLEMENTATION
#include "../include/uvzmq.h"
#include <zmq.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static int g_received = 0;

static void on_recv(uvzmq_socket_t *socket, zmq_msg_t *msg, void *user_data)
{
    (void)socket;
    printf("[RECV] Message received\n");
    fflush(stdout);
    zmq_msg_send(msg, uvzmq_get_zmq_socket(socket), 0);
    zmq_msg_close(msg);
    (*(int *)user_data)++;
}

int main(void)
{
    printf("[TEST] Starting simple loop test\n");
    fflush(stdout);
    
    uv_loop_t loop;
    uv_loop_init(&loop);
    printf("[TEST] Loop initialized\n");
    fflush(stdout);
    
    void *zmq_ctx = zmq_ctx_new();
    void *zmq_sock = zmq_socket(zmq_ctx, ZMQ_REP);
    zmq_bind(zmq_sock, "tcp://*:5701");
    printf("[TEST] ZMQ socket bound\n");
    fflush(stdout);
    
    g_received = 0;
    
    uvzmq_socket_t *uvzmq_sock = NULL;
    int rc = uvzmq_socket_new(&loop, zmq_sock, on_recv, NULL, &g_received, &uvzmq_sock);
    printf("[TEST] uvzmq_socket_new returned: %d\n", rc);
    fflush(stdout);
    
    printf("[TEST] Testing UV_RUN_ONCE...\n");
    fflush(stdout);
    for (int i = 0; i < 3; i++) {
        int rc = uv_run(&loop, UV_RUN_ONCE);
        printf("[TEST] UV_RUN_ONCE iteration %d returned: %d\n", i, rc);
        fflush(stdout);
    }
    
    printf("[TEST] Testing UV_RUN_NOWAIT...\n");
    fflush(stdout);
    for (int i = 0; i < 3; i++) {
        int rc = uv_run(&loop, UV_RUN_NOWAIT);
        printf("[TEST] UV_RUN_NOWAIT iteration %d returned: %d\n", i, rc);
        fflush(stdout);
    }
    
    printf("[TEST] Received %d messages\n", g_received);
    fflush(stdout);
    
    uvzmq_socket_free(uvzmq_sock);
    printf("[TEST] uvzmq_socket_free completed\n");
    fflush(stdout);
    
    zmq_close(zmq_sock);
    zmq_ctx_term(zmq_ctx);
    uv_loop_close(&loop);
    
    printf("\n[TEST] Completed successfully\n");
    fflush(stdout);
    
    return 0;
}