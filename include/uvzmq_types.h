#ifndef UVZMQ_TYPES_H
#define UVZMQ_TYPES_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
struct uv_loop_s;

/* Forward declare zmq_msg_t */
typedef struct zmq_msg_t zmq_msg_t;

/* UVZMQ socket - opaque type */
typedef struct uvzmq_socket_s uvzmq_socket_t;

/* Callback types */
typedef void (*uvzmq_recv_callback)(uvzmq_socket_t *socket, zmq_msg_t *msg, void *user_data);
typedef void (*uvzmq_send_callback)(uvzmq_socket_t *socket, void *user_data);

/* Socket events */
typedef enum {
    UVZMQ_POLLIN = 1,
    UVZMQ_POLLOUT = 2,
    UVZMQ_POLLERR = 4
} uvzmq_event_t;

#ifdef __cplusplus
}
#endif

#endif /* UVZMQ_TYPES_H */