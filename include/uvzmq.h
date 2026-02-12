/**
 * @mainpage UVZMQ Documentation
 *
 * @section intro Introduction
 * UVZMQ is a libuv-based ZeroMQ integration layer that allows ZeroMQ to
 * use libuv's event loop instead of creating its own I/O threads.
 *
 * UVZMQ provides **one thing only**: integrating ZMQ sockets with libuv
 * event loop. All other ZMQ operations (send, recv, poll, setsockopt, etc.)
 * should be used directly from the ZMQ API.
 *
 * @section features Features
 * - ✅ **Zero Internal Threads** - uvzmq does NOT create any threads, reuses libuv loop thread
 * - ✅ **Minimal API** - Only essential functions needed
 * - ✅ **Event-driven** - Uses libuv's event loop for all I/O
 * - ✅ **Direct ZMQ Access** - Full access to ZMQ APIs without abstraction
 * - ✅ **C99 standard** - Works with GCC and Clang compilers
 *
 * @section design Design Principles
 * - **Zero Thread Creation**: uvzmq completely avoids thread creation
 * - **Libuv Integration**: ZMQ pollers use libuv event loop instead of own threads
 * - **Minimal API**: Only 3 core functions + 4 getter functions
 * - **Transparent**: Structure is public, no hidden magic
 * - **Zero-abstraction**: Direct ZMQ API access
 *
 * @section architecture Architecture
 * Unlike traditional ZeroMQ which creates I/O threads for socket operations,
 * uvzmq modifies libzmq's poller implementation to use libuv's event loop.
 *
 * Traditional ZMQ:
 * - User Thread -> ZMQ API -> ZMQ I/O Thread (epoll/kqueue) -> Network
 *
 * uvzmq + libzmq integration:
 * - User Thread -> ZMQ API -> libuv Event Loop (uv_poll) -> Network
 *
 * This design:
 * - Reduces thread overhead and context switching
 * - Simplifies thread synchronization
 * - Provides consistent event loop semantics with libuv applications
 *
 * @section performance Performance
 * UVZMQ provides performance benefits by:
 * - Eliminating ZMQ I/O thread overhead (no thread creation/maintenance)
 * - Using single event loop for all I/O (libuv + ZMQ)
 * - Reducing context switches between user and I/O threads
 *
 * @section quickstart Quick Start
 * Build uvzmq with the modified libzmq:
 * @code
 * mkdir build && cd build
 * cmake -DUVZMQ_ENABLE_LIBUV_POLLER=ON ..
 * make
 * @endcode
 *
 * Then use it in your code:
 * @code
 * #include "uvzmq.h"
 * #include <zmq.h>
 * #include <uv.h>
 *
 * void on_recv(uvzmq_socket_t* s, zmq_msg_t* msg, void* data) {
 *     // Echo back (zero-copy)
 *     zmq_msg_send(msg, uvzmq_get_zmq_socket(s), 0);
 *
 *     // IMPORTANT: Close message to avoid memory leak
 *     zmq_msg_close(msg);
 * }
 *
 * int main(void) {
 *     // Create ZMQ context (with zero I/O threads!)
 *     void* zmq_ctx = zmq_ctx_new();
 *     zmq_ctx_set(zmq_ctx, ZMQ_IO_THREADS, 0);  // Critical: 0 I/O threads
 *
 *     // Create ZMQ socket
 *     void* zmq_sock = zmq_socket(zmq_ctx, ZMQ_REP);
 *     zmq_bind(zmq_sock, "tcp://0.0.0.0:5555");
 *
 *     // Create libuv loop
 *     uv_loop_t loop;
 *     uv_loop_init(&loop);
 *
 *     // Integrate with libuv
 *     uvzmq_socket_t* uvzmq_sock = NULL;
 *     uvzmq_socket_new(&loop, zmq_sock, on_recv, NULL, &uvzmq_sock);
 *
 *     // Run event loop
 *     uv_run(&loop, UV_RUN_DEFAULT);
 *
 *     // Cleanup
 *     uvzmq_socket_free(uvzmq_sock);
 *     zmq_close(zmq_sock);
 *     zmq_ctx_term(zmq_ctx);
 *     uv_loop_close(&loop);
 *
 *     return 0;
 * }
 * @endcode
 *
 * @section critical Critical Configuration
 * When using uvzmq, you MUST:
 * 1. Set `ZMQ_IO_THREADS=0` on the ZMQ context
 * 2. Build uvzmq with `-DUVZMQ_ENABLE_LIBUV_POLLER=ON`
 *
 * Failure to do so will result in ZMQ still creating I/O threads.
 *
 * @section api API Reference
 * See the following sections for detailed API documentation:
 * - @ref uvzmq_socket_s - Socket structure
 * - @ref uvzmq_socket_new - Create a new socket
 * - @ref uvzmq_socket_close - Close the socket
 * - @ref uvzmq_socket_free - Free the socket
 * - @ref uvzmq_get_zmq_socket - Get ZMQ socket
 * - @ref uvzmq_get_loop - Get libuv loop
 * - @ref uvzmq_get_user_data - Get user data
 * - @ref uvzmq_get_fd - Get file descriptor
 *
 * @section examples Examples
 * See the `examples/` directory for complete examples:
 * - `simple.c` - Basic REQ/REP pattern
 * - `best_practices.c` - Complete example with signal handling
 * - `test_*.c` - Various test cases
 *
 * @section building Building
 * @code
 * git submodule update --init --recursive
 * mkdir build && cd build
 * cmake -DUVZMQ_ENABLE_LIBUV_POLLER=ON ..
 * make
 * @endcode
 *
 * Build options:
 * - `UVZMQ_ENABLE_LIBUV_POLLER=ON` - Enable libuv-based poller (required)
 * - `UVZMQ_BUILD_EXAMPLES=OFF` - Disable examples
 * - `UVZMQ_BUILD_BENCHMARKS=OFF` - Disable benchmarks
 *
 * @section threads Thread Safety
 * UVZMQ is **NOT thread-safe**. Each `uvzmq_socket_t` must be used by a
 * single thread only.
 *
 * For multi-threaded applications:
 * - Create separate `uvzmq_socket_t` instances for each thread
 * - Use separate libuv event loops for each thread
 * - Do NOT share `uvzmq_socket_t` or `zmq_sock` across threads
 *
 * @section error Error Handling
 * All functions return `0` on success and `-1` on failure. To diagnose
 * errors:
 * @code
 * if (uvzmq_socket_new(loop, zmq_sock, on_recv, NULL, &sock) != 0) {
 *     // Check system errno
 *     perror("uvzmq_socket_new");
 *
 *     // Or check ZMQ errors
 *     int zmq_err = zmq_errno();
 *     fprintf(stderr, "ZMQ error: %s\n", zmq_strerror(zmq_err));
 * }
 * @endcode
 *
 * @section cleanup Cleanup Order
 * Important: Follow the correct cleanup order:
 * @code
 * uvzmq_socket_free(uvzmq_sock);  // First
 * zmq_close(zmq_sock);              // Second
 * zmq_ctx_term(zmq_ctx);            // Third
 * uv_loop_close(&loop);             // Last
 * @endcode
 *
 * @file uvzmq.h
 * @brief Header file for integrating ZeroMQ with libuv
 *
 * UVZMQ provides a libuv-based ZeroMQ integration layer that allows ZeroMQ
 * to use libuv's event loop instead of creating its own I/O threads.
 *
 * Design Principles:
 * - Zero Thread Creation: uvzmq does NOT create any threads
 * - Libuv Integration: ZMQ pollers use libuv event loop
 * - Minimal API: Only 3 core functions
 * - Transparent: Structure is public, no hidden magic
 * - Zero-abstraction: Direct ZMQ API access
 *
 * Critical Configuration:
 * - MUST set ZMQ_IO_THREADS=0 on ZMQ context
 * - MUST build with -DUVZMQ_ENABLE_LIBUV_POLLER=ON
 */

#ifndef UVZMQ_H
#define UVZMQ_H

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <uv.h>

// Include ZMQ headers before using zmq_msg_t in callback
#include <zmq.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Forward declaration of uvzmq_socket_t
 */
typedef struct uvzmq_socket_s uvzmq_socket_t;

/**
 * @brief Callback type for receiving messages
 *
 * @param socket The uvzmq socket
 * @param msg The received ZMQ message (MUST be closed with zmq_msg_close())
 * @param user_data User data passed to uvzmq_socket_new()
 *
 * @note This callback MUST call zmq_msg_close(msg) after processing
 *       to avoid memory leaks.
 * @note You can reuse the message with zmq_msg_send() for echo/reply.
 */
typedef void (*uvzmq_recv_callback)(uvzmq_socket_t* socket,
                                    zmq_msg_t* msg,
                                    void* user_data);

/**
 * @brief UVZMQ socket structure
 *
 * This structure is public and can be directly accessed by users.
 *
 * @warning Each uvzmq_socket_t must be used by a single thread only.
 */
struct uvzmq_socket_s {
    uv_loop_t* loop;             /**< libuv event loop */
    void* zmq_sock;              /**< underlying ZMQ socket */
    int zmq_fd;                  /**< ZMQ socket file descriptor */
    uvzmq_recv_callback on_recv; /**< receive callback */
    void* user_data;             /**< user data */
    int closed;                  /**< socket closed flag */
    uv_poll_t* poll_handle;      /**< libuv poll handle */
    int ref_count;               /**< reference count for async cleanup */
};

/**
 * @brief Create a new UVZMQ socket and integrate with libuv
 *
 * This function:
 * 1. Allocates the uvzmq_socket_t structure
 * 2. Gets the ZMQ socket file descriptor
 * 3. Initializes libuv poll handle
 * 4. Starts monitoring the socket for readability
 *
 * @param loop libuv event loop
 * @param zmq_sock existing ZMQ socket
 * @param on_recv receive callback
 * @param user_data user data
 * @param socket [out] output parameter for the created socket
 *
 * @note On failure, check errno or zmq_errno() for detailed error information.
 * Common failure causes:
 *   - Invalid parameters (NULL pointers)
 *   - Memory allocation failure (ENOMEM)
 *   - ZMQ socket not properly configured
 *   - libuv initialization failure
 *
 * @return 0 on success, -1 on failure
 */
int uvzmq_socket_new(uv_loop_t* loop,
                     void* zmq_sock,
                     uvzmq_recv_callback on_recv,
                     void* user_data,
                     uvzmq_socket_t** socket);

/**
 * @brief Get the underlying ZMQ socket
 *
 * Use this to call standard ZMQ APIs directly.
 *
 * @param socket uvzmq socket
 * @return pointer to ZMQ socket, or NULL if socket is invalid
 */
static inline void* uvzmq_get_zmq_socket(uvzmq_socket_t* socket) {
    return socket ? socket->zmq_sock : NULL;
}

/**
 * @brief Get the libuv event loop
 *
 * @param socket uvzmq socket
 * @return pointer to libuv event loop, or NULL if socket is invalid
 */
static inline uv_loop_t* uvzmq_get_loop(uvzmq_socket_t* socket) {
    return socket ? socket->loop : NULL;
}

/**
 * @brief Get the user data
 *
 * @param socket uvzmq socket
 * @return user data pointer, or NULL if socket is invalid
 */
static inline void* uvzmq_get_user_data(uvzmq_socket_t* socket) {
    return socket ? socket->user_data : NULL;
}

/**
 * @brief Get the ZMQ socket file descriptor
 *
 * @param socket uvzmq socket
 * @return file descriptor, or -1 if socket is invalid
 */
static inline int uvzmq_get_fd(uvzmq_socket_t* socket) {
    return socket ? socket->zmq_fd : -1;
}

/**
 * @brief Close the UVZMQ socket
 *
 * This stops polling but does not free the socket structure.
 * Call uvzmq_socket_free() to fully cleanup.
 *
 * @param socket uvzmq socket
 * @return 0 on success, negative value on failure
 */
int uvzmq_socket_close(uvzmq_socket_t* socket);

/**
 * @brief Free the UVZMQ socket
 *
 * This stops polling, cleans up resources, and frees the socket structure.
 * Does NOT close the underlying ZMQ socket.
 *
 * @param socket uvzmq socket
 * @return 0 on success, negative value on failure
 */
int uvzmq_socket_free(uvzmq_socket_t* socket);

/**
 * @brief Start the reaper for handling socket cleanup
 *
 * When ZMQ_IO_THREADS=0, ZMQ's reaper thread is not created. This function
 * provides an alternative reaper mechanism that runs in the libuv event loop.
 * It periodically checks for sockets that need cleanup and handles the
 * reaping process that would normally be done by the reaper thread.
 *
 * @param loop libuv event loop
 * @return 0 on success, -1 on failure
 *
 * @note This should be called once per event loop before any zmq_close()
 *       operations are performed.
 * @note The reaper uses a timer with a 10ms interval to check for cleanup work.
 */
int uvzmq_reaper_start(uv_loop_t* loop);

/**
 * @brief Stop the reaper
 *
 * Stops the reaper timer and waits for any pending cleanup to complete.
 *
 * @param loop libuv event loop
 * @return 0 on success, -1 on failure
 */
int uvzmq_reaper_stop(uv_loop_t* loop);

#ifdef __cplusplus
}
#endif

#endif /* UVZMQ_H */
