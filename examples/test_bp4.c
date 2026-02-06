#define UVZMQ_IMPLEMENTATION
#include "../include/uvzmq.h"
#include <zmq.h>
#include <stdio.h>

int main(void)
{
    printf("Test: Start\n");
    fflush(stdout);
    
    uv_loop_t loop;
    uv_loop_init(&loop);
    printf("Test: Loop initialized\n");
    fflush(stdout);
    
    void *zmq_ctx = zmq_ctx_new();
    void *zmq_sock = zmq_socket(zmq_ctx, ZMQ_REP);
    
    int timeout = 5000;
    zmq_setsockopt(zmq_sock, ZMQ_RCVTIMEO, &timeout, sizeof(timeout));
    int rcvbuf = 1024 * 1024;
    zmq_setsockopt(zmq_sock, ZMQ_RCVBUF, &rcvbuf, sizeof(rcvbuf));
    int sndbuf = 1024 * 1024;
    zmq_setsockopt(zmq_sock, ZMQ_SNDBUF, &sndbuf, sizeof(sndbuf));
    
    zmq_bind(zmq_sock, "tcp://*:6008");
    printf("Test: All ZMQ setup done\n");
    fflush(stdout);
    
    void on_recv(uvzmq_socket_t *socket, zmq_msg_t *msg, void *user_data)
    {
        (void)socket;
        (void)msg;
        (void)user_data;
    }
    
    uvzmq_socket_t *uvzmq_sock = NULL;
    uvzmq_socket_new(&loop, zmq_sock, on_recv, NULL, NULL, &uvzmq_sock);
    printf("Test: UVZMQ socket created\n");
    fflush(stdout);
    
    printf("Test: About to run loop\n");
    fflush(stdout);
    
    uv_run(&loop, UV_RUN_NOWAIT);
    printf("Test: Loop iteration 0 done\n");
    fflush(stdout);
    
    uv_run(&loop, UV_RUN_NOWAIT);
    printf("Test: Loop iteration 1 done\n");
    fflush(stdout);
    
    uv_run(&loop, UV_RUN_NOWAIT);
    printf("Test: Loop iteration 2 done\n");
    fflush(stdout);
    
    uvzmq_socket_free(uvzmq_sock);
    zmq_close(zmq_sock);
    zmq_ctx_term(zmq_ctx);
    uv_loop_close(&loop);
    
    printf("Test: Done!\n");
    return 0;
}