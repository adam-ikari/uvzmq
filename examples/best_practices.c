#include "../include/uvzmq.h"
#include <zmq.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

static volatile int keep_running = 1;

static void signal_handler(int sig)
{
    (void)sig;
    printf("\n[INFO] Received signal, shutting down...\n");
    keep_running = 0;
}

// Example: Proper resource management
int main(void)
{
    printf("========================================\n");
    printf("UVZMQ Best Practices Example\n");
    printf("========================================\n\n");
    fflush(stdout);
    
    // Setup signal handler for graceful shutdown
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    printf("[INFO] Signal handlers installed\n");
    fflush(stdout);
    
    // Create libuv loop
    uv_loop_t loop;
    uv_loop_init(&loop);
    printf("[INFO] libuv loop initialized\n");
    fflush(stdout);
    
    // Create ZMQ context
    void *zmq_ctx = zmq_ctx_new();
    printf("[INFO] ZMQ context created\n");
    fflush(stdout);
    
    // Create ZMQ socket
    void *zmq_sock = zmq_socket(zmq_ctx, ZMQ_REP);
    printf("[INFO] ZMQ REP socket created\n");
    fflush(stdout);
    
    // Set socket options for better performance
    int timeout = 5000;  // 5 second timeout
    zmq_setsockopt(zmq_sock, ZMQ_RCVTIMEO, &timeout, sizeof(timeout));
    
    int rcvbuf = 1024 * 1024;  // 1MB receive buffer
    int sndbuf = 1024 * 1024;  // 1MB send buffer
    zmq_setsockopt(zmq_sock, ZMQ_RCVBUF, &rcvbuf, sizeof(rcvbuf));
    zmq_setsockopt(zmq_sock, ZMQ_SNDBUF, &sndbuf, sizeof(sndbuf));
    printf("[INFO] Socket options configured\n");
    fflush(stdout);
    
    // Bind socket
    const char *endpoint = "tcp://*:5555";
    zmq_bind(zmq_sock, endpoint);
    printf("[INFO] Socket bound to %s\n", endpoint);
    fflush(stdout);
    
    // Create UVZMQ socket (this integrates ZMQ with libuv)
    uvzmq_socket_t *uvzmq_sock = NULL;
    int received_count = 0;
    
    // Callback: This is called when data arrives
    void on_recv(uvzmq_socket_t *socket, zmq_msg_t *msg, void *user_data)
    {
        (void)socket;
        int *count = (int *)user_data;
        
        // Process the message
        size_t size = zmq_msg_size(msg);
        const char *content = (const char *)zmq_msg_data(msg);
        
        printf("[RECV] Received %zu bytes: %.*s\n", size, (int)size, content);
        fflush(stdout);
        
        // Echo back the message (zero-copy: reuse the same zmq_msg_t)
        zmq_msg_send(msg, uvzmq_get_zmq_socket(socket), 0);
        
        // IMPORTANT: Always close the message after processing
        // This is required to avoid memory leaks!
        zmq_msg_close(msg);
        
        (*count)++;
    };
    
    printf("[INFO] Creating UVZMQ socket...\n");
    fflush(stdout);
    int rc = uvzmq_socket_new(&loop, zmq_sock, on_recv, NULL, 
                                 &received_count, &uvzmq_sock);
    if (rc != UVZMQ_OK) {
        fprintf(stderr, "[ERROR] Failed to create uvzmq socket: %s\n", 
                uvzmq_strerror_last());
        fflush(stderr);
        
        // Clean up resources on error
        zmq_close(zmq_sock);
        zmq_ctx_term(zmq_ctx);
        uv_loop_close(&loop);
        return 1;
    }
    printf("[INFO] UVZMQ socket created successfully\n");
    fflush(stdout);
    
    printf("\n[INFO] Server running on %s\n", endpoint);
    printf("[INFO] Press Ctrl+C to stop\n\n");
    fflush(stdout);
    
    // Run event loop
    while (keep_running) {
        uv_run(&loop, UV_RUN_ONCE);
        usleep(10000);  // 10ms sleep to prevent busy waiting
    }
    
    printf("\n[INFO] Shutting down...\n");
    fflush(stdout);
    
    // Proper cleanup order (IMPORTANT!)
    // 1. Free UVZMQ socket (stops libuv polling)
    uvzmq_socket_free(uvzmq_sock);
    printf("[INFO] UVZMQ socket freed\n");
    
    // 2. Close ZMQ socket
    zmq_close(zmq_sock);
    printf("[INFO] ZMQ socket closed\n");
    
    // 3. Terminate ZMQ context
    zmq_ctx_term(zmq_ctx);
    printf("[INFO] ZMQ context terminated\n");
    
    // 4. Close libuv loop
    uv_loop_close(&loop);
    printf("[INFO] libuv loop closed\n");
    
    printf("\n[INFO] Total messages received: %d\n", received_count);
    printf("[INFO] Shutdown complete\n");
    
    return 0;
}