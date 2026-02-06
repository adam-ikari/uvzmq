#include "uvzmq.h"
#include <stdlib.h>
#include <string.h>

#if UVZMQ_USE_MIMALLOC
#include <mimalloc.h>
#define UVZMQ_MALLOC mi_malloc
#define UVZMQ_FREE mi_free
#else
#define UVZMQ_MALLOC malloc
#define UVZMQ_FREE free
#endif

#if UVZMQ_FEATURE_LOGGING && !defined(NDEBUG)
#define UVZMQ_LOG_DEBUG(fmt, ...) fprintf(stderr, "[DEBUG] " fmt "\n", ##__VA_ARGS__)
#else
#define UVZMQ_LOG_DEBUG(fmt, ...) do {} while (0)
#endif

#define UVZMQ_LOG_ERROR(fmt, ...) fprintf(stderr, "[ERROR] " fmt "\n", ##__VA_ARGS__)

extern void uvzmq_set_last_error(int error);

struct uvzmq_socket_s {
    uvzmq_context_t *context;
    void *zmq_sock;
    int closed;
    int type;
    uv_poll_t *poll_handle;
    uvzmq_event_callback_t event_callback;
    void *user_data;
};

typedef struct {
    zmq_msg_t *msg;
    uvzmq_send_callback_t callback;
    void *user_data;
} uvzmq_send_req_t;

static void on_uv_poll(uv_poll_t *handle, int status, int events)
{
    uvzmq_socket_t *socket = (uvzmq_socket_t *)handle->data;

    if (!socket || socket->closed) {
        return;
    }

    if (status < 0) {
        UVZMQ_LOG_ERROR("Poll error: %s", uv_strerror(status));
        return;
    }

    int zmq_events = 0;
    if (events & UV_READABLE) zmq_events |= UVZMQ_POLLIN;
    if (events & UV_WRITABLE) zmq_events |= UVZMQ_POLLOUT;

    if (socket->event_callback) {
        socket->event_callback(socket, zmq_events, socket->user_data);
    }
}

static int uvzmq_get_socket_fd(void *zmq_sock)
{
    int fd = 0;
    size_t fd_size = sizeof(fd);

    int rc = zmq_getsockopt(zmq_sock, ZMQ_FD, &fd, &fd_size);
    if (rc != 0) {
        return -1;
    }

    return fd;
}

static int uvzmq_get_socket_events(void *zmq_sock)
{
    int events = 0;
    size_t events_size = sizeof(events);

    int rc = zmq_getsockopt(zmq_sock, ZMQ_EVENTS, &events, &events_size);
    if (rc != 0) {
        return -1;
    }

    return events;
}

int uvzmq_send_async(uvzmq_socket_t *socket, zmq_msg_t *msg, uvzmq_send_callback_t callback, void *user_data)
{
    (void)socket;
    (void)msg;
    (void)callback;
    (void)user_data;
    uvzmq_set_last_error(UVZMQ_ENOTSUP);
    return UVZMQ_ENOTSUP;
}

int uvzmq_socket_start(uvzmq_socket_t *socket, uvzmq_event_callback_t callback, void *user_data)
{
    if (!socket || socket->closed) {
        uvzmq_set_last_error(UVZMQ_EINVAL);
        return UVZMQ_EINVAL;
    }

    if (socket->poll_handle) {
        return UVZMQ_OK;
    }

    int fd = uvzmq_get_socket_fd(socket->zmq_sock);
    if (fd < 0) {
        uvzmq_set_last_error(UVZMQ_ERROR);
        return UVZMQ_ERROR;
    }

    socket->poll_handle = UVZMQ_MALLOC(sizeof(uv_poll_t));
    if (!socket->poll_handle) {
        uvzmq_set_last_error(UVZMQ_ENOMEM);
        return UVZMQ_ENOMEM;
    }

    socket->poll_handle->data = socket;
    socket->event_callback = callback;
    socket->user_data = user_data;

    int events = uvzmq_get_socket_events(socket->zmq_sock);
    int uv_events = 0;
    if (events & ZMQ_POLLIN) uv_events |= UV_READABLE;
    if (events & ZMQ_POLLOUT) uv_events |= UV_WRITABLE;

    int rc = uv_poll_init_socket(socket->context->loop, socket->poll_handle, fd);
    if (rc != 0) {
        UVZMQ_LOG_ERROR("Failed to init poll: %s", uv_strerror(rc));
        UVZMQ_FREE(socket->poll_handle);
        socket->poll_handle = NULL;
        uvzmq_set_last_error(UVZMQ_ERROR);
        return UVZMQ_ERROR;
    }

    rc = uv_poll_start(socket->poll_handle, uv_events, on_uv_poll);
    if (rc != 0) {
        UVZMQ_LOG_ERROR("Failed to start poll: %s", uv_strerror(rc));
        uv_close((uv_handle_t *)socket->poll_handle, NULL);
        UVZMQ_FREE(socket->poll_handle);
        socket->poll_handle = NULL;
        uvzmq_set_last_error(UVZMQ_ERROR);
        return UVZMQ_ERROR;
    }

    return UVZMQ_OK;
}

int uvzmq_socket_stop(uvzmq_socket_t *socket)
{
    if (!socket) {
        uvzmq_set_last_error(UVZMQ_EINVAL);
        return UVZMQ_EINVAL;
    }

    if (!socket->poll_handle) {
        return UVZMQ_OK;
    }

    uv_poll_stop(socket->poll_handle);
    uv_close((uv_handle_t *)socket->poll_handle, NULL);

    UVZMQ_FREE(socket->poll_handle);
    socket->poll_handle = NULL;
    socket->event_callback = NULL;
    socket->user_data = NULL;

    return UVZMQ_OK;
}