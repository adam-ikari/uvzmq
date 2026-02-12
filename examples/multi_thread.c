#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <zmq.h>

#include "uvzmq.h"

// Server thread data
struct server_data {
    uv_loop_t* loop;
    int port;
    int message_count;
    int* received_count;
};

// Client thread data
struct client_data {
    uv_loop_t* loop;
    int port;
    int message_count;
};

static int server_received = 0;

// Server: REP socket
static void server_on_recv(uvzmq_socket_t* socket,
                           zmq_msg_t* msg,
                           void* user_data) {
    struct server_data* data = (struct server_data*)user_data;

    size_t size = zmq_msg_size(msg);
    const char* content = (const char*)zmq_msg_data(msg);

    printf("[SERVER] Received: %.*s\n", (int)size, content);

    // Echo back the message
    zmq_msg_send(msg, uvzmq_get_zmq_socket(socket), 0);
    zmq_msg_close(msg);

    (*data->received_count)++;
}

static void* server_thread_func(void* arg) {
    struct server_data* data = (struct server_data*)arg;

    printf("[SERVER] Starting REP server on port %d\n", data->port);

    void* zmq_ctx = zmq_ctx_new();
    void* zmq_sock = zmq_socket(zmq_ctx, ZMQ_REP);

    char bind_addr[64];
    snprintf(bind_addr, sizeof(bind_addr), "tcp://*:%d", data->port);
    zmq_bind(zmq_sock, bind_addr);

    uvzmq_socket_t* uvzmq_sock = NULL;
    uvzmq_socket_new(data->loop, zmq_sock, server_on_recv, data, &uvzmq_sock);

    printf("[SERVER] Ready to receive messages\n");

    // Run event loop
    for (int i = 0; i < 200 && *data->received_count < data->message_count;
         i++) {
        uv_run(data->loop, UV_RUN_ONCE);
        usleep(10000);  // 10ms
    }

    uvzmq_socket_free(uvzmq_sock);
    zmq_close(zmq_sock);
    zmq_ctx_term(zmq_ctx);

    printf("[SERVER] Received %d messages\n", *data->received_count);
    return NULL;
}

// Client: REQ socket
static void* client_thread_func(void* arg) {
    struct client_data* data = (struct client_data*)arg;

    printf("[CLIENT] Starting REQ client\n");
    usleep(200000);  // Wait for server to start

    void* zmq_ctx = zmq_ctx_new();
    void* zmq_sock = zmq_socket(zmq_ctx, ZMQ_REQ);

    int timeout = 5000;
    zmq_setsockopt(zmq_sock, ZMQ_RCVTIMEO, &timeout, sizeof(timeout));

    char connect_addr[64];
    snprintf(
        connect_addr, sizeof(connect_addr), "tcp://127.0.0.1:%d", data->port);
    zmq_connect(zmq_sock, connect_addr);

    printf("[CLIENT] Sending %d messages...\n", data->message_count);

    for (int i = 0; i < data->message_count; i++) {
        char msg[64];
        snprintf(msg, sizeof(msg), "Request %d", i);

        printf("[CLIENT] Sending: %s\n", msg);
        zmq_send(zmq_sock, msg, strlen(msg), 0);

        zmq_msg_t reply;
        zmq_msg_init(&reply);

        int rc = zmq_msg_recv(&reply, zmq_sock, 0);
        if (rc >= 0) {
            size_t size = zmq_msg_size(&reply);
            printf("[CLIENT] Reply: %.*s\n",
                   (int)size,
                   (char*)zmq_msg_data(&reply));
        } else {
            printf("[CLIENT] No reply received\n");
        }

        zmq_msg_close(&reply);
        usleep(100000);  // 100ms delay
    }

    zmq_close(zmq_sock);
    zmq_ctx_term(zmq_ctx);

    printf("[CLIENT] Finished\n");
    return NULL;
}

// Subscriber
static void sub_on_recv(uvzmq_socket_t* socket,
                        zmq_msg_t* msg,
                        void* user_data) {
    (void)socket;
    int* count = (int*)user_data;

    size_t size = zmq_msg_size(msg);
    printf("[SUB] Received: %.*s\n", (int)size, (char*)zmq_msg_data(msg));

    (*count)++;
    zmq_msg_close(msg);
}

// Publisher thread
struct pub_data {
    uv_loop_t* loop;
    int message_count;
    int* sub_received;
};

static void* pub_thread_func(void* arg) {
    struct pub_data* data = (struct pub_data*)arg;

    printf("[PUB] Starting publisher\n");
    usleep(200000);  // Wait for subscriber

    void* zmq_ctx = zmq_ctx_new();
    void* pub_sock = zmq_socket(zmq_ctx, ZMQ_PUB);
    zmq_bind(pub_sock, "inproc://pubsub_test");

    printf("[PUB] Sending %d messages...\n", data->message_count);

    for (int i = 0; i < data->message_count; i++) {
        char msg[64];
        snprintf(msg, sizeof(msg), "News %d", i);
        zmq_send(pub_sock, msg, strlen(msg), 0);
        printf("[PUB] Sent: %s\n", msg);
        usleep(100000);  // 100ms
    }

    zmq_close(pub_sock);
    zmq_ctx_term(zmq_ctx);

    printf("[PUB] Finished\n");
    return NULL;
}

// Subscriber thread
static void* sub_thread_func(void* arg) {
    struct pub_data* data = (struct pub_data*)arg;

    printf("[SUB] Starting subscriber\n");

    void* zmq_ctx = zmq_ctx_new();
    void* sub_sock = zmq_socket(zmq_ctx, ZMQ_SUB);
    zmq_setsockopt(sub_sock, ZMQ_SUBSCRIBE, "", 0);
    zmq_connect(sub_sock, "inproc://pubsub_test");

    uvzmq_socket_t* uvzmq_sock = NULL;
    uvzmq_socket_new(
        data->loop, sub_sock, sub_on_recv, data->sub_received, &uvzmq_sock);

    printf("[SUB] Listening for messages\n");

    // Run event loop
    for (int i = 0; i < 200 && *data->sub_received < data->message_count; i++) {
        uv_run(data->loop, UV_RUN_ONCE);
        usleep(10000);  // 10ms
    }

    uvzmq_socket_free(uvzmq_sock);
    zmq_close(sub_sock);
    zmq_ctx_term(zmq_ctx);

    printf("[SUB] Received %d messages\n", *data->sub_received);
    return NULL;
}

int main(void) {
    printf("========================================\n");
    printf("UVZMQ Real-World Multi-Thread Test\n");
    printf("========================================\n\n");

    // Test 1: REQ/REP with separate loops and threads
    printf("=== Test 1: REQ/REP (Server + Client) ===\n");

    uv_loop_t server_loop, client_loop;
    uv_loop_init(&server_loop);
    uv_loop_init(&client_loop);

    struct server_data server_data = {.loop = &server_loop,
                                      .port = 6001,
                                      .message_count = 5,
                                      .received_count = &server_received};

    struct client_data client_data = {
        .loop = &client_loop, .port = 6001, .message_count = 5};

    pthread_t server_thread, client_thread;
    pthread_create(&server_thread, NULL, server_thread_func, &server_data);
    pthread_create(&client_thread, NULL, client_thread_func, &client_data);

    pthread_join(server_thread, NULL);
    pthread_join(client_thread, NULL);

    printf("\n✅ REQ/REP test completed\n\n");

    // Test 2: PUB/SUB with separate loops and threads
    printf("=== Test 2: PUB/SUB (Publisher + Subscriber) ===\n");

    uv_loop_t pub_loop, sub_loop;
    uv_loop_init(&pub_loop);
    uv_loop_init(&sub_loop);

    int sub_received = 0;

    struct pub_data pub_data = {
        .loop = &pub_loop, .message_count = 5, .sub_received = &sub_received};

    struct pub_data sub_data = {
        .loop = &sub_loop, .message_count = 5, .sub_received = &sub_received};

    pthread_t pub_thread, sub_thread;
    pthread_create(&pub_thread, NULL, pub_thread_func, &pub_data);
    pthread_create(&sub_thread, NULL, sub_thread_func, &sub_data);

    pthread_join(pub_thread, NULL);
    pthread_join(sub_thread, NULL);

    printf("\n✅ PUB/SUB test completed\n");

    // Cleanup
    uv_loop_close(&server_loop);
    uv_loop_close(&client_loop);
    uv_loop_close(&pub_loop);
    uv_loop_close(&sub_loop);

    printf("\n========================================\n");
    printf("All tests completed successfully!\n");
    printf("========================================\n");

    return 0;
}
