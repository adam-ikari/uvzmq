#ifndef UVZMQ_H
#define UVZMQ_H

#include "uvzmq_types.h"
#include "uvzmq_features.h"
#include <uv.h>
#include <zmq.h>

/* Memory allocation macros */
#if UVZMQ_USE_MIMALLOC
#include <mimalloc.h>
#define UVZMQ_MALLOC mi_malloc
#define UVZMQ_FREE mi_free
#else
#define UVZMQ_MALLOC malloc
#define UVZMQ_FREE free
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ========== Error Codes ========== */

/* Success */
#define UVZMQ_OK 0

/* Error codes */
#define UVZMQ_ERROR_INVALID_PARAM -1
#define UVZMQ_ERROR_NOMEM -2
#define UVZMQ_ERROR_INIT_FAILED -3
#define UVZMQ_ERROR_POLL_START_FAILED -4
#define UVZMQ_ERROR_GETSOCKOPT_FAILED -5

/* ========== Thread Safety ==========
 *
 * UVZMQ is NOT thread-safe by default. Each uvzmq_socket_t must be used
 * by a single thread only. The underlying ZMQ socket must not be accessed
 * from other threads while uvzmq_socket_new is active.
 *
 * ========== Core Integration Functions ========== */

/* Initialize UVZMQ socket and integrate with libuv event loop
 * 
 * This is the ONLY function you need to integrate ZMQ with libuv.
 * After calling this, you can use all standard ZMQ APIs directly.
 *
 * Parameters:
 *   loop: libuv event loop
 *   zmq_sock: existing ZMQ socket (created with zmq_socket())
 *   on_recv: callback for when socket is readable
 *   on_send: callback for when socket is writable (currently unused)
 *   user_data: user data passed to callbacks
 *   socket: [out] pointer to receive the created uvzmq socket
 *
 * Returns: UVZMQ_OK on success, error code on failure
 *
 * Example:
 *   void *zmq_ctx = zmq_ctx_new();
 *   void *zmq_sock = zmq_socket(zmq_ctx, ZMQ_REQ);
 *   zmq_connect(zmq_sock, "tcp://localhost:5555");
 *   
 *   uvzmq_socket_t *uvzmq_sock = NULL;
 *   uvzmq_socket_new(loop, zmq_sock, on_recv, NULL, NULL, &uvzmq_sock);
 *
 *   // Now use standard ZMQ APIs
 *   zmq_send(zmq_sock, "Hello", 5, 0);
 *
 *   // Free uvzmq socket when done (does NOT close zmq_sock)
 *   uvzmq_socket_free(uvzmq_sock);
 *
 *   // Close ZMQ socket
 *   zmq_close(zmq_sock);
 *   zmq_ctx_term(zmq_ctx);
 *
 * NOTE:
 *   - UVZMQ provides ONLY libuv event loop integration via uv_poll
 *   - For other operations (send, recv, poll), use standard ZMQ APIs directly
 *   - Use uvzmq_get_zmq_socket() to access the underlying ZMQ socket
 *
 * Thread Safety:
 *   Each uvzmq_socket_t must be used by a single thread only.
 *   The underlying ZMQ socket must not be accessed from other threads
 *   while uvzmq_socket_new is active.
 */
int uvzmq_socket_new(uv_loop_t *loop, void *zmq_sock,
                     uvzmq_recv_callback on_recv,
                     uvzmq_send_callback on_send,
                     void *user_data,
                     uvzmq_socket_t **socket);

/* Stop event handling for the socket
 * This stops libuv from polling the socket, but the socket remains valid.
 * 
 * Returns: UVZMQ_OK on success, error code on failure
 */
int uvzmq_socket_close(uvzmq_socket_t *socket);

/* Free UVZMQ socket resources
 * 
 * This stops libuv from polling the socket and frees uvzmq resources.
 * This does NOT close the underlying ZMQ socket.
 * 
 * You must call zmq_close() on the ZMQ socket yourself after calling this function.
 * 
 * Returns: UVZMQ_OK on success, error code on failure
 */
int uvzmq_socket_free(uvzmq_socket_t *socket);

/* ========== Utility Functions ========== */

/* Get the underlying ZMQ socket
 * Use this to call standard ZMQ APIs
 * Returns: pointer to ZMQ socket (void*)
 */
void *uvzmq_get_zmq_socket(uvzmq_socket_t *socket);

/* Get the libuv loop
 * Returns: pointer to uv_loop_t
 */
uv_loop_t *uvzmq_get_loop(uvzmq_socket_t *socket);

/* Get user data
 * Returns: user data pointer
 */
void *uvzmq_get_user_data(uvzmq_socket_t *socket);

/* Get the ZMQ file descriptor
 * Use this to integrate with other libuv handle types
 * Returns: file descriptor, or -1 on error
 */
int uvzmq_get_fd(uvzmq_socket_t *socket);

/* ========== Error Handling ========== */

/* Get error message string
 * Returns: static string describing the error
 */
const char *uvzmq_strerror(int err);

#ifdef __cplusplus
}
#endif

/* ========== Implementation ========== */
#ifdef UVZMQ_IMPLEMENTATION

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

/* Internal structure */
struct uvzmq_socket_s {
    uv_loop_t *loop;
    void *zmq_sock;
    int zmq_fd;
    uvzmq_recv_callback on_recv;
    uvzmq_send_callback on_send;
    void *user_data;
    int closed;
    uv_poll_t *poll_handle;
    long long total_messages;
};

/* Internal callback */
static void uvzmq_poll_callback(uv_poll_t *handle, int status, int events)
{
    (void)status;
    uvzmq_socket_t *socket = (uvzmq_socket_t *)handle->data;

    if (socket->closed) {
        return;
    }

    if (events & UV_READABLE && socket->on_recv) {
        int batch_count = 0;
        
        while (1) {
            zmq_msg_t msg;
            zmq_msg_init(&msg);

            int recv_rc = zmq_msg_recv(&msg, socket->zmq_sock, ZMQ_DONTWAIT);
            if (recv_rc >= 0) {
                socket->on_recv(socket, &msg, socket->user_data);
                socket->total_messages++;
                batch_count++;
                
                if (batch_count % 50 == 0) {
                    int zmq_events;
                    size_t events_size = sizeof(zmq_events);
                    if (zmq_getsockopt(socket->zmq_sock, ZMQ_EVENTS, &zmq_events, &events_size) == 0) {
                        if (!(zmq_events & ZMQ_POLLIN)) {
                            break;
                        }
                    }
                }
                
                if (batch_count >= 1000) {
                    break;
                }
            } else if (errno == EAGAIN) {
                zmq_msg_close(&msg);
                break;
            } else {
                fprintf(stderr, "[UVZMQ] zmq_msg_recv failed: %s (errno=%d)\n", 
                        zmq_strerror(errno), errno);
                zmq_msg_close(&msg);
                break;
            }
        }
    }
}

int uvzmq_socket_new(uv_loop_t *loop, void *zmq_sock,
                     uvzmq_recv_callback on_recv,
                     uvzmq_send_callback on_send,
                     void *user_data,
                     uvzmq_socket_t **socket)
{
    if (!loop || !zmq_sock || !socket) {
        return UVZMQ_ERROR_INVALID_PARAM;
    }

    uvzmq_socket_t *sock = UVZMQ_MALLOC(sizeof(uvzmq_socket_t));
    if (!sock) {
        return UVZMQ_ERROR_NOMEM;
    }

    memset(sock, 0, sizeof(uvzmq_socket_t));

    sock->loop = loop;
    sock->zmq_sock = zmq_sock;
    sock->on_recv = on_recv;
    sock->on_send = on_send;
    sock->user_data = user_data;
    sock->closed = 0;

    size_t fd_size = sizeof(sock->zmq_fd);
    int rc = zmq_getsockopt(zmq_sock, ZMQ_FD, &sock->zmq_fd, &fd_size);
    if (rc != 0) {
        fprintf(stderr, "[UVZMQ] zmq_getsockopt ZMQ_FD failed: %d (errno=%d)\n", rc, errno);
        UVZMQ_FREE(sock);
        return UVZMQ_ERROR_GETSOCKOPT_FAILED;
    }

    uv_poll_t *poll_handle = UVZMQ_MALLOC(sizeof(uv_poll_t));
    if (!poll_handle) {
        UVZMQ_FREE(sock);
        return UVZMQ_ERROR_NOMEM;
    }

    poll_handle->data = sock;
    sock->poll_handle = poll_handle;

    rc = uv_poll_init(loop, poll_handle, sock->zmq_fd);
    if (rc != 0) {
        fprintf(stderr, "[UVZMQ] uv_poll_init failed: %d\n", rc);
        UVZMQ_FREE(poll_handle);
        UVZMQ_FREE(sock);
        return UVZMQ_ERROR_INIT_FAILED;
    }

    rc = uv_poll_start(poll_handle, UV_READABLE, uvzmq_poll_callback);
    if (rc != 0) {
        fprintf(stderr, "[UVZMQ] uv_poll_start failed: %d\n", rc);
        uv_close((uv_handle_t *)poll_handle, NULL);
        UVZMQ_FREE(poll_handle);
        UVZMQ_FREE(sock);
        return UVZMQ_ERROR_POLL_START_FAILED;
    }

    *socket = sock;
    return UVZMQ_OK;
}

int uvzmq_socket_close(uvzmq_socket_t *socket)
{
    if (!socket || socket->closed) {
        return UVZMQ_ERROR_INVALID_PARAM;
    }

    socket->closed = 1;
    return UVZMQ_OK;
}

int uvzmq_socket_free(uvzmq_socket_t *socket)
{
    if (!socket) {
        return UVZMQ_ERROR_INVALID_PARAM;
    }

    if (!socket->closed) {
        uvzmq_socket_close(socket);
    }

    if (socket->poll_handle) {
        uv_poll_stop(socket->poll_handle);
        UVZMQ_FREE(socket->poll_handle);
        socket->poll_handle = NULL;
    }

    UVZMQ_FREE(socket);
    return UVZMQ_OK;
}

void *uvzmq_get_zmq_socket(uvzmq_socket_t *socket)
{
    return socket ? socket->zmq_sock : NULL;
}

uv_loop_t *uvzmq_get_loop(uvzmq_socket_t *socket)
{
    return socket ? socket->loop : NULL;
}

void *uvzmq_get_user_data(uvzmq_socket_t *socket)
{
    return socket ? socket->user_data : NULL;
}

int uvzmq_get_fd(uvzmq_socket_t *socket)
{
    return socket ? socket->zmq_fd : -1;
}

const char *uvzmq_strerror(int err)
{
    switch (err) {
        case UVZMQ_OK: return "Success";
        case UVZMQ_ERROR_INVALID_PARAM: return "Invalid parameter";
        case UVZMQ_ERROR_NOMEM: return "Out of memory";
        case UVZMQ_ERROR_INIT_FAILED: return "Poll initialization failed";
        case UVZMQ_ERROR_POLL_START_FAILED: return "Poll start failed";
        case UVZMQ_ERROR_GETSOCKOPT_FAILED: return "Get socket option failed";
        case ENOMEM: return "Out of memory (errno)";
        case EINVAL: return "Invalid argument (errno)";
        default: return "Unknown error";
    }
}

#endif /* UVZMQ_IMPLEMENTATION */

#endif /* UVZMQ_H */