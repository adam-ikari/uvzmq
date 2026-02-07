# UVZMQ

[中文文档](README_CN.md) | [教程](TUTORIAL.md)

Minimal libuv integration for ZeroMQ.

UVZMQ provides **one thing only**: integrating ZMQ sockets with libuv event loop using `uv_poll`. All other ZMQ operations (send, recv, poll, setsockopt, etc.) should be used directly from the ZMQ API.

## Features

- ✅ **Minimal API** - Only essential functions needed
- ✅ **Event-driven** - Uses libuv's `uv_poll` for efficient I/O
- ✅ **Batch processing** - Optimized for high-throughput scenarios
- ✅ **Zero-copy support** - Compatible with ZMQ's zero-copy messaging
- ✅ **C99 standard** - Works with GCC and Clang compilers

## Quick Start

UVZMQ is a **header-only library**. Include the header file in one of your source files with the implementation macro:

```c
// In ONE of your source files
#define UVZMQ_IMPLEMENTATION
#include "uvzmq.h"

// In other source files
#include "uvzmq.h"
```

Then use it in your code:

```c
#include "uvzmq.h"
#include <zmq.h>
#include <uv.h>

// Callback function
void on_recv(uvzmq_socket_t *s, zmq_msg_t *msg, void *data) {
    // Echo back (zero-copy)
    zmq_msg_send(msg, uvzmq_get_zmq_socket(s), 0);

    // IMPORTANT: Close message to avoid memory leak
    zmq_msg_close(msg);
}

int main(void) {
    // Create ZMQ context and socket
    void *zmq_ctx = zmq_ctx_new();
    void *zmq_sock = zmq_socket(zmq_ctx, ZMQ_REP);
    zmq_bind(zmq_sock, "tcp://*:5555");

    // Create libuv loop
    uv_loop_t loop;
    uv_loop_init(&loop);

    // Integrate with libuv
    uvzmq_socket_t *uvzmq_sock = NULL;
    uvzmq_socket_new(&loop, zmq_sock, on_recv, NULL, &uvzmq_sock);

    // Run event loop
    int keep_running = 1;
    while (keep_running) {
        uv_run(&loop, UV_RUN_ONCE);
    }

    // Cleanup
    uvzmq_socket_free(uvzmq_sock);
    zmq_close(zmq_sock);
    zmq_ctx_term(zmq_ctx);
    uv_loop_close(&loop);

    return 0;
}
```

**Important:** Only define `UVZMQ_IMPLEMENTATION` in **ONE** source file. All other files should just include `"uvzmq.h"`.

## Learn More

- 📖 [Tutorial](TUTORIAL.md) - Comprehensive guide with examples and best practices
- 📚 [API Documentation](docs/index.html) - Detailed API reference (run `make docs`)
- 🌐 [中文文档](README_CN.md) | [教程](TUTORIAL_CN.md) - Chinese documentation and tutorial

## Building

### Prerequisites

- C99 compatible compiler (GCC or Clang)
- CMake 3.10 or higher
- libuv (included as submodule)
- ZeroMQ 4.x (included as submodule)

### Build Steps

```bash
git submodule update --init --recursive
mkdir build && cd build
cmake ..
make
```

### Build Options

```bash
# Disable examples
cmake -DUVZMQ_BUILD_EXAMPLES=OFF ..

# Disable benchmarks
cmake -DUVZMQ_BUILD_BENCHMARKS=OFF ..
```

## API Reference

### Core Functions

#### `uvzmq_socket_new`

Initialize UVZMQ socket and integrate with libuv event loop.

```c
int uvzmq_socket_new(uv_loop_t *loop,
                     void *zmq_sock,
                     uvzmq_recv_callback on_recv,
                     void *user_data,
                     uvzmq_socket_t **socket);
```

**Parameters:**

- `loop` - libuv event loop
- `zmq_sock` - existing ZMQ socket
- `on_recv` - callback for when socket is readable
- `user_data` - user data passed to callbacks
- `socket` - [out] pointer to receive the created uvzmq socket

**Returns:** `0` on success, `-1` on failure

**Error Diagnosis:** On failure, you can diagnose errors by:

- Check `errno` for system-level errors
- Call `zmq_errno()` to get ZMQ error code
- Call `zmq_strerror(zmq_errno())` for error message

#### `uvzmq_socket_close`

Stop event handling for the socket without freeing resources.

```c
int uvzmq_socket_close(uvzmq_socket_t *socket);
```

**Note:** This stops libuv from polling the socket, but the socket remains valid. Useful when you want to temporarily stop event handling without destroying the socket. To permanently remove the socket, use `uvzmq_socket_free()`.

#### `uvzmq_socket_free`

Free UVZMQ socket resources.

```c
int uvzmq_socket_free(uvzmq_socket_t *socket);
```

**Note:** This does NOT close the underlying ZMQ socket. You must call `zmq_close()` yourself.

### Utility Functions

#### `uvzmq_get_zmq_socket`

Get the underlying ZMQ socket.

```c
void *uvzmq_get_zmq_socket(uvzmq_socket_t *socket);
```

#### `uvzmq_get_loop`

Get the libuv loop.

```c
uv_loop_t *uvzmq_get_loop(uvzmq_socket_t *socket);
```

#### `uvzmq_get_user_data`

Get user data.

```c
void *uvzmq_get_user_data(uvzmq_socket_t *socket);
```

#### `uvzmq_get_fd`

Get the ZMQ file descriptor.

```c
int uvzmq_get_fd(uvzmq_socket_t *socket);
```

### Error Handling

All functions return `0` on success and `-1` on failure. To diagnose errors:

```c
if (uvzmq_socket_new(loop, zmq_sock, on_recv, NULL, &sock) != 0) {
    // Check system errno
    perror("uvzmq_socket_new");

    // Or check ZMQ errors
    int zmq_err = zmq_errno();
    fprintf(stderr, "ZMQ error: %s\n", zmq_strerror(zmq_err));
}
```

Common error sources:

- Invalid parameters (NULL pointers)
- ZMQ socket not properly initialized
- ZMQ socket not bound/connected
- libuv loop allocation failure

## Thread Safety

UVZMQ is **NOT thread-safe**. Each `uvzmq_socket_t` must be used by a single thread only.

For multi-threaded applications:

- Create separate `uvzmq_socket_t` instances for each thread
- Use separate ZMQ contexts or configure `ZMQ_IO_THREADS` appropriately
- Do NOT share `uvzmq_socket_t` or `zmq_sock` across threads

## Callback Requirements

The `on_recv` callback **MUST** close the `zmq_msg_t` after processing:

```c
void on_recv(uvzmq_socket_t *socket, zmq_msg_t *msg, void *user_data) {
    // Process message
    zmq_msg_send(msg, uvzmq_get_zmq_socket(socket), 0);

    // REQUIRED: Close message to avoid memory leak
    zmq_msg_close(msg);
}
```

## Examples

See the `examples/` directory for complete examples:

- `simple.c` - Basic REQ/REP pattern
- `best_practices.c` - Complete example with signal handling
- `test_*.c` - Various test cases

💡 **Tip**: Check out the [Tutorial](TUTORIAL.md) for detailed usage guides and common patterns.

## Performance

UVZMQ provides significant performance improvements over timer-based polling:

- **Timer-based**: ~11.2ms per message
- **Event-driven (UVZMQ)**: ~0.056ms per message
- **Improvement**: ~200x faster

For large messages (>1KB), UVZMQ achieves performance comparable to native ZMQ with only 5-8% overhead due to libuv callback infrastructure.

## Design Philosophy

UVZMQ follows these principles:

1. **Minimal** - Only provides libuv event loop integration
2. **Direct** - Users interact with ZMQ APIs directly
3. **Transparent** - No hidden abstractions or magic
4. **Efficient** - Zero unnecessary overhead

## License

MIT License

## Contributing

Contributions are welcome! Please ensure:

- Code follows C99 standard
- No C11 features (for wider compatibility)
- All functions have proper error checking
- Examples demonstrate best practices
