#include "../include/uvzmq.h"
#include <zmq.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

struct thread_data {
    uv_loop_t *loop;
    int *message_count;
    int max_messages;
};

static void *loop_thread_func(void *arg)
{
    struct thread_data *data = (struct thread_data *)arg;
    
    for (int i = 0; i < 200 && *data->message_count < data->max_messages; i++) {
        uv_run(data->loop, UV_RUN_ONCE);
        usleep(10000); // 10ms
    }
    
    return NULL;
}

static int message_count = 0;

static void on_recv(uvzmq_socket_t *socket, zmq_msg_t *msg, void *user_data)
{
    (void)socket;
    (void)user_data;
    size_t size = zmq_msg_size(msg);
    printf("Received message #%d: %.*s\n", ++message_count, (int)size, (char *)zmq_msg_data(msg));
    zmq_msg_close(msg);
}

int main(void)
{
    printf("UVZMQ PUB/SUB Example\n");
    printf("=====================\n\n");

    uv_loop_t loop;
    uv_loop_init(&loop);

    // Create ZMQ context and sockets (use same context for PUB and SUB)
    void *zmq_ctx = zmq_ctx_new();

    // Create SUB socket first
    void *sub_sock = zmq_socket(zmq_ctx, ZMQ_SUB);
    
    // Set subscription before connecting
    zmq_setsockopt(sub_sock, ZMQ_SUBSCRIBE, "", 0);
    
    // Set receive timeout to avoid blocking
    int timeout = 1000;
    zmq_setsockopt(sub_sock, ZMQ_RCVTIMEO, &timeout, sizeof(timeout));
    
    zmq_connect(sub_sock, "inproc://test");
    
    printf("SUB socket created and subscribed\n");

    // Create PUB socket
    void *pub_sock = zmq_socket(zmq_ctx, ZMQ_PUB);
    zmq_bind(pub_sock, "inproc://test");

    // Integrate SUB socket with libuv
    uvzmq_socket_t *uvzmq_sub = NULL;
    uvzmq_socket_new(&loop, sub_sock, on_recv, NULL, NULL, &uvzmq_sub);

printf("SUB socket created and subscribed\n");
    printf("PUB socket bound to inproc://test\n");
    printf("SUB socket connected to inproc://test\n");
    printf("Starting event loop thread...\n");
    
    // Run event loop in background thread
    struct thread_data thread_data;
    thread_data.loop = &loop;
    thread_data.message_count = &message_count;
    thread_data.max_messages = 10;
    
    pthread_t thread;
    pthread_create(&thread, NULL, loop_thread_func, &thread_data);
    
    // Wait a bit for event loop to start
    usleep(100000); // 100ms
    
    printf("Sending 10 messages...\n\n");

    // Send messages using standard ZMQ API
    for (int i = 0; i < 10; i++) {
        char msg[64];
        snprintf(msg, sizeof(msg), "Message %d", i);
        zmq_send(pub_sock, msg, strlen(msg), 0);
        printf("Sent: Message %d\n", i);
        usleep(100000); // 100ms delay
    }

    pthread_join(thread, NULL);

    // Cleanup
    uvzmq_socket_free(uvzmq_sub);
    zmq_close(pub_sock);
    zmq_close(sub_sock);
    zmq_ctx_term(zmq_ctx);
    uv_loop_close(&loop);

    printf("\nDone! Received %d messages\n", message_count);
    return 0;
}