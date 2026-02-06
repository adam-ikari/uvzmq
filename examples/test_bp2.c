#define UVZMQ_IMPLEMENTATION
#include "../include/uvzmq.h"
#include <zmq.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>

static volatile int keep_running = 1;

static void signal_handler(int sig)
{
    (void)sig;
    keep_running = 0;
}

int main(void)
{
    printf("Test 1: Start\n");
    fflush(stdout);
    
    uv_loop_t loop;
    uv_loop_init(&loop);
    printf("Test 1: Loop OK\n");
    fflush(stdout);
    
    void *zmq_ctx = zmq_ctx_new();
    void *zmq_sock = zmq_socket(zmq_ctx, ZMQ_REP);
    printf("Test 1: ZMQ OK\n");
    fflush(stdout);
    
    zmq_bind(zmq_sock, "tcp://*:6004");
    printf("Test 1: Bind OK\n");
    fflush(stdout);
    
    void on_recv(uvzmq_socket_t *socket, zmq_msg_t *msg, void *user_data)
    {
        (void)socket;
        (void)msg;
        (void)user_data;
    }
    
    uvzmq_socket_t *uvzmq_sock = NULL;
    uvzmq_socket_new(&loop, zmq_sock, on_recv, NULL, NULL, &uvzmq_sock);
    printf("Test 1: UVZMQ OK\n");
    fflush(stdout);
    
    uvzmq_socket_free(uvzmq_sock);
    zmq_close(zmq_sock);
    zmq_ctx_term(zmq_ctx);
    uv_loop_close(&loop);
    
    printf("Test 1: Done!\n");
    fflush(stdout);
    
    printf("\nTest 2: Start (with signal)\n");
    fflush(stdout);
    
    keep_running = 1;
    signal(SIGINT, signal_handler);
    printf("Test 2: Signal handler OK\n");
    fflush(stdout);
    
    uv_loop_init(&loop);
    zmq_ctx = zmq_ctx_new();
    zmq_sock = zmq_socket(zmq_ctx, ZMQ_REP);
    zmq_bind(zmq_sock, "tcp://*:6005");
    uvzmq_socket_new(&loop, zmq_sock, on_recv, NULL, NULL, &uvzmq_sock);
    printf("Test 2: All init OK\n");
    fflush(stdout);
    
    uvzmq_socket_free(uvzmq_sock);
    zmq_close(zmq_sock);
    zmq_ctx_term(zmq_ctx);
    uv_loop_close(&loop);
    
    printf("Test 2: Done!\n");
    fflush(stdout);
    
    printf("\nTest 3: Start (with setsockopt)\n");
    fflush(stdout);
    
    uv_loop_init(&loop);
    zmq_ctx = zmq_ctx_new();
    zmq_sock = zmq_socket(zmq_ctx, ZMQ_REP);
    
    int timeout = 5000;
    zmq_setsockopt(zmq_sock, ZMQ_RCVTIMEO, &timeout, sizeof(timeout));
    printf("Test 3: RCVTIMEO OK\n");
    fflush(stdout);
    
    int rcvbuf = 1024 * 1024;
    zmq_setsockopt(zmq_sock, ZMQ_RCVBUF, &rcvbuf, sizeof(rcvbuf));
    printf("Test 3: RCVBUF OK\n");
    fflush(stdout);
    
    int sndbuf = 1024 * 1024;
    zmq_setsockopt(zmq_sock, ZMQ_SNDBUF, &sndbuf, sizeof(sndbuf));
    printf("Test 3: SNDBUF OK\n");
    fflush(stdout);
    
    zmq_bind(zmq_sock, "tcp://*:6006");
    printf("Test 3: Bind OK\n");
    fflush(stdout);
    
    uvzmq_socket_new(&loop, zmq_sock, on_recv, NULL, NULL, &uvzmq_sock);
    printf("Test 3: UVZMQ OK\n");
    fflush(stdout);
    
    uvzmq_socket_free(uvzmq_sock);
    zmq_close(zmq_sock);
    zmq_ctx_term(zmq_ctx);
    uv_loop_close(&loop);
    
    printf("Test 3: Done!\n");
    fflush(stdout);
    
    return 0;
}