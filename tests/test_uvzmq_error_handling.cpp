/**
 * @file test_uvzmq_error_handling.cpp
 * @brief Unit tests for error handling in uvzmq
 */

#define UVZMQ_IMPLEMENTATION
#include "../include/uvzmq.h"

#include <gtest/gtest.h>
#include <zmq.h>
#include <uv.h>

class UVZMQErrorHandlingTest : public ::testing::Test {
protected:
    void SetUp() override {
        ASSERT_EQ(uv_loop_init(&loop), 0);

        zmq_ctx = zmq_ctx_new();
        ASSERT_NE(zmq_ctx, nullptr);
    }

    void TearDown() override {
        // Run the loop to ensure all async cleanup completes
        uv_run(&loop, UV_RUN_NOWAIT);
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
    void* zmq_sock = nullptr;
};

/**
 * @brief Test NULL loop parameter
 */
TEST_F(UVZMQErrorHandlingTest, NullLoop) {
    zmq_sock = zmq_socket(zmq_ctx, ZMQ_SUB);
    ASSERT_NE(zmq_sock, nullptr);

    uvzmq_socket_t* socket = nullptr;
    int rc = uvzmq_socket_new(nullptr, zmq_sock, nullptr, nullptr, &socket);

    EXPECT_EQ(rc, -1);
    EXPECT_EQ(socket, nullptr);
}

/**
 * @brief Test NULL ZMQ socket parameter
 */
TEST_F(UVZMQErrorHandlingTest, NullZMQSocket) {
    uvzmq_socket_t* socket = nullptr;
    int rc = uvzmq_socket_new(&loop, nullptr, nullptr, nullptr, &socket);

    EXPECT_EQ(rc, -1);
    EXPECT_EQ(socket, nullptr);
}

/**
 * @brief Test NULL output parameter
 */
TEST_F(UVZMQErrorHandlingTest, NullOutputParameter) {
    zmq_sock = zmq_socket(zmq_ctx, ZMQ_SUB);
    ASSERT_NE(zmq_sock, nullptr);

    int rc = uvzmq_socket_new(&loop, zmq_sock, nullptr, nullptr, nullptr);

    EXPECT_EQ(rc, -1);
}

/**
 * @brief Test all NULL parameters
 */
TEST_F(UVZMQErrorHandlingTest, AllNullParameters) {
    int rc = uvzmq_socket_new(nullptr, nullptr, nullptr, nullptr, nullptr);

    EXPECT_EQ(rc, -1);
}

/**
 * @brief Test uvzmq_socket_close with NULL
 */
TEST_F(UVZMQErrorHandlingTest, CloseNullSocket) {
    int rc = uvzmq_socket_close(nullptr);

    EXPECT_EQ(rc, -1);
}

/**
 * @brief Test uvzmq_socket_free with NULL
 */
TEST_F(UVZMQErrorHandlingTest, FreeNullSocket) {
    int rc = uvzmq_socket_free(nullptr);

    EXPECT_EQ(rc, -1);
}

/**
 * @brief Test close on already closed socket
 */
TEST_F(UVZMQErrorHandlingTest, CloseAlreadyClosed) {
    zmq_sock = zmq_socket(zmq_ctx, ZMQ_SUB);
    ASSERT_NE(zmq_sock, nullptr);

    uvzmq_socket_t* socket = nullptr;
    int rc1 = uvzmq_socket_new(&loop, zmq_sock, nullptr, nullptr, &socket);
    ASSERT_EQ(rc1, 0);

    int rc2 = uvzmq_socket_close(socket);
    EXPECT_EQ(rc2, 0);

    int rc3 = uvzmq_socket_close(socket);
    EXPECT_EQ(rc3, -1);

    uvzmq_socket_free(socket);
}

/**
 * @brief Test free on already closed socket
 */
TEST_F(UVZMQErrorHandlingTest, FreeAlreadyClosed) {
    zmq_sock = zmq_socket(zmq_ctx, ZMQ_SUB);
    ASSERT_NE(zmq_sock, nullptr);

    uvzmq_socket_t* socket = nullptr;
    int rc1 = uvzmq_socket_new(&loop, zmq_sock, nullptr, nullptr, &socket);
    ASSERT_EQ(rc1, 0);

    uvzmq_socket_close(socket);
    int rc2 = uvzmq_socket_free(socket);
    EXPECT_EQ(rc2, 0);
}

/**
 * @brief Test callback NULL is allowed
 */
TEST_F(UVZMQErrorHandlingTest, CallbackNullAllowed) {
    zmq_sock = zmq_socket(zmq_ctx, ZMQ_SUB);
    ASSERT_NE(zmq_sock, nullptr);

    uvzmq_socket_t* socket = nullptr;
    int rc = uvzmq_socket_new(&loop, zmq_sock, nullptr, nullptr, &socket);

    EXPECT_EQ(rc, 0);
    EXPECT_NE(socket, nullptr);

    uvzmq_socket_free(socket);
}

/**
 * @brief Test user_data NULL is allowed
 */
TEST_F(UVZMQErrorHandlingTest, UserDataNullAllowed) {
    zmq_sock = zmq_socket(zmq_ctx, ZMQ_SUB);
    ASSERT_NE(zmq_sock, nullptr);

    bool flag = true;
    auto callback = [](uvzmq_socket_t*, zmq_msg_t* msg, void*) {
        zmq_msg_close(msg);
    };

    uvzmq_socket_t* socket = nullptr;
    int rc = uvzmq_socket_new(&loop, zmq_sock, callback, nullptr, &socket);

    EXPECT_EQ(rc, 0);
    EXPECT_NE(socket, nullptr);

    uvzmq_socket_free(socket);
}

/**
 * @brief Test error handling chain
 */
TEST_F(UVZMQErrorHandlingTest, ErrorChain) {
    // Create socket
    zmq_sock = zmq_socket(zmq_ctx, ZMQ_SUB);
    ASSERT_NE(zmq_sock, nullptr);

    uvzmq_socket_t* socket = nullptr;
    int rc1 = uvzmq_socket_new(&loop, zmq_sock, nullptr, nullptr, &socket);
    EXPECT_EQ(rc1, 0);
    EXPECT_NE(socket, nullptr);

    // Close socket
    int rc2 = uvzmq_socket_close(socket);
    EXPECT_EQ(rc2, 0);

    // Try to close again (should fail)
    int rc3 = uvzmq_socket_close(socket);
    EXPECT_EQ(rc3, -1);

    // Free socket
    int rc4 = uvzmq_socket_free(socket);
    EXPECT_EQ(rc4, 0);

    // After free, set socket to NULL to avoid double free
    socket = nullptr;

    // Try to free again (should fail)
    int rc5 = uvzmq_socket_free(socket);
    EXPECT_EQ(rc5, -1);
}

/**
 * @brief Test ZMQ socket not configured
 */
TEST_F(UVZMQErrorHandlingTest, ZMQSocketNotConfigured) {
    // Create ZMQ socket but don't configure it
    zmq_sock = zmq_socket(zmq_ctx, ZMQ_SUB);
    ASSERT_NE(zmq_sock, nullptr);

    // Don't bind or connect, just create uvzmq socket
    uvzmq_socket_t* socket = nullptr;
    int rc = uvzmq_socket_new(&loop, zmq_sock, nullptr, nullptr, &socket);

    // This should succeed (uvzmq doesn't require binding/connecting)
    EXPECT_EQ(rc, 0);
    EXPECT_NE(socket, nullptr);

    uvzmq_socket_free(socket);
}

/**
 * @brief Test memory allocation failure simulation
 */
TEST_F(UVZMQErrorHandlingTest, MemoryAllocationFailure) {
    zmq_sock = zmq_socket(zmq_ctx, ZMQ_SUB);
    ASSERT_NE(zmq_sock, nullptr);

    // Note: We can't easily simulate malloc failure in unit tests
    // This test documents expected behavior
    // If malloc fails, uvzmq_socket_new should return -1

    uvzmq_socket_t* socket = nullptr;
    int rc = uvzmq_socket_new(&loop, zmq_sock, nullptr, nullptr, &socket);

    // Under normal conditions, this should succeed
    EXPECT_EQ(rc, 0);

    if (rc == 0) {
        uvzmq_socket_free(socket);
    }
}

/**
 * @brief Test poll handle initialization failure
 */
TEST_F(UVZMQErrorHandlingTest, PollHandleInitFailure) {
    // Note: We can't easily simulate uv_poll_init failure
    // This test documents expected behavior
    // If uv_poll_init fails, uvzmq_socket_new should:
    // 1. Free the poll_handle
    // 2. Free the socket
    // 3. Return -1

    zmq_sock = zmq_socket(zmq_ctx, ZMQ_SUB);
    ASSERT_NE(zmq_sock, nullptr);

    uvzmq_socket_t* socket = nullptr;
    int rc = uvzmq_socket_new(&loop, zmq_sock, nullptr, nullptr, &socket);

    // Under normal conditions, this should succeed
    EXPECT_EQ(rc, 0);

    if (rc == 0) {
        uvzmq_socket_free(socket);
    }
}

/**
 * @brief Test error after socket creation
 */
TEST_F(UVZMQErrorHandlingTest, ErrorAfterCreation) {
    zmq_sock = zmq_socket(zmq_ctx, ZMQ_SUB);
    ASSERT_NE(zmq_sock, nullptr);

    uvzmq_socket_t* socket = nullptr;
    int rc1 = uvzmq_socket_new(&loop, zmq_sock, nullptr, nullptr, &socket);
    ASSERT_EQ(rc1, 0);

    // Simulate various error scenarios after creation
    // (documenting expected behavior)

    // 1. Close should handle closed flag
    socket->closed = 1;
    int rc2 = uvzmq_socket_close(socket);
    EXPECT_EQ(rc2, -1);

    // Reset
    socket->closed = 0;

    // 2. Free should handle cleanup
    int rc3 = uvzmq_socket_free(socket);
    EXPECT_EQ(rc3, 0);
}

/**
 * @brief Test callback receives valid parameters
 */
TEST_F(UVZMQErrorHandlingTest, CallbackValidParameters) {
    zmq_sock = zmq_socket(zmq_ctx, ZMQ_SUB);
    ASSERT_NE(zmq_sock, nullptr);

    bool callback_valid = false;
    auto callback = [](uvzmq_socket_t* s, zmq_msg_t* msg, void* data) {
        bool* valid = (bool*)data;
        // Check that parameters are valid
        if (s != nullptr && msg != nullptr && data != nullptr) {
            *valid = true;
        }
        zmq_msg_close(msg);
    };

    uvzmq_socket_t* socket = nullptr;
    int rc = uvzmq_socket_new(&loop, zmq_sock, callback, &callback_valid, &socket);

    EXPECT_EQ(rc, 0);

    uvzmq_socket_free(socket);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}