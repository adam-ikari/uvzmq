#include "../include/uvzmq.h"
#include <zmq.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

struct test_data {
    int port;
    int received_count;
};

static void server_on_recv(uvzmq_socket_t *socket, zmq_msg_t *msg, void *user_data)
{
    (void)socket;
    int *count = (int *)user_data;
    size_t size = zmq_msg_size(msg);
    const char *content = (const char *)zmq_msg_data(msg);
    printf("[SERVER] Received: %.*s\n", (int)size, content);
    zmq_msg_send(msg, uvzmq_get_zmq_socket(socket), 0);
    zmq_msg_close(msg);
    (*count)++;
}

static void timer_callback(uv_timer_t *handle)
{
    (void)handle;
    uv_stop((uv_loop_t *)handle->loop);
}

static void *client_thread(void *arg)
{
    (void)arg;
    usleep(100000);  // Wait for server
    
    void *zmq_ctx = zmq_ctx_new();
    void *zmq_sock = zmq_socket(zmq_ctx, ZMQ_REQ);
    zmq_connect(zmq_sock, "tcp://127.0.0.1:5601");
    
    printf("[CLIENT] Sending message...\n");
    zmq_send(zmq_sock, "Test", 4, 0);
    
    zmq_msg_t reply;
    zmq_msg_init(&reply);
    zmq_msg_recv(&reply, zmq_sock, 0);
    zmq_msg_close(&reply);
    
    printf("[CLIENT] Received reply\n");
    
    zmq_close(zmq_sock);
    zmq_ctx_term(zmq_ctx);
    return NULL;
}

int main(void)
{
    printf("========================================\n");
    printf("Testing libuv loop compatibility\n");
    printf("========================================\n\n");
    
    // Test with UV_RUN_DEFAULT (with timeout)
    printf("=== Test 1: UV_RUN_DEFAULT ===\n");
    {
        uv_loop_t loop;
        uv_loop_init(&loop);
        
        void *zmq_ctx = zmq_ctx_new();
        void *zmq_sock = zmq_socket(zmq_ctx, ZMQ_REP);
        zmq_bind(zmq_sock, "tcp://*:5601");
        
        int received = 0;
        uvzmq_socket_t *uvzmq_sock = NULL;
        uvzmq_socket_new(&loop, zmq_sock, server_on_recv, NULL, &received, &uvzmq_sock);
        
        // Start client thread
        pthread_t client;
        pthread_create(&client, NULL, client_thread, NULL);
        
        // Run with timeout
        printf("[TEST1] Running UV_RUN_DEFAULT for 3 seconds...\n");
        uv_timer_t timer;
        uv_timer_init(&loop, &timer);
        uv_timer_start(&timer, timer_callback, 3000, 0);
        
        uv_run(&loop, UV_RUN_DEFAULT);
        
        pthread_join(client, NULL);
        
        uvzmq_socket_free(uvzmq_sock);
        zmq_close(zmq_sock);
        zmq_ctx_term(zmq_ctx);
        uv_loop_close(&loop);
        
        printf("[TEST1] Received %d messages\n\n", received);
    }
    
    // Test with UV_RUN_ONCE
    printf("=== Test 2: UV_RUN_ONCE ===\n");
    {
        uv_loop_t loop;
        uv_loop_init(&loop);
        
        void *zmq_ctx = zmq_ctx_new();
        void *zmq_sock = zmq_socket(zmq_ctx, ZMQ_REP);
        zmq_bind(zmq_sock, "tcp://*:5602");
        
        int received = 0;
        uvzmq_socket_t *uvzmq_sock = NULL;
        uvzmq_socket_new(&loop, zmq_sock, server_on_recv, NULL, &received, &uvzmq_sock);
        
        // Start client thread
        pthread_t client;
        pthread_create(&client, NULL, client_thread, NULL);
        
        // Run multiple times
        printf("[TEST2] Running UV_RUN_ONCE for 2 seconds...\n");
        for (int i = 0; i < 200 && received < 1; i++) {
            uv_run(&loop, UV_RUN_ONCE);
            if (i < 10) {
                printf("[TEST2] Iteration %d, received: %d\n", i, received);
            }
            usleep(10000);
        }
        
        pthread_join(client, NULL);
        
        uvzmq_socket_free(uvzmq_sock);
        zmq_close(zmq_sock);
        zmq_ctx_term(zmq_ctx);
        uv_loop_close(&loop);
        
        printf("[TEST2] Received %d messages\n\n", received);
    }
    
    // Test with UV_RUN_NOWAIT
    printf("=== Test 3: UV_RUN_NOWAIT ===\n");
    {
        uv_loop_t loop;
        uv_loop_init(&loop);
        
        void *zmq_ctx = zmq_ctx_new();
        void *zmq_sock = zmq_socket(zmq_ctx, ZMQ_REP);
        zmq_bind(zmq_sock, "tcp://*:5603");
        
        int received = 0;
        uvzmq_socket_t *uvzmq_sock = NULL;
        uvzmq_socket_new(&loop, zmq_sock, server_on_recv, NULL, &received, &uvzmq_sock);
        
        // Start client thread
        pthread_t client;
        pthread_create(&client, NULL, client_thread, NULL);
        
        // Run multiple times
        printf("[TEST3] Running UV_RUN_NOWAIT for 2 seconds...\n");
        for (int i = 0; i < 400 && received < 1; i++) {
            uv_run(&loop, UV_RUN_NOWAIT);
            uv_run(&loop, UV_RUN_NOWAIT);  // Run twice
            if (i < 10) {
                printf("[TEST3] Iteration %d, received: %d\n", i, received);
            }
            usleep(5000);
        }
        
        pthread_join(client, NULL);
        
        uvzmq_socket_free(uvzmq_sock);
        zmq_close(zmq_sock);
        zmq_ctx_term(zmq_ctx);
        uv_loop_close(&loop);
        
        printf("[TEST3] Received %d messages\n\n", received);
    }
    
    printf("========================================\n");
    printf("All tests completed\n");
    printf("========================================\n");
    
    return 0;
}