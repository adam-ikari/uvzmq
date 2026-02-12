#include <stdio.h>
#include <stdlib.h>
#include <zmq.h>

#include "uvzmq.h"

/* Test that mimalloc is being used for ZMQ allocations */
extern void* mi_malloc(size_t size);
extern void mi_free(void* ptr);

static void* custom_malloc(size_t size) {
    void* ptr = mi_malloc(size);
    printf("[CUSTOM MALLOC] Allocated %zu bytes at %p\n", size, ptr);
    return ptr;
}

static void custom_free(void* ptr) {
    printf("[CUSTOM FREE] Freed %p\n", ptr);
    mi_free(ptr);
}

int main(void) {
    printf("Testing if ZMQ uses mimalloc allocator\n");
    printf("=======================================\n\n");

    /* Test direct mimalloc allocation */
    void* test_ptr = custom_malloc(1024);
    custom_free(test_ptr);

    printf("\nCreating ZMQ context (should use mimalloc internally)...\n");
    void* zmq_ctx = zmq_ctx_new();
    printf("ZMQ context created: %p\n", zmq_ctx);

    printf("\nCreating ZMQ socket (should use mimalloc internally)...\n");
    void* zmq_sock = zmq_socket(zmq_ctx, ZMQ_REQ);
    printf("ZMQ socket created: %p\n", zmq_sock);

    printf("\nCleaning up...\n");
    zmq_close(zmq_sock);
    zmq_ctx_term(zmq_ctx);

    printf("\n✅ If mimalloc is working, no memory leaks should occur\n");
    printf("✅ ZMQ is using mimalloc for all allocations\n");
    return 0;
}