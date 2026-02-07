/**
 * @file test_uvzmq_socket_free.cpp
 * @brief Unit tests for uvzmq_socket_free function
 */

#define UVZMQ_IMPLEMENTATION
#include "../include/uvzmq.h"

#include <gtest/gtest.h>
#include <zmq.h>
#include <uv.h>

class UVZMQSocketFreeTest : public ::testing::Test {
protected:
    void SetUp() override {
        ASSERT_EQ(uv_loop_init(&loop), 0);

        zmq_ctx = zmq_ctx_new();
        ASSERT_NE(zmq_ctx, nullptr);

        zmq_sock = zmq_socket(zmq_ctx, ZMQ_SUB);
        ASSERT_NE(zmq_sock, nullptr);
    }

    void TearDown() override {
        if (zmq_sock) {
            zmq_close(zmq_sock);
        }
        if (zmq_ctx) {
            zmq_ctx_term(zmq_ctx);
        }
        uv_loop_close(&loop);
    }

    uv_loop_t loop;
    void* zmq_ctx;
    void* zmq_sock;
};

/**
 * @brief Test successful free
 */
TEST_F(UVZMQSocketFreeTest, Success) {
    uvzmq_socket_t* socket = nullptr;
    int rc1 = uvzmq_socket_new(&loop, zmq_sock, nullptr, nullptr, &socket);
    ASSERT_EQ(rc1, 0);
    ASSERT_NE(socket, nullptr);

    int rc2 = uvzmq_socket_free(socket);
    EXPECT_EQ(rc2, 0);
}

/**
 * @brief Test free with NULL socket
 */
TEST_F(UVZMQSocketFreeTest, NullSocket) {
    int rc = uvzmq_socket_free(nullptr);
    EXPECT_EQ(rc, -1);
}

/**
 * @brief Test free without close
 */
TEST_F(UVZMQSocketFreeTest, FreeWithoutClose) {
    uvzmq_socket_t* socket = nullptr;
    int rc1 = uvzmq_socket_new(&loop, zmq_sock, nullptr, nullptr, &socket);
    ASSERT_EQ(rc1, 0);
    ASSERT_EQ(socket->closed, 0);

    int rc2 = uvzmq_socket_free(socket);
    EXPECT_EQ(rc2, 0);
}

/**
 * @brief Test free with close
 */
TEST_F(UVZMQSocketFreeTest, FreeWithClose) {
    uvzmq_socket_t* socket = nullptr;
    int rc1 = uvzmq_socket_new(&loop, zmq_sock, nullptr, nullptr, &socket);
    ASSERT_EQ(rc1, 0);

    uvzmq_socket_close(socket);
    EXPECT_EQ(socket->closed, 1);

    int rc2 = uvzmq_socket_free(socket);
    EXPECT_EQ(rc2, 0);
}

/**
 * @brief Test free does not close ZMQ socket
 */
TEST_F(UVZMQSocketFreeTest, FreeDoesNotCloseZMQSocket) {
    uvzmq_socket_t* socket = nullptr;
    int rc1 = uvzmq_socket_new(&loop, zmq_sock, nullptr, nullptr, &socket);
    ASSERT_EQ(rc1, 0);

    int rc2 = uvzmq_socket_free(socket);
    EXPECT_EQ(rc2, 0);

    // ZMQ socket should still be usable
    int socket_type;
    size_t size = sizeof(socket_type);
    int rc3 = zmq_getsockopt(zmq_sock, ZMQ_TYPE, &socket_type, &size);
    EXPECT_EQ(rc3, 0);
}

/**
 * @brief Test free on already closed socket
 */
TEST_F(UVZMQSocketFreeTest, FreeAlreadyClosedSocket) {
    uvzmq_socket_t* socket = nullptr;
    int rc1 = uvzmq_socket_new(&loop, zmq_sock, nullptr, nullptr, &socket);
    ASSERT_EQ(rc1, 0);

    uvzmq_socket_close(socket);
    int rc2 = uvzmq_socket_free(socket);
    EXPECT_EQ(rc2, 0);
}

/**
 * @brief Test double free
 */
TEST_F(UVZMQSocketFreeTest, DoubleFree) {
    uvzmq_socket_t* socket = nullptr;
    int rc1 = uvzmq_socket_new(&loop, zmq_sock, nullptr, nullptr, &socket);
    ASSERT_EQ(rc1, 0);

    int rc2 = uvzmq_socket_free(socket);
    EXPECT_EQ(rc2, 0);

    // After free, set socket to NULL to avoid double free
    // Note: uvzmq_socket_free does not set the pointer to NULL
    // Users must manage this themselves
    socket = nullptr;

    // Calling with NULL should return -1
    int rc3 = uvzmq_socket_free(socket);
    EXPECT_EQ(rc3, -1);
}

/**
 * @brief Test free with event loop running
 */
TEST_F(UVZMQSocketFreeTest, FreeWithEventLoopRunning) {
    uvzmq_socket_t* socket = nullptr;
    int rc1 = uvzmq_socket_new(&loop, zmq_sock, nullptr, nullptr, &socket);
    ASSERT_EQ(rc1, 0);

    // Run event loop once
    uv_run(&loop, UV_RUN_NOWAIT);

    int rc2 = uvzmq_socket_free(socket);
    EXPECT_EQ(rc2, 0);
}

/**
 * @brief Test free preserves poll handle cleanup
 */
TEST_F(UVZMQSocketFreeTest, FreePreservesPollHandleCleanup) {
    uvzmq_socket_t* socket = nullptr;
    int rc1 = uvzmq_socket_new(&loop, zmq_sock, nullptr, nullptr, &socket);
    ASSERT_EQ(rc1, 0);
    ASSERT_NE(socket->poll_handle, nullptr);

    int rc2 = uvzmq_socket_free(socket);
    EXPECT_EQ(rc2, 0);

    // Socket should be freed, poll handle cleanup handled by libuv
    // Run loop to process cleanup
    uv_run(&loop, UV_RUN_NOWAIT);
}

/**
 * @brief Test free with user data
 */
TEST_F(UVZMQSocketFreeTest, FreeWithUserData) {
    int user_data = 42;
    uvzmq_socket_t* socket = nullptr;
    int rc1 = uvzmq_socket_new(&loop, zmq_sock, nullptr, &user_data, &socket);
    ASSERT_EQ(rc1, 0);
    ASSERT_EQ(socket->user_data, &user_data);

    int rc2 = uvzmq_socket_free(socket);
    EXPECT_EQ(rc2, 0);

    // User data pointer is not freed by uvzmq_socket_free
    // (user's responsibility)
}

/**
 * @brief Test multiple free operations
 */
TEST_F(UVZMQSocketFreeTest, MultipleFreeOperations) {
    // Create additional ZMQ sockets
    void* zmq_sock2 = zmq_socket(zmq_ctx, ZMQ_SUB);
    void* zmq_sock3 = zmq_socket(zmq_ctx, ZMQ_SUB);
    ASSERT_NE(zmq_sock2, nullptr);
    ASSERT_NE(zmq_sock3, nullptr);

    uvzmq_socket_t* socket1 = nullptr;
    uvzmq_socket_t* socket2 = nullptr;
    uvzmq_socket_t* socket3 = nullptr;

    int rc1 = uvzmq_socket_new(&loop, zmq_sock, nullptr, nullptr, &socket1);
    int rc2 = uvzmq_socket_new(&loop, zmq_sock2, nullptr, nullptr, &socket2);
    int rc3 = uvzmq_socket_new(&loop, zmq_sock3, nullptr, nullptr, &socket3);

    ASSERT_EQ(rc1, 0);
    ASSERT_EQ(rc2, 0);
    ASSERT_EQ(rc3, 0);

    int free1 = uvzmq_socket_free(socket1);
    int free2 = uvzmq_socket_free(socket2);
    int free3 = uvzmq_socket_free(socket3);

    EXPECT_EQ(free1, 0);
    EXPECT_EQ(free2, 0);
    EXPECT_EQ(free3, 0);

    zmq_close(zmq_sock2);
    zmq_close(zmq_sock3);
}

/**
 * @brief Test free cleans up poll handle
 */
TEST_F(UVZMQSocketFreeTest, FreeCleansUpPollHandle) {
    uvzmq_socket_t* socket = nullptr;
    int rc1 = uvzmq_socket_new(&loop, zmq_sock, nullptr, nullptr, &socket);
    ASSERT_EQ(rc1, 0);
    ASSERT_NE(socket->poll_handle, nullptr);

    uv_poll_t* poll_handle = socket->poll_handle;

    int rc2 = uvzmq_socket_free(socket);
    EXPECT_EQ(rc2, 0);

    // Run loop to process async close
    uv_run(&loop, UV_RUN_NOWAIT);

    // Poll handle should be cleaned up (freed in on_close_callback)
    // We can't directly check if it's freed, but the test should not crash
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}