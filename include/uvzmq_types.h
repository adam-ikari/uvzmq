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

/* Callback types
 *
 * IMPORTANT: The on_recv callback MUST close the zmq_msg_t after use
 * by calling zmq_msg_close(). Failure to do so will cause memory leaks.
 *
 * on_recv callback:
 *   - Called when data is available on the socket
 *   - The zmq_msg_t is passed with received data
 *   - MUST call zmq_msg_close(msg) after processing
 *   - Can use zmq_msg_send(msg, ...) to echo/reuse the message
 *
 * on_send callback:
 *   - Currently not implemented (reserved for future use)
 *   - Set to NULL for now
 */
typedef void (*uvzmq_recv_callback)(uvzmq_socket_t *socket, zmq_msg_t *msg, void *user_data);
typedef void (*uvzmq_send_callback)(uvzmq_socket_t *socket, void *user_data);

#ifdef __cplusplus
}
#endif

#endif /* UVZMQ_TYPES_H */