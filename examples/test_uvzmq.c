#define UVZMQ_IMPLEMENTATION
#include "../include/uvzmq.h"
#include <zmq.h>
#include <stdio.h>

int main(void)
{
    printf("Starting test...\n");
    fflush(stdout);
    
    uv_loop_t loop;
    uv_loop_init(&loop);
    printf("Loop initialized\n");
    fflush(stdout);
    
    void *zmq_ctx = zmq_ctx_new();
    void *zmq_sock = zmq_socket(zmq_ctx, ZMQ_REP);
    zmq_bind(zmq_sock, "tcp://*:6001");
    printf("Socket bound\n");
    fflush(stdout);
    
    void on_recv(uvzmq_socket_t *socket, zmq_msg_t *msg, void *user_data)
    {
        (void)socket;
        (void)msg;
        (void)user_data;
        printf("on_recv called\n");
        fflush(stdout);
    }
    
    printf("About to call uvzmq_socket_new...\n");
    fflush(stdout);
    
    uvzmq_socket_t *uvzmq_sock = NULL;
    int rc = uvzmq_socket_new(&loop, zmq_sock, on_recv, NULL, NULL, &uvzmq_sock);
    printf("uvzmq_socket_new returned: %d\n", rc);
    if (rc != UVZMQ_OK) {
        printf("Error: %s\n", uvzmq_strerror(rc));
    }
    fflush(stdout);
    
    uvzmq_socket_free(uvzmq_sock);
    zmq_close(zmq_sock);
    zmq_ctx_term(zmq_ctx);
    uv_loop_close(&loop);
    
    printf("Done!\n");
    return 0;
}