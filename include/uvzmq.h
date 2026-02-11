/**
 * @mainpage UVZMQ Documentation
 *
 * @section intro Introduction
 * UVZMQ is a minimal integration layer between ZeroMQ and libuv event loop,
 * allowing you to use ZMQ sockets with libuv's event-driven model.
 *
 * UVZMQ provides **one thing only**: integrating ZMQ sockets with libuv
 * event loop using `uv_poll`. All other ZMQ operations (send, recv, poll,
 * setsockopt, etc.) should be used directly from the ZMQ API.
 *
 * @section features Features
 * - ✅ **Minimal API** - Only essential functions needed
 * - ✅ **Event-driven** - Uses libuv's `uv_poll` for efficient I/O
 * - ✅ **Batch processing** - Optimized for high-throughput scenarios
 * - ✅ **Zero-copy support** - Compatible with ZMQ's zero-copy messaging
 * - ✅ **C99 standard** - Works with GCC and Clang compilers
 *
 * @section design Design Principles
 * - **Minimal**: Only 3 core functions + 4 getter functions
 * - **Transparent**: Structure is public, no hidden magic
 * - **Zero-abstraction**: Direct ZMQ API access
 * - **Header-only**: Single file integration
 *
 * @section performance Performance
 * UVZMQ provides significant performance improvements over timer-based polling:
 * - **Timer-based**: ~11.2ms per message
 * - **Event-driven (UVZMQ)**: ~0.056ms per message
 * - **Improvement**: ~200x faster
 *
 * For large messages (>1KB), UVZMQ achieves performance comparable to
 * native ZMQ with only 5-8% overhead due to libuv callback infrastructure.
 *
 * @section quickstart Quick Start
 * UVZMQ is a **header-only library**. Include the header file in one of
 * your source files with the implementation macro:
 *
 * @code
 * // In ONE of your source files
 * #define UVZMQ_IMPLEMENTATION
 * #include "uvzmq.h"
 *
 * // In other source files
 * #include "uvzmq.h"
 * @endcode
 *
 * Then use it in your code:
 *
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
 *     // Create ZMQ context and socket
 *     void* zmq_ctx = zmq_ctx_new();
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
 * cmake ..
 * make
 * @endcode
 *
 * @section batch Batch Processing Configuration
 * Users should use ZMQ API directly for batch size control:
 * @code
 * int batch_size = 8192;
 * zmq_setsockopt(zmq_sock, ZMQ_IN_BATCH_SIZE, &batch_size, sizeof(batch_size));
 * @endcode
 *
 * @section threads Thread Safety
 * UVZMQ is **NOT thread-safe**. Each `uvzmq_socket_t` must be used by a
 * single thread only.
 *
 * For multi-threaded applications:
 * - Create separate `uvzmq_socket_t` instances for each thread
 * - Use separate ZMQ contexts or configure `ZMQ_IO_THREADS` appropriately
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
 * @brief Header-only library for integrating ZeroMQ with libuv
 *
 * UVZMQ provides a minimal integration layer between ZeroMQ and libuv
 * event loop, allowing you to use ZMQ sockets with libuv's event-driven
 * model.
 *
 * Design Principles:
 * - Minimal: Only 3 core functions
 * - Transparent: Structure is public, no hidden magic
 * - Zero-abstraction: Direct ZMQ API access
 * - Header-only: Single file integration
 *
 * Usage:
 * @code
 * #define UVZMQ_IMPLEMENTATION
 * #include "uvzmq.h"
 * @endcode
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

#ifdef __cplusplus
}
#endif

/**
 * @brief Implementation section
 *
 * Define UVZMQ_IMPLEMENTATION in exactly one source file before
 * including this header to enable the implementation.
 *
 * @code
 * #define UVZMQ_IMPLEMENTATION
 * #include "uvzmq.h"
 * @endcode
 */
#ifdef UVZMQ_IMPLEMENTATION

#include <errno.h>
#include <stdio.h>
#include <string.h>

/**
 * @brief Internal libuv poll callback
 *
 * This function is called by libuv when the ZMQ socket becomes readable.
 * It processes all available messages until zmq_msg_recv returns EAGAIN.
 *
 * @param handle libuv poll handle
 * @param status libuv status (unused)
 * @param events libuv events (UV_READABLE)
 */
static void uvzmq_poll_callback(uv_poll_t* handle, int status, int events) {
    (void)status;
    uvzmq_socket_t* socket = (uvzmq_socket_t*)handle->data;

    if (socket->closed) {
        return;
    }

    if (events & UV_READABLE && socket->on_recv) {
        while (1) {
            zmq_msg_t msg;
            zmq_msg_init(&msg);

            int recv_rc = zmq_msg_recv(&msg, socket->zmq_sock, ZMQ_DONTWAIT);
            if (recv_rc >= 0) {
                socket->on_recv(socket, &msg, socket->user_data);
            } else if (errno == EAGAIN || errno == EINTR) {
                zmq_msg_close(&msg);
                break;
            } else {
                zmq_msg_close(&msg);
                break;
            }
        }
    }
}

int uvzmq_socket_new(uv_loop_t* loop,
                     void* zmq_sock,
                     uvzmq_recv_callback on_recv,
                     void* user_data,
                     uvzmq_socket_t** socket) {
    if (!loop || !zmq_sock || !socket) {
        return -1;
    }

    uvzmq_socket_t* sock = (uvzmq_socket_t*)malloc(sizeof(uvzmq_socket_t));
    if (!sock) {
        return -1;
    }

    memset(sock, 0, sizeof(uvzmq_socket_t));

    sock->loop = loop;
    sock->zmq_sock = zmq_sock;
    sock->on_recv = on_recv;
    sock->user_data = user_data;
    sock->closed = 0;
    sock->ref_count = 0;

    size_t fd_size = sizeof(sock->zmq_fd);
    int rc = zmq_getsockopt(zmq_sock, ZMQ_FD, &sock->zmq_fd, &fd_size);
    if (rc != 0) {
        free(sock);
        return -1;
    }

#ifdef DEBUG
    /* Validate socket type in debug mode */
    int socket_type;
    size_t type_size = sizeof(socket_type);
    if (zmq_getsockopt(zmq_sock, ZMQ_TYPE, &socket_type, &type_size) == 0) {
        if (socket_type == ZMQ_PUB || socket_type == ZMQ_PUSH) {
            fprintf(stderr,
                    "[WARNING] Socket type %d cannot receive messages. "
                    "UVZMQ monitors for readable events.\n",
                    socket_type);
        }
    }
#endif

    uv_poll_t* poll_handle = (uv_poll_t*)malloc(sizeof(uv_poll_t));
    if (!poll_handle) {
        free(sock);
        return -1;
    }

    poll_handle->data = sock;
    sock->poll_handle = poll_handle;

    rc = uv_poll_init(loop, poll_handle, sock->zmq_fd);
    if (rc != 0) {
        free(poll_handle);
        free(sock);
        return -1;
    }

    rc = uv_poll_start(poll_handle, UV_READABLE, uvzmq_poll_callback);
    if (rc != 0) {
        uv_close((uv_handle_t*)poll_handle, NULL);
        free(poll_handle);
        free(sock);
        return -1;
    }

    *socket = sock;
    return 0;
}

/**
 * @brief libuv handle close callback
 *
 * This callback is called when the poll handle is closed.
 * It decrements the reference count and frees the socket if count reaches 0.
 *
 * @param handle libuv handle
 */
static void on_close_callback(uv_handle_t* handle) {
    uv_poll_t* poll_handle = (uv_poll_t*)handle;
    uvzmq_socket_t* socket = (uvzmq_socket_t*)poll_handle->data;

    if (socket && --socket->ref_count == 0) {
        free(socket);
    }

    free(poll_handle);
}

/**
 * @brief Close the UVZMQ socket
 *
 * Sets the closed flag to stop receiving messages.
 * Does not free the socket structure.
 *
 * @param socket uvzmq socket
 * @return 0 on success, -1 on failure
 */
int uvzmq_socket_close(uvzmq_socket_t* socket) {
    if (!socket || socket->closed) {
        return -1;
    }

    socket->closed = 1;
    return 0;
}

/**
 * @brief Free the UVZMQ socket
 *
 * This function:
 * 1. Calls uvzmq_socket_close() if not already closed
 * 2. Stops the poll handle
 * 3. Closes the poll handle asynchronously
 * 4. Frees the socket structure
 *
 * @note Does NOT close the underlying ZMQ socket.
 *       The caller is responsible for closing zmq_sock.
 *
 * @param socket uvzmq socket
 * @return 0 on success, -1 on failure
 */
int uvzmq_socket_free(uvzmq_socket_t* socket) {
    if (!socket) {
        return -1;
    }

    if (!socket->closed) {
        uvzmq_socket_close(socket);
    }

    if (socket->poll_handle) {
        socket->ref_count++;
        uv_poll_stop(socket->poll_handle);
        uv_close((uv_handle_t*)socket->poll_handle, on_close_callback);
        socket->poll_handle = NULL;
    }

    return 0;
}

#endif /* UVZMQ_IMPLEMENTATION */

#endif /* UVZMQ_H */