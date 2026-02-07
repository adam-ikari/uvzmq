# UVZMQ Tutorial

A comprehensive guide to using UVZMQ for integrating ZeroMQ with libuv.

## Table of Contents

1. [Introduction](#introduction)
2. [Installation](#installation)
3. [Basic Usage](#basic-usage)
4. [Common Patterns](#common-patterns)
5. [Best Practices](#best-practices)
6. [Troubleshooting](#troubleshooting)

---

## Introduction

UVZMQ is a minimal library that bridges ZeroMQ and libuv, enabling event-driven messaging with ZMQ sockets. It provides only the essential integration layer while giving you full access to the ZMQ API.

### Key Concepts

- **Event-Driven**: Uses libuv's `uv_poll` for efficient I/O
- **Batch Processing**: Processes up to 1000 messages per event
- **Zero-Copy**: Compatible with ZMQ's zero-copy messaging
- **Header-Only**: Single file integration, no build dependencies

---

## Installation

### Option 1: Clone and Use

```bash
git clone https://github.com/yourusername/uvzmq.git
cd uvzmq
```

### Option 2: Copy Header File

Copy `include/uvzmq.h` to your project directory.

### Requirements

- C99 compatible compiler (GCC, Clang)
- libuv (v1.x)
- ZeroMQ (v4.x)

---

## Basic Usage

### Step 1: Include the Header

In **ONE** of your source files:

```c
#define UVZMQ_IMPLEMENTATION
#include "uvzmq.h"
```

In other source files:

```c
#include "uvzmq.h"
```

### Step 2: Create a Basic Server

```c
#include "uvzmq.h"
#include <zmq.h>
#include <uv.h>
#include <stdio.h>
#include <string.h>

// Callback function - called when messages arrive
void on_recv(uvzmq_socket_t* socket, zmq_msg_t* msg, void* user_data) {
    // Get message data
    size_t size = zmq_msg_size(msg);
    const char* data = (const char*)zmq_msg_data(msg);
    
    printf("Received: %.*s\n", (int)size, data);
    
    // Echo back (zero-copy)
    zmq_msg_send(msg, uvzmq_get_zmq_socket(socket), 0);
    
    // IMPORTANT: Close the message
    zmq_msg_close(msg);
}

int main(void) {
    // Initialize libuv
    uv_loop_t loop;
    uv_loop_init(&loop);
    
    // Create ZMQ context and socket
    void* zmq_ctx = zmq_ctx_new();
    void* zmq_sock = zmq_socket(zmq_ctx, ZMQ_REP);
    zmq_bind(zmq_sock, "tcp://*:5555");
    
    // Integrate with libuv
    uvzmq_socket_t* uvzmq_sock = NULL;
    int rc = uvzmq_socket_new(&loop, zmq_sock, on_recv, NULL, &uvzmq_sock);
    if (rc != 0) {
        fprintf(stderr, "Failed to create UVZMQ socket\n");
        return 1;
    }
    
    // Run event loop
    printf("Server running on tcp://*:5555\n");
    uv_run(&loop, UV_RUN_DEFAULT);
    
    // Cleanup
    uvzmq_socket_free(uvzmq_sock);
    zmq_close(zmq_sock);
    zmq_ctx_term(zmq_ctx);
    uv_loop_close(&loop);
    
    return 0;
}
```

### Step 3: Create a Basic Client

```c
#include <zmq.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    void* context = zmq_ctx_new();
    void* socket = zmq_socket(context, ZMQ_REQ);
    zmq_connect(socket, "tcp://localhost:5555");
    
    // Send message
    const char* msg = "Hello from client";
    zmq_send(socket, msg, strlen(msg), 0);
    
    // Receive reply
    char buffer[256];
    int size = zmq_recv(socket, buffer, sizeof(buffer) - 1, 0);
    if (size > 0) {
        buffer[size] = '\0';
        printf("Received: %s\n", buffer);
    }
    
    // Cleanup
    zmq_close(socket);
    zmq_ctx_term(context);
    
    return 0;
}
```

---

## Common Patterns

### Pattern 1: Request-Reply (REQ/REP)

See `examples/req_rep.c` for a complete example.

```c
// Server
void* server_sock = zmq_socket(ctx, ZMQ_REP);
zmq_bind(server_sock, "tcp://*:5555");

// Client
void* client_sock = zmq_socket(ctx, ZMQ_REQ);
zmq_connect(client_sock, "tcp://localhost:5555");
```

### Pattern 2: Publish-Subscribe (PUB/SUB)

See `examples/pub_sub.c` for a complete example.

```c
// Publisher
void* pub_sock = zmq_socket(ctx, ZMQ_PUB);
zmq_bind(pub_sock, "tcp://*:5556");

// Subscriber
void* sub_sock = zmq_socket(ctx, ZMQ_SUB);
zmq_connect(sub_sock, "tcp://localhost:5556");
zmq_setsockopt(sub_sock, ZMQ_SUBSCRIBE, "", 0);  // Subscribe to all
```

### Pattern 3: Push-Pull (PUSH/PULL)

See `examples/test_push_pull.c` for a complete example.

```c
// Pusher
void* push_sock = zmq_socket(ctx, ZMQ_PUSH);
zmq_bind(push_sock, "tcp://*:5557");

// Puller
void* pull_sock = zmq_socket(ctx, ZMQ_PULL);
zmq_connect(pull_sock, "tcp://localhost:5557");
```

### Pattern 4: Multi-Threaded

See `examples/multi_thread.c` for a complete example.

Each thread must have its own `uvzmq_socket_t` instance.

---

## Best Practices

### 1. Always Close Messages

```c
void on_recv(uvzmq_socket_t* socket, zmq_msg_t* msg, void* user_data) {
    // Process message
    zmq_msg_send(msg, uvzmq_get_zmq_socket(socket), 0);
    
    // REQUIRED: Close to avoid memory leak
    zmq_msg_close(msg);
}
```

### 2. Proper Cleanup Order

```c
// Correct order:
uvzmq_socket_free(uvzmq_sock);  // 1. Free UVZMQ
zmq_close(zmq_sock);              // 2. Close ZMQ socket
zmq_ctx_term(zmq_ctx);            // 3. Terminate context
uv_loop_close(&loop);             // 4. Close loop
```

### 3. Error Handling

```c
int rc = uvzmq_socket_new(&loop, zmq_sock, on_recv, NULL, &uvzmq_sock);
if (rc != 0) {
    // Check system error
    perror("uvzmq_socket_new");
    
    // Or check ZMQ error
    int zmq_err = zmq_errno();
    fprintf(stderr, "ZMQ error: %s\n", zmq_strerror(zmq_err));
    
    return 1;
}
```

### 4. Thread Safety

Each `uvzmq_socket_t` must be used by a single thread only:

```c
// Thread 1
uvzmq_socket_t* socket1 = NULL;
uvzmq_socket_new(&loop1, zmq_sock1, callback1, NULL, &socket1);

// Thread 2
uvzmq_socket_t* socket2 = NULL;
uvzmq_socket_new(&loop2, zmq_sock2, callback2, NULL, &socket2);

// NEVER share socket1 between threads
```

### 5. Use Zero-Copy for Performance

```c
void on_recv(uvzmq_socket_t* socket, zmq_msg_t* msg, void* user_data) {
    // Reuse message instead of copying
    zmq_msg_send(msg, uvzmq_get_zmq_socket(socket), 0);
    
    // Still need to close after send
    zmq_msg_close(msg);
}
```

---

## Troubleshooting

### Issue: "Segmentation fault"

**Cause**: Probably double-free or use-after-free.

**Solution**: Ensure proper cleanup order and message closing:

```c
// Always close messages
zmq_msg_close(msg);

// Always free socket before ZMQ socket
uvzmq_socket_free(uvzmq_sock);
zmq_close(zmq_sock);
```

### Issue: "No messages received"

**Cause**: Missing subscription or wrong socket type.

**Solution**:

```c
// For PUB/SUB, always subscribe:
zmq_setsockopt(sub_sock, ZMQ_SUBSCRIBE, "", 0);

// Check socket type matches:
// REQ ↔ REP
// PUB ↔ SUB
// PUSH ↔ PULL
```

### Issue: "Memory leak"

**Cause**: Not closing `zmq_msg_t` in callback.

**Solution**:

```c
void on_recv(uvzmq_socket_t* socket, zmq_msg_t* msg, void* user_data) {
    // Process message
    // ...
    
    // MUST close!
    zmq_msg_close(msg);
}
```

### Issue: "uvzmq_socket_new returns -1"

**Cause**: Invalid parameters or initialization failure.

**Solution**:

```c
// Check parameters
if (!loop || !zmq_sock || !socket) {
    fprintf(stderr, "Invalid parameters\n");
    return;
}

// Check error details
if (uvzmq_socket_new(loop, zmq_sock, on_recv, NULL, &socket) != 0) {
    perror("uvzmq_socket_new");
    // Check ZMQ errors
    fprintf(stderr, "ZMQ error: %s\n", zmq_strerror(zmq_errno()));
}
```

---

## Performance Tips

### 1. Use Batch Processing

UVZMQ automatically batches up to 1000 messages per event. For very high throughput, you can adjust the constants:

```c
#define UVZMQ_MAX_BATCH_SIZE 2000  // Increase batch size
#define UVZMQ_BATCH_CHECK_INTERVAL 100  // Less frequent checks
```

### 2. Optimize Socket Options

```c
// Set high water mark for better throughput
int hwm = 10000;
zmq_setsockopt(sock, ZMQ_SNDHWM, &hwm, sizeof(hwm));
zmq_setsockopt(sock, ZMQ_RCVHWM, &hwm, sizeof(hwm));
```

### 3. Use Zero-Copy

Avoid copying message data:

```c
// Good: Zero-copy
zmq_msg_send(msg, socket, 0);

// Bad: Copy data
char* data = malloc(size);
memcpy(data, zmq_msg_data(msg), size);
zmq_send(socket, data, size, 0);
free(data);
```

---

## Additional Resources

- **API Documentation**: Run `make docs` to generate API docs
- **Examples**: See `examples/` directory
- **Tests**: See `tests/` directory for unit tests
- **ZMQ Guide**: http://zguide.zeromq.org/
- **libuv Guide**: https://docs.libuv.org/

---

## Contributing

Contributions are welcome! Please ensure:

- Code follows C99 standard
- All functions have proper error checking
- Examples demonstrate best practices
- Tests cover new features