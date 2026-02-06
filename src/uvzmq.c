#include "../include/uvzmq.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#if UVZMQ_USE_MIMALLOC
#include <mimalloc.h>
#define UVZMQ_MALLOC mi_malloc
#define UVZMQ_FREE mi_free
#else
#define UVZMQ_MALLOC malloc
#define UVZMQ_FREE free
#endif

/* Thread-local error storage */
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#define UVZMQ_THREAD_LOCAL _Thread_local
#elif defined(__GNUC__)
#define UVZMQ_THREAD_LOCAL __thread
#else
#error "Thread-local storage not supported"
#endif

static UVZMQ_THREAD_LOCAL int uvzmq_last_error = 0;

struct uvzmq_socket_s {
    uv_loop_t *loop;
    void *zmq_sock;
    int zmq_fd;
    uvzmq_recv_callback on_recv;
    uvzmq_send_callback on_send;  // Currently unused, reserved for future use
    void *user_data;
    int closed;
    uv_poll_t *poll_handle;
    
    // Performance optimization counters
    long long total_messages;
};

static void uvzmq_poll_callback(uv_poll_t *handle, int status, int events)
{
    (void)status;  // Unused parameter
    uvzmq_socket_t *socket = (uvzmq_socket_t *)handle->data;

    if (socket->closed) {
        return;
    }

    // Handle readable events
    if (events & UV_READABLE && socket->on_recv) {
        // Process all available messages (ZMQ FD is edge-triggered)
        // For large messages, process without batch limit to minimize poll events
        int batch_count = 0;
        
        while (1) {
            zmq_msg_t msg;
            zmq_msg_init(&msg);

            int recv_rc = zmq_msg_recv(&msg, socket->zmq_sock, ZMQ_DONTWAIT);
            if (recv_rc >= 0) {
                socket->on_recv(socket, &msg, socket->user_data);
                socket->total_messages++;
                batch_count++;
                
                // Check if there are more events to prevent infinite loops
                // Only check periodically to reduce overhead
                if (batch_count % 50 == 0) {
                    int zmq_events;
                    size_t events_size = sizeof(zmq_events);
                    if (zmq_getsockopt(socket->zmq_sock, ZMQ_EVENTS, &zmq_events, &events_size) == 0) {
                        if (!(zmq_events & ZMQ_POLLIN)) {
                            break;  // No more messages
                        }
                    }
                }
                
                // Safety limit to prevent CPU starvation in case of errors
                if (batch_count >= 1000) {
                    break;
                }
            } else if (errno == EAGAIN) {
                zmq_msg_close(&msg);
                break; // No more messages
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
        uvzmq_last_error = UVZMQ_ERROR_INVALID_PARAM;
        return UVZMQ_ERROR_INVALID_PARAM;
    }

    uvzmq_socket_t *sock = UVZMQ_MALLOC(sizeof(uvzmq_socket_t));
    if (!sock) {
        uvzmq_last_error = UVZMQ_ERROR_NOMEM;
        return UVZMQ_ERROR_NOMEM;
    }

    memset(sock, 0, sizeof(uvzmq_socket_t));

    sock->loop = loop;
    sock->zmq_sock = zmq_sock;
    sock->on_recv = on_recv;
    sock->on_send = on_send;
    sock->user_data = user_data;
    sock->closed = 0;

    // Get ZMQ socket file descriptor
    size_t fd_size = sizeof(sock->zmq_fd);
    int rc = zmq_getsockopt(zmq_sock, ZMQ_FD, &sock->zmq_fd, &fd_size);
    if (rc != 0) {
        fprintf(stderr, "[UVZMQ] zmq_getsockopt ZMQ_FD failed: %d (errno=%d)\n", rc, errno);
        UVZMQ_FREE(sock);
        uvzmq_last_error = UVZMQ_ERROR_GETSOCKOPT_FAILED;
        return UVZMQ_ERROR_GETSOCKOPT_FAILED;
    }

    // Use uv_poll for event-driven I/O
    uv_poll_t *poll_handle = UVZMQ_MALLOC(sizeof(uv_poll_t));
    if (!poll_handle) {
        UVZMQ_FREE(sock);
        uvzmq_last_error = UVZMQ_ERROR_NOMEM;
        return UVZMQ_ERROR_NOMEM;
    }

    poll_handle->data = sock;
    sock->poll_handle = poll_handle;

    rc = uv_poll_init(loop, poll_handle, sock->zmq_fd);
    if (rc != 0) {
        fprintf(stderr, "[UVZMQ] uv_poll_init failed: %d\n", rc);
        UVZMQ_FREE(poll_handle);
        UVZMQ_FREE(sock);
        uvzmq_last_error = UVZMQ_ERROR_INIT_FAILED;
        return UVZMQ_ERROR_INIT_FAILED;
    }

    rc = uv_poll_start(poll_handle, UV_READABLE, uvzmq_poll_callback);
    if (rc != 0) {
        fprintf(stderr, "[UVZMQ] uv_poll_start failed: %d\n", rc);
        uv_close((uv_handle_t *)poll_handle, NULL);
        UVZMQ_FREE(poll_handle);
        UVZMQ_FREE(sock);
        uvzmq_last_error = UVZMQ_ERROR_POLL_START_FAILED;
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

    // Stop poll handle before freeing
    // Note: uv_poll handles don't need uv_close, just uv_poll_stop and free
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

int uvzmq_poll(uvzmq_socket_t *socket, int events, int timeout_ms)
{
    if (!socket) {
        return UVZMQ_ERROR_INVALID_PARAM;
    }

    zmq_pollitem_t item;
    item.socket = socket->zmq_sock;
    item.fd = 0;
    item.events = 0;
    item.revents = 0;

    if (events & UVZMQ_POLLIN) {
        item.events |= ZMQ_POLLIN;
    }
    if (events & UVZMQ_POLLOUT) {
        item.events |= ZMQ_POLLOUT;
    }

    int rc = zmq_poll(&item, 1, timeout_ms);
    if (rc < 0) {
        uvzmq_last_error = errno;
        return -1;
    }

    int result = 0;
    if (item.revents & ZMQ_POLLIN) {
        result |= UVZMQ_POLLIN;
    }
    if (item.revents & ZMQ_POLLOUT) {
        result |= UVZMQ_POLLOUT;
    }
    if (item.revents & ZMQ_POLLERR) {
        result |= UVZMQ_POLLERR;
    }

    return result;
}

int uvzmq_errno(void)
{
    return uvzmq_last_error;
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

const char *uvzmq_strerror_last(void)
{
    return uvzmq_strerror(uvzmq_last_error);
}
