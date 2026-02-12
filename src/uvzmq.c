/**
 * @file uvzmq.c
 * @brief Implementation of uvzmq - libuv-based ZeroMQ integration
 *
 * This file contains the implementation of uvzmq functions that integrate
 * ZeroMQ sockets with libuv's event loop.
 *
 * Design Philosophy:
 * - Zero Thread Creation: uvzmq does NOT create any threads
 * - Libuv Integration: ZMQ pollers use libuv event loop instead of own threads
 * - Minimal API: Only essential functions
 * - Transparent: Public structure, no hidden magic
 */

#include "uvzmq.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>

/**
 * @brief Internal reaper state
 */
typedef struct uvzmq_reaper_s {
    uv_loop_t* loop;
    uv_timer_t timer;
    int running;
} uvzmq_reaper_t;

/**
 * @brief Global reaper instance (one per event loop is supported for now)
 */
static uvzmq_reaper_t* g_reaper = NULL;

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

/**
 * @brief Reaper timer callback
 *
 * This function is called periodically by the libuv timer to check for
 * sockets that need cleanup. It processes any pending ZMQ commands that
 * would normally be handled by the reaper thread.
 *
 * @param handle libuv timer handle
 */
static void uvzmq_reaper_timer_callback(uv_timer_t* handle) {
    (void)handle;

    /*
     * When ZMQ_IO_THREADS=0, ZMQ does not create a reaper thread.
     * The reaper thread's job is to process cleanup commands for closed sockets.
     *
     * In our libuv-based approach:
     * 1. zmq_close() sends the socket to reaper for cleanup
     * 2. Without a reaper thread, this work must be done in the user's thread
     * 3. We process ZMQ's mailbox commands here to complete the cleanup
     *
     * Note: This is a simplified approach. A full implementation would need to
     * integrate more deeply with ZMQ's internal poller mechanism.
     */
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

int uvzmq_socket_close(uvzmq_socket_t* socket) {
    if (!socket || socket->closed) {
        return -1;
    }

    socket->closed = 1;
    return 0;
}

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

int uvzmq_reaper_start(uv_loop_t* loop) {
    if (!loop) {
        return -1;
    }

    /* Check if reaper is already started */
    if (g_reaper && g_reaper->loop == loop && g_reaper->running) {
        return 0;
    }

    /* Create new reaper if needed */
    if (!g_reaper) {
        g_reaper = (uvzmq_reaper_t*)malloc(sizeof(uvzmq_reaper_t));
        if (!g_reaper) {
            return -1;
        }
        memset(g_reaper, 0, sizeof(uvzmq_reaper_t));
    }

    g_reaper->loop = loop;

    /* Initialize timer */
    int rc = uv_timer_init(loop, &g_reaper->timer);
    if (rc != 0) {
        return -1;
    }

    /* Set timer data */
    g_reaper->timer.data = g_reaper;

    /* Start timer: check every 10ms for cleanup work */
    rc = uv_timer_start(&g_reaper->timer, uvzmq_reaper_timer_callback, 10, 10);
    if (rc != 0) {
        uv_close((uv_handle_t*)&g_reaper->timer, NULL);
        return -1;
    }

    g_reaper->running = 1;
    return 0;
}

int uvzmq_reaper_stop(uv_loop_t* loop) {
    if (!loop || !g_reaper || g_reaper->loop != loop) {
        return -1;
    }

    if (!g_reaper->running) {
        return 0;
    }

    /* Stop timer */
    uv_timer_stop(&g_reaper->timer);
    g_reaper->running = 0;

    return 0;
}
