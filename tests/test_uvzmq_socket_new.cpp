/**
 * @file test_uvzmq_socket_new.cpp
 * @brief Unit tests for uvzmq_socket_new function
 */

#define UVZMQ_IMPLEMENTATION
#include "../include/uvzmq.h"

#include <gtest/gtest.h>
#include <zmq.h>
#include <uv.h>

class UVZMQSocketNewTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize libuv loop
        ASSERT_EQ(uv_loop_init(&loop), 0);

        // Create ZMQ context
        zmq_ctx = zmq_ctx_new();
        ASSERT_NE(zmq_ctx, nullptr);

        // Create ZMQ socket
        zmq_sock = zmq_socket(zmq_ctx, ZMQ_SUB);
        ASSERT_NE(zmq_sock, nullptr);
    }

    void TearDown() override {
        // Cleanup ZMQ
        if (zmq_sock) {
            zmq_close(zmq_sock);
        }
        if (zmq_ctx) {
            zmq_ctx_term(zmq_ctx);
        }

        // Cleanup libuv
        uv_loop_close(&loop);
    }

    uv_loop_t loop;
    void* zmq_ctx;
    void* zmq_sock;
};

/**
 * @brief Test successful creation of uvzmq socket
 */
TEST_F(UVZMQSocketNewTest, Success) {
    uvzmq_socket_t* socket = nullptr;
    int rc = uvzmq_socket_new(&loop, zmq_sock, nullptr, nullptr, &socket);

    EXPECT_EQ(rc, 0);
    EXPECT_NE(socket, nullptr);

    // Verify socket structure
    EXPECT_EQ(socket->loop, &loop);
    EXPECT_EQ(socket->zmq_sock, zmq_sock);
    EXPECT_EQ(socket->on_recv, nullptr);
    EXPECT_EQ(socket->user_data, nullptr);
    EXPECT_EQ(socket->closed, 0);
    EXPECT_NE(socket->poll_handle, nullptr);
    EXPECT_GT(socket->zmq_fd, 0);

    // Cleanup
    uvzmq_socket_free(socket);
}

/**
 * @brief Test with valid callback
 */
TEST_F(UVZMQSocketNewTest, WithCallback) {
    bool callback_called = false;
    auto callback = [](uvzmq_socket_t* socket, zmq_msg_t* msg,
                       void* user_data) {
        *(bool*)user_data = true;
        zmq_msg_close(msg);
    };

    uvzmq_socket_t* socket = nullptr;
    int rc = uvzmq_socket_new(&loop, zmq_sock, callback, &callback_called,
                              &socket);

    EXPECT_EQ(rc, 0);
    EXPECT_NE(socket, nullptr);
    EXPECT_EQ(socket->on_recv, callback);
    EXPECT_EQ(socket->user_data, &callback_called);

    // Cleanup
    uvzmq_socket_free(socket);
}

/**
 * @brief Test with user data
 */
TEST_F(UVZMQSocketNewTest, WithUserData) {
    int user_data = 42;
    uvzmq_socket_t* socket = nullptr;
    int rc = uvzmq_socket_new(&loop, zmq_sock, nullptr, &user_data, &socket);

    EXPECT_EQ(rc, 0);
    EXPECT_NE(socket, nullptr);
    EXPECT_EQ(socket->user_data, &user_data);

    // Cleanup
    uvzmq_socket_free(socket);
}

/**
 * @brief Test with NULL loop (should fail)
 */
TEST_F(UVZMQSocketNewTest, NullLoop) {
    uvzmq_socket_t* socket = nullptr;
    int rc = uvzmq_socket_new(nullptr, zmq_sock, nullptr, nullptr, &socket);

    EXPECT_EQ(rc, -1);
    EXPECT_EQ(socket, nullptr);
}

/**
 * @brief Test with NULL ZMQ socket (should fail)
 */
TEST_F(UVZMQSocketNewTest, NullZMQSocket) {
    uvzmq_socket_t* socket = nullptr;
    int rc = uvzmq_socket_new(&loop, nullptr, nullptr, nullptr, &socket);

    EXPECT_EQ(rc, -1);
    EXPECT_EQ(socket, nullptr);
}

/**
 * @brief Test with NULL output parameter (should fail)
 */
TEST_F(UVZMQSocketNewTest, NullOutputParameter) {
    int rc = uvzmq_socket_new(&loop, zmq_sock, nullptr, nullptr, nullptr);

    EXPECT_EQ(rc, -1);
}

/**
 * @brief Test with multiple sockets
 */
TEST_F(UVZMQSocketNewTest, MultipleSockets) {
    // Create additional ZMQ sockets for testing
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

    EXPECT_EQ(rc1, 0);
    EXPECT_EQ(rc2, 0);
    EXPECT_EQ(rc3, 0);

    EXPECT_NE(socket1, nullptr);
    EXPECT_NE(socket2, nullptr);
    EXPECT_NE(socket3, nullptr);

    EXPECT_NE(socket1, socket2);
    EXPECT_NE(socket2, socket3);
    EXPECT_NE(socket1, socket3);

    // Cleanup
    uvzmq_socket_free(socket1);
    uvzmq_socket_free(socket2);
    uvzmq_socket_free(socket3);

    zmq_close(zmq_sock2);
    zmq_close(zmq_sock3);
}

/**
 * @brief Test file descriptor retrieval
 */
TEST_F(UVZMQSocketNewTest, FileDescriptorRetrieval) {
    uvzmq_socket_t* socket = nullptr;
    int rc = uvzmq_socket_new(&loop, zmq_sock, nullptr, nullptr, &socket);

    EXPECT_EQ(rc, 0);
    EXPECT_NE(socket, nullptr);

    // Verify FD is valid
    EXPECT_GT(socket->zmq_fd, 0);

    // Verify FD matches ZMQ socket FD
    int zmq_fd;
    size_t fd_size = sizeof(zmq_fd);
    int get_rc = zmq_getsockopt(zmq_sock, ZMQ_FD, &zmq_fd, &fd_size);
    EXPECT_EQ(get_rc, 0);
    EXPECT_EQ(socket->zmq_fd, zmq_fd);

    // Cleanup
    uvzmq_socket_free(socket);
}

/**
 * @brief Test poll handle initialization
 */
TEST_F(UVZMQSocketNewTest, PollHandleInitialization) {
    uvzmq_socket_t* socket = nullptr;
    int rc = uvzmq_socket_new(&loop, zmq_sock, nullptr, nullptr, &socket);

    EXPECT_EQ(rc, 0);
    EXPECT_NE(socket, nullptr);

    // Verify poll handle is initialized
    EXPECT_NE(socket->poll_handle, nullptr);
    EXPECT_EQ(socket->poll_handle->data, socket);

    // Cleanup
    uvzmq_socket_free(socket);
}

/**
 * @brief Test closed flag initial state
 */
TEST_F(UVZMQSocketNewTest, ClosedFlagInitialState) {
    uvzmq_socket_t* socket = nullptr;
    int rc = uvzmq_socket_new(&loop, zmq_sock, nullptr, nullptr, &socket);

    EXPECT_EQ(rc, 0);
    EXPECT_NE(socket, nullptr);
    EXPECT_EQ(socket->closed, 0);

    // Cleanup
    uvzmq_socket_free(socket);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}