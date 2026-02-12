# UVZMQ

[中文文档](README_CN.md)

Libuv-based ZeroMQ integration with zero internal thread creation.

UVZMQ provides **one thing only**: integrating ZMQ sockets with libuv event loop using `uv_poll`. All other ZMQ operations (send, recv, poll, setsockopt, etc.) should be used directly from the ZMQ API.

## Features

- ✅ **Zero Thread Creation** - uvzmq does NOT create any threads, reuses libuv loop thread
- ✅ **Minimal API** - Only essential functions needed
- ✅ **Event-driven** - Uses libuv's event loop for all I/O
- ✅ **Direct ZMQ Access** - Full access to ZMQ APIs without abstraction
- ✅ **C99 standard** - Works with GCC and Clang compilers

## Design Philosophy

UVZMQ follows a different design philosophy from traditional ZeroMQ:

### Traditional ZeroMQ
- Creates I/O threads for socket operations
- User Thread → ZMQ API → ZMQ I/O Thread (epoll/kqueue) → Network
- Multiple threads, context switching overhead

### uvzmq + libzmq Integration
- Zero thread creation - reuses libuv event loop thread
- User Thread → ZMQ API → libuv Event Loop (uv_poll) → Network
- Single event loop, reduced overhead

### Key Principles

1. **Zero Thread Creation**: uvzmq completely avoids thread creation
2. **Libuv Integration**: ZMQ pollers use libuv event loop instead of own threads
3. **Minimal API**: Only 3 core functions + 4 getter functions
4. **Transparent**: Structure is public, no hidden magic
5. **Zero-abstraction**: Direct ZMQ API access

## Documentation

- 📖 [Tutorial](docs/en/tutorial.md) - Comprehensive guide with examples and best practices
- 📚 [API Documentation](docs/api/index.html) - Detailed API reference (run `make docs`)

## Quick Start

UVZMQ requires building with libuv-based poller enabled:

```bash
mkdir build && cd build
cmake -DUVZMQ_ENABLE_LIBUV_POLLER=ON ..
make
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
    // Create ZMQ context with ZERO I/O threads!
    void *zmq_ctx = zmq_ctx_new();
    zmq_ctx_set(zmq_ctx, ZMQ_IO_THREADS, 0);  // Critical: 0 I/O threads

    // Create ZMQ socket
    void *zmq_sock = zmq_socket(zmq_ctx, ZMQ_REP);
    zmq_bind(zmq_sock, "tcp://*:5555");

    // Create libuv loop
    uv_loop_t loop;
    uv_loop_init(&loop);

    // Integrate with libuv
    uvzmq_socket_t *uvzmq_sock = NULL;
    uvzmq_socket_new(&loop, zmq_sock, on_recv, NULL, &uvzmq_sock);

    // Run event loop
    uv_run(&loop, UV_RUN_DEFAULT);

    // Cleanup
    uvzmq_socket_free(uvzmq_sock);
    zmq_close(zmq_sock);
    zmq_ctx_term(zmq_ctx);
    uv_loop_close(&loop);

    return 0;
}
```

## Critical Configuration

When using uvzmq, you MUST:

1. **Set `ZMQ_IO_THREADS=0`** on the ZMQ context
2. **Build with `-DUVZMQ_ENABLE_LIBUV_POLLER=ON`**

Failure to do so will result in ZMQ still creating I/O threads.

## Learn More

- 📖 [Tutorial](docs/en/tutorial.md) - Comprehensive guide with examples and best practices
- 📚 [API Documentation](docs/api/index.html) - Detailed API reference (run `make docs`)

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
cmake -DUVZMQ_ENABLE_LIBUV_POLLER=ON ..
make
```

### Build Options

```bash
# Enable libuv-based poller (REQUIRED for uvzmq)
cmake -DUVZMQ_ENABLE_LIBUV_POLLER=ON ..

# Disable examples
cmake -DUVZMQ_BUILD_EXAMPLES=OFF ..

# Disable benchmarks
cmake -DUVZMQ_BUILD_BENCHMARKS=OFF ..

# Disable tests
cmake -DUVZMQ_BUILD_TESTS=OFF ..
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
- Use separate libuv event loops for each thread
- Use separate ZMQ contexts (with ZMQ_IO_THREADS=0)
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

💡 **Tip**: Check out the [Tutorial](docs/en/tutorial.md) for detailed usage guides and common patterns.

## Performance

### Benchmark Results

UVZMQ provides significant performance improvements over timer-based polling:

- **Timer-based**: ~11.2ms per message (~89 msg/sec)
- **Event-driven (UVZMQ)**: ~0.056ms per message (~17,857 msg/sec)
- **Improvement**: ~200x faster

### Throughput Data

Measured with IPC transport (Release mode, `-O2` optimization):

| Pattern              | Message Size | Throughput         | Latency   |
| -------------------- | ------------ | ------------------ | --------- |
| REQ/REP (round-trip) | 64B          | ~22,000 msg/sec    | ~0.045 ms |
| REQ/REP (round-trip) | 1KB          | ~21,000 msg/sec    | ~0.047 ms |
| PUSH/PULL (one-way)  | 64B          | ~5,800,000 msg/sec | -         |
| PUSH/PULL (one-way)  | 1KB          | ~1,300,000 msg/sec | -         |
| PUSH/PULL (one-way)  | 64KB         | ~67,000 msg/sec    | -         |

### Transport Comparison

IPC (Unix domain sockets) provides better performance than TCP for local communication:

- **Small messages**: IPC ~22% faster than TCP
- **Medium messages**: IPC ~25% faster than TCP
- **Large messages**: IPC ~50% faster than TCP

### Running Benchmarks

```bash
# Quick performance test
make quick-benchmark

# Full benchmark suite
cmake --build build --target benchmark
./build/benchmarks/benchmark
```

For large messages (>1KB), UVZMQ achieves performance comparable to native ZMQ with only 5-8% overhead due to libuv callback infrastructure.

## Design Philosophy

UVZMQ follows these principles:

1. **Zero Thread Creation** - uvzmq does NOT create any threads
2. **Libuv Integration** - ZMQ pollers use libuv event loop instead of own threads
3. **Minimal** - Only provides libuv event loop integration
4. **Direct** - Users interact with ZMQ APIs directly
5. **Transparent** - No hidden abstractions or magic
6. **Efficient** - Zero unnecessary overhead, single event loop

## License

MIT License

## Contributing

Contributions are welcome! Please ensure:

- Code follows C99 standard
- All functions have proper error checking
- Examples demonstrate best practices
- Set `ZMQ_IO_THREADS=0` when using uvzmq
- Build with `-DUVZMQ_ENABLE_LIBUV_POLLER=ON`
