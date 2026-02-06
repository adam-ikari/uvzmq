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

extern int uvzmq_map_zmq_error(int zmq_err);
extern void uvzmq_set_last_error(int error);

struct uvzmq_socket_s {
    uvzmq_context_t *context;
    void *zmq_sock;
    int closed;
    int type;
};

static int uvzmq_to_zmq_type(uvzmq_socket_type_t type)
{
    switch (type) {
        case UVZMQ_PAIR: return ZMQ_PAIR;
        case UVZMQ_PUB: return ZMQ_PUB;
        case UVZMQ_SUB: return ZMQ_SUB;
        case UVZMQ_REQ: return ZMQ_REQ;
        case UVZMQ_REP: return ZMQ_REP;
        case UVZMQ_DEALER: return ZMQ_DEALER;
        case UVZMQ_ROUTER: return ZMQ_ROUTER;
        case UVZMQ_PULL: return ZMQ_PULL;
        case UVZMQ_PUSH: return ZMQ_PUSH;
        case UVZMQ_XPUB: return ZMQ_XPUB;
        case UVZMQ_XSUB: return ZMQ_XSUB;
        case UVZMQ_STREAM: return ZMQ_STREAM;
        default: return -1;
    }
}

int uvzmq_socket_new(uvzmq_context_t *context, uvzmq_socket_type_t type, uvzmq_socket_t **socket)
{
    if (!context || !socket) {
        uvzmq_set_last_error(UVZMQ_EINVAL);
        return UVZMQ_EINVAL;
    }

    int zmq_type = uvzmq_to_zmq_type(type);
    if (zmq_type < 0) {
        uvzmq_set_last_error(UVZMQ_EINVAL);
        return UVZMQ_EINVAL;
    }

    uvzmq_socket_t *sock = UVZMQ_MALLOC(sizeof(uvzmq_socket_t));
    if (!sock) {
        uvzmq_set_last_error(UVZMQ_ENOMEM);
        return UVZMQ_ENOMEM;
    }

    memset(sock, 0, sizeof(uvzmq_socket_t));
    sock->context = context;
    sock->type = type;

    sock->zmq_sock = zmq_socket(context->zmq_ctx, zmq_type);
    if (!sock->zmq_sock) {
        int zmq_err = zmq_errno();
        UVZMQ_LOG_ERROR("Failed to create ZMQ socket: %s", zmq_strerror(zmq_err));
        UVZMQ_FREE(sock);
        uvzmq_set_last_error(uvzmq_map_zmq_error(zmq_err));
        return uvzmq_map_zmq_error(zmq_err);
    }

    *socket = sock;
    return UVZMQ_OK;
}

int uvzmq_socket_free(uvzmq_socket_t *socket)
{
    if (!socket) {
        uvzmq_set_last_error(UVZMQ_EINVAL);
        return UVZMQ_EINVAL;
    }

    if (!socket->closed) {
        uvzmq_socket_close(socket);
    }

    if (socket->zmq_sock) {
        zmq_close(socket->zmq_sock);
        socket->zmq_sock = NULL;
    }

    UVZMQ_FREE(socket);
    return UVZMQ_OK;
}

int uvzmq_socket_close(uvzmq_socket_t *socket)
{
    if (!socket) {
        uvzmq_set_last_error(UVZMQ_EINVAL);
        return UVZMQ_EINVAL;
    }

    if (socket->closed) {
        return UVZMQ_OK;
    }

    socket->closed = 1;
    return UVZMQ_OK;
}

void *uvzmq_socket_get_zmq_socket(uvzmq_socket_t *socket)
{
    return socket ? socket->zmq_sock : NULL;
}

uvzmq_context_t *uvzmq_socket_get_context(uvzmq_socket_t *socket)
{
    return socket ? socket->context : NULL;
}

int uvzmq_setsockopt_int(uvzmq_socket_t *socket, int option, int value)
{
    if (!socket || socket->closed) {
        uvzmq_set_last_error(UVZMQ_EINVAL);
        return UVZMQ_EINVAL;
    }

    int rc = zmq_setsockopt(socket->zmq_sock, option, &value, sizeof(value));
    if (rc != 0) {
        int zmq_err = zmq_errno();
        uvzmq_set_last_error(uvzmq_map_zmq_error(zmq_err));
        return uvzmq_map_zmq_error(zmq_err);
    }

    return UVZMQ_OK;
}

int uvzmq_getsockopt_int(uvzmq_socket_t *socket, int option, int *value)
{
    if (!socket || !value || socket->closed) {
        uvzmq_set_last_error(UVZMQ_EINVAL);
        return UVZMQ_EINVAL;
    }

    size_t size = sizeof(*value);
    int rc = zmq_getsockopt(socket->zmq_sock, option, value, &size);
    if (rc != 0) {
        int zmq_err = zmq_errno();
        uvzmq_set_last_error(uvzmq_map_zmq_error(zmq_err));
        return uvzmq_map_zmq_error(zmq_err);
    }

    return UVZMQ_OK;
}

int uvzmq_setsockopt_uint64(uvzmq_socket_t *socket, int option, uint64_t value)
{
    if (!socket || socket->closed) {
        uvzmq_set_last_error(UVZMQ_EINVAL);
        return UVZMQ_EINVAL;
    }

    int rc = zmq_setsockopt(socket->zmq_sock, option, &value, sizeof(value));
    if (rc != 0) {
        int zmq_err = zmq_errno();
        uvzmq_set_last_error(uvzmq_map_zmq_error(zmq_err));
        return uvzmq_map_zmq_error(zmq_err);
    }

    return UVZMQ_OK;
}

int uvzmq_getsockopt_uint64(uvzmq_socket_t *socket, int option, uint64_t *value)
{
    if (!socket || !value || socket->closed) {
        uvzmq_set_last_error(UVZMQ_EINVAL);
        return UVZMQ_EINVAL;
    }

    size_t size = sizeof(*value);
    int rc = zmq_getsockopt(socket->zmq_sock, option, value, &size);
    if (rc != 0) {
        int zmq_err = zmq_errno();
        uvzmq_set_last_error(uvzmq_map_zmq_error(zmq_err));
        return uvzmq_map_zmq_error(zmq_err);
    }

    return UVZMQ_OK;
}

int uvzmq_setsockopt_int64(uvzmq_socket_t *socket, int option, int64_t value)
{
    if (!socket || socket->closed) {
        uvzmq_set_last_error(UVZMQ_EINVAL);
        return UVZMQ_EINVAL;
    }

    int rc = zmq_setsockopt(socket->zmq_sock, option, &value, sizeof(value));
    if (rc != 0) {
        int zmq_err = zmq_errno();
        uvzmq_set_last_error(uvzmq_map_zmq_error(zmq_err));
        return uvzmq_map_zmq_error(zmq_err);
    }

    return UVZMQ_OK;
}

int uvzmq_getsockopt_int64(uvzmq_socket_t *socket, int option, int64_t *value)
{
    if (!socket || !value || socket->closed) {
        uvzmq_set_last_error(UVZMQ_EINVAL);
        return UVZMQ_EINVAL;
    }

    size_t size = sizeof(*value);
    int rc = zmq_getsockopt(socket->zmq_sock, option, value, &size);
    if (rc != 0) {
        int zmq_err = zmq_errno();
        uvzmq_set_last_error(uvzmq_map_zmq_error(zmq_err));
        return uvzmq_map_zmq_error(zmq_err);
    }

    return UVZMQ_OK;
}

int uvzmq_setsockopt_bin(uvzmq_socket_t *socket, int option, const void *value, size_t size)
{
    if (!socket || socket->closed) {
        uvzmq_set_last_error(UVZMQ_EINVAL);
        return UVZMQ_EINVAL;
    }

    if (option != ZMQ_SUBSCRIBE && option != ZMQ_UNSUBSCRIBE) {
        if (!value || size == 0) {
            uvzmq_set_last_error(UVZMQ_EINVAL);
            return UVZMQ_EINVAL;
        }
    }

    int rc = zmq_setsockopt(socket->zmq_sock, option, value, size);
    if (rc != 0) {
        int zmq_err = zmq_errno();
        uvzmq_set_last_error(uvzmq_map_zmq_error(zmq_err));
        return uvzmq_map_zmq_error(zmq_err);
    }

    return UVZMQ_OK;
}

int uvzmq_getsockopt_bin(uvzmq_socket_t *socket, int option, void *value, size_t *size)
{
    if (!socket || !value || !size || socket->closed) {
        uvzmq_set_last_error(UVZMQ_EINVAL);
        return UVZMQ_EINVAL;
    }

    int rc = zmq_getsockopt(socket->zmq_sock, option, value, size);
    if (rc != 0) {
        int zmq_err = zmq_errno();
        uvzmq_set_last_error(uvzmq_map_zmq_error(zmq_err));
        return uvzmq_map_zmq_error(zmq_err);
    }

    return UVZMQ_OK;
}

int uvzmq_bind(uvzmq_socket_t *socket, const char *endpoint)
{
    if (!socket || !endpoint || socket->closed) {
        uvzmq_set_last_error(UVZMQ_EINVAL);
        return UVZMQ_EINVAL;
    }

    int rc = zmq_bind(socket->zmq_sock, endpoint);
    if (rc != 0) {
        int zmq_err = zmq_errno();
        UVZMQ_LOG_ERROR("Failed to bind to %s: %s", endpoint, zmq_strerror(zmq_err));
        uvzmq_set_last_error(uvzmq_map_zmq_error(zmq_err));
        return uvzmq_map_zmq_error(zmq_err);
    }

    return UVZMQ_OK;
}

int uvzmq_connect(uvzmq_socket_t *socket, const char *endpoint)
{
    if (!socket || !endpoint || socket->closed) {
        uvzmq_set_last_error(UVZMQ_EINVAL);
        return UVZMQ_EINVAL;
    }

    int rc = zmq_connect(socket->zmq_sock, endpoint);
    if (rc != 0) {
        int zmq_err = zmq_errno();
        UVZMQ_LOG_ERROR("Failed to connect to %s: %s", endpoint, zmq_strerror(zmq_err));
        uvzmq_set_last_error(uvzmq_map_zmq_error(zmq_err));
        return uvzmq_map_zmq_error(zmq_err);
    }

    return UVZMQ_OK;
}

int uvzmq_disconnect(uvzmq_socket_t *socket, const char *endpoint)
{
    if (!socket || !endpoint || socket->closed) {
        uvzmq_set_last_error(UVZMQ_EINVAL);
        return UVZMQ_EINVAL;
    }

    int rc = zmq_disconnect(socket->zmq_sock, endpoint);
    if (rc != 0) {
        int zmq_err = zmq_errno();
        uvzmq_set_last_error(uvzmq_map_zmq_error(zmq_err));
        return uvzmq_map_zmq_error(zmq_err);
    }

    return UVZMQ_OK;
}

int uvzmq_unbind(uvzmq_socket_t *socket, const char *endpoint)
{
    if (!socket || !endpoint || socket->closed) {
        uvzmq_set_last_error(UVZMQ_EINVAL);
        return UVZMQ_EINVAL;
    }

    int rc = zmq_unbind(socket->zmq_sock, endpoint);
    if (rc != 0) {
        int zmq_err = zmq_errno();
        uvzmq_set_last_error(uvzmq_map_zmq_error(zmq_err));
        return uvzmq_map_zmq_error(zmq_err);
    }

    return UVZMQ_OK;
}

int uvzmq_send(uvzmq_socket_t *socket, zmq_msg_t *msg, int flags)
{
    if (!socket || !msg || socket->closed) {
        uvzmq_set_last_error(UVZMQ_EINVAL);
        return UVZMQ_EINVAL;
    }

    int zmq_flags = 0;
    if (flags & UVZMQ_DONTWAIT) zmq_flags |= ZMQ_DONTWAIT;
    if (flags & UVZMQ_SNDMORE) zmq_flags |= ZMQ_SNDMORE;

    int rc = zmq_msg_send(msg, socket->zmq_sock, zmq_flags);
    if (rc < 0) {
        int zmq_err = zmq_errno();
        uvzmq_set_last_error(uvzmq_map_zmq_error(zmq_err));
        return uvzmq_map_zmq_error(zmq_err);
    }

    return UVZMQ_OK;
}

int uvzmq_recv(uvzmq_socket_t *socket, zmq_msg_t *msg, int flags)
{
    if (!socket || !msg || socket->closed) {
        uvzmq_set_last_error(UVZMQ_EINVAL);
        return UVZMQ_EINVAL;
    }

    int zmq_flags = 0;
    if (flags & UVZMQ_DONTWAIT) zmq_flags |= ZMQ_DONTWAIT;

    int rc = zmq_msg_recv(msg, socket->zmq_sock, zmq_flags);
    if (rc < 0) {
        int zmq_err = zmq_errno();
        uvzmq_set_last_error(uvzmq_map_zmq_error(zmq_err));
        return uvzmq_map_zmq_error(zmq_err);
    }

    return UVZMQ_OK;
}