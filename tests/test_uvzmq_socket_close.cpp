/**
 * @file test_uvzmq_socket_close.cpp
 * @brief Unit tests for uvzmq_socket_close function
 */

#define UVZMQ_IMPLEMENTATION
#include "../include/uvzmq.h"

#include <gtest/gtest.h>
#include <zmq.h>
#include <uv.h>

class UVZMQSocketCloseTest : public ::testing::Test {
protected:
    void SetUp() override {
        ASSERT_EQ(uv_loop_init(&loop), 0);

        zmq_ctx = zmq_ctx_new();
        ASSERT_NE(zmq_ctx, nullptr);

        zmq_sock = zmq_socket(zmq_ctx, ZMQ_SUB);
        ASSERT_NE(zmq_sock, nullptr);

        int rc = uvzmq_socket_new(&loop, zmq_sock, nullptr, nullptr, &socket);
        ASSERT_EQ(rc, 0);
        ASSERT_NE(socket, nullptr);
    }

    void TearDown() override {
        if (socket) {
            uvzmq_socket_free(socket);
        }
        if (zmq_sock) {
            zmq_close(zmq_sock);
        }
        if (zmq_ctx) {
            zmq_ctx_term(zmq_ctx);
        }
        // Run the loop to ensure all async cleanup completes
        uv_run(&loop, UV_RUN_NOWAIT);
        uv_loop_close(&loop);
    }

    uv_loop_t loop;
    void* zmq_ctx;
    void* zmq_sock;
    uvzmq_socket_t* socket;
};

/**
 * @brief Test successful close
 */
TEST_F(UVZMQSocketCloseTest, Success) {
    EXPECT_EQ(socket->closed, 0);

    int rc = uvzmq_socket_close(socket);
    EXPECT_EQ(rc, 0);
    EXPECT_EQ(socket->closed, 1);
}

/**
 * @brief Test double close
 */
TEST_F(UVZMQSocketCloseTest, DoubleClose) {
    int rc1 = uvzmq_socket_close(socket);
    EXPECT_EQ(rc1, 0);
    EXPECT_EQ(socket->closed, 1);

    int rc2 = uvzmq_socket_close(socket);
    EXPECT_EQ(rc2, -1);
}

/**
 * @brief Test close with NULL socket
 */
TEST_F(UVZMQSocketCloseTest, NullSocket) {
    int rc = uvzmq_socket_close(nullptr);
    EXPECT_EQ(rc, -1);
}

/**
 * @brief Test close effect on poll handle
 */
TEST_F(UVZMQSocketCloseTest, CloseEffectOnPollHandle) {
    EXPECT_NE(socket->poll_handle, nullptr);
    EXPECT_EQ(socket->closed, 0);

    uvzmq_socket_close(socket);

    EXPECT_EQ(socket->closed, 1);
    // Poll handle should still exist (not freed)
    EXPECT_NE(socket->poll_handle, nullptr);
}

/**
 * @brief Test close preserves socket structure
 */
TEST_F(UVZMQSocketCloseTest, ClosePreservesStructure) {
    void* saved_loop = socket->loop;
    void* saved_zmq_sock = socket->zmq_sock;
    void* saved_user_data = socket->user_data;
    int saved_fd = socket->zmq_fd;

    uvzmq_socket_close(socket);

    EXPECT_EQ(socket->loop, saved_loop);
    EXPECT_EQ(socket->zmq_sock, saved_zmq_sock);
    EXPECT_EQ(socket->user_data, saved_user_data);
    EXPECT_EQ(socket->zmq_fd, saved_fd);
}

/**
 * @brief Test close with callback
 */
TEST_F(UVZMQSocketCloseTest, CloseWithCallback) {
    bool callback_called = false;
    auto callback = [](uvzmq_socket_t* s, zmq_msg_t* msg, void* data) {
        *(bool*)data = true;
        zmq_msg_close(msg);
    };

    socket->on_recv = callback;
    socket->user_data = &callback_called;

    uvzmq_socket_close(socket);

    EXPECT_EQ(socket->closed, 1);
    EXPECT_EQ(socket->on_recv, callback);
    EXPECT_EQ(socket->user_data, &callback_called);
}

/**
 * @brief Test close effect on event loop
 */
TEST_F(UVZMQSocketCloseTest, CloseEffectOnEventLoop) {
    uvzmq_socket_close(socket);

    // Run event loop - should not process messages
    // (no messages to process, but close flag should prevent processing)
    int rc = uv_run(&loop, UV_RUN_NOWAIT);
    EXPECT_GE(rc, 0);

    EXPECT_EQ(socket->closed, 1);
}

/**
 * @brief Test close after free
 */
TEST_F(UVZMQSocketCloseTest, CloseAfterFree) {
    uvzmq_socket_free(socket);
    socket = nullptr;

    int rc = uvzmq_socket_close(socket);
    EXPECT_EQ(rc, -1);
}

/**
 * @brief Test close before free
 */
TEST_F(UVZMQSocketCloseTest, CloseBeforeFree) {
    int rc1 = uvzmq_socket_close(socket);
    EXPECT_EQ(rc1, 0);

    int rc2 = uvzmq_socket_free(socket);
    EXPECT_EQ(rc2, 0);

    socket = nullptr;
}

/**
 * @brief Test close does not affect ZMQ socket
 */
TEST_F(UVZMQSocketCloseTest, CloseDoesNotAffectZMQSocket) {
    // Verify ZMQ socket is still valid after close
    int socket_type;
    size_t size = sizeof(socket_type);
    int rc1 = zmq_getsockopt(zmq_sock, ZMQ_TYPE, &socket_type, &size);
    EXPECT_EQ(rc1, 0);

    uvzmq_socket_close(socket);

    // ZMQ socket should still be usable
    int rc2 = zmq_getsockopt(zmq_sock, ZMQ_TYPE, &socket_type, &size);
    EXPECT_EQ(rc2, 0);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}