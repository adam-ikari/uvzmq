#include "../include/uvzmq.h"
#include <zmq.h>
#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    printf("Step 1: Create loop\n");
    fflush(stdout);
    uv_loop_t loop;
    uv_loop_init(&loop);
    
    printf("Step 2: Create ZMQ socket\n");
    fflush(stdout);
    void *zmq_ctx = zmq_ctx_new();
    void *zmq_sock = zmq_socket(zmq_ctx, ZMQ_REP);
    
    printf("Step 3: Bind socket\n");
    fflush(stdout);
    zmq_bind(zmq_sock, "tcp://*:5801");
    
    printf("Step 4: Create uvzmq socket\n");
    fflush(stdout);
    uvzmq_socket_t *uvzmq_sock = NULL;
    int rc = uvzmq_socket_new(&loop, zmq_sock, NULL, NULL, NULL, &uvzmq_sock);
    printf("Step 4 result: %d\n", rc);
    fflush(stdout);
    
    printf("Step 5: Free uvzmq socket\n");
    fflush(stdout);
    uvzmq_socket_free(uvzmq_sock);
    
    printf("Step 6: Cleanup\n");
    fflush(stdout);
    zmq_close(zmq_sock);
    zmq_ctx_term(zmq_ctx);
    uv_loop_close(&loop);
    
    printf("Done!\n");
    fflush(stdout);
    
    return 0;
}