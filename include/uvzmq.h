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

#endif /* UVZMQ_H */