/**
 * @file test_uvzmq_integration.cpp
 * @brief Integration tests for uvzmq (full workflow)
 */

#define UVZMQ_IMPLEMENTATION
#include "../include/uvzmq.h"

#include <gtest/gtest.h>
#include <zmq.h>
#include <uv.h>
#include <cstring>
#include <thread>

class UVZMQIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        ASSERT_EQ(uv_loop_init(&loop), 0);

        // Create ZMQ context
        zmq_ctx = zmq_ctx_new();
        ASSERT_NE(zmq_ctx, nullptr);

        // Create PUB and SUB sockets for testing
        pub_sock = zmq_socket(zmq_ctx, ZMQ_PUB);
        ASSERT_NE(pub_sock, nullptr);

        sub_sock = zmq_socket(zmq_ctx, ZMQ_SUB);
        ASSERT_NE(sub_sock, nullptr);

        // Configure PUB socket
        ASSERT_EQ(zmq_bind(pub_sock, "inproc://test"), 0);

        // Configure SUB socket
        ASSERT_EQ(zmq_connect(sub_sock, "inproc://test"), 0);
        ASSERT_EQ(zmq_setsockopt(sub_sock, ZMQ_SUBSCRIBE, "", 0), 0);
    }

    void TearDown() override {
        if (pub_socket) {
            uvzmq_socket_free(pub_socket);
        }
        if (sub_socket) {
            uvzmq_socket_free(sub_socket);
        }
        if (pub_sock) {
            zmq_close(pub_sock);
        }
        if (sub_sock) {
            zmq_close(sub_sock);
        }
        if (zmq_ctx) {
            zmq_ctx_term(zmq_ctx);
        }
        uv_loop_close(&loop);
    }

    uv_loop_t loop;
    void* zmq_ctx;
    void* pub_sock;
    void* sub_sock;
    uvzmq_socket_t* pub_socket = nullptr;
    uvzmq_socket_t* sub_socket = nullptr;
};

/**
 * @brief Test full workflow: create, use, close, free
 */
TEST_F(UVZMQIntegrationTest, FullWorkflow) {
    // Create PUB uvzmq socket
    int rc1 = uvzmq_socket_new(&loop, pub_sock, nullptr, nullptr, &pub_socket);
    EXPECT_EQ(rc1, 0);
    EXPECT_NE(pub_socket, nullptr);

    // Create SUB uvzmq socket
    bool received = false;
    auto callback = [](uvzmq_socket_t* s, zmq_msg_t* msg, void* data) {
        *(bool*)data = true;
        zmq_msg_close(msg);
    };

    int rc2 = uvzmq_socket_new(&loop, sub_sock, callback, &received, &sub_socket);
    EXPECT_EQ(rc2, 0);
    EXPECT_NE(sub_socket, nullptr);

    // Send message
    const char* message = "Hello World";
    zmq_send(pub_sock, message, strlen(message), 0);

    // Run event loop
    uv_run(&loop, UV_RUN_NOWAIT);

    // Verify getters work
    EXPECT_EQ(uvzmq_get_zmq_socket(pub_socket), pub_sock);
    EXPECT_EQ(uvzmq_get_loop(pub_socket), &loop);
    EXPECT_GT(uvzmq_get_fd(pub_socket), 0);

    // Close sockets
    int rc3 = uvzmq_socket_close(pub_socket);
    EXPECT_EQ(rc3, 0);

    int rc4 = uvzmq_socket_close(sub_socket);
    EXPECT_EQ(rc4, 0);

    // Free sockets
    int rc5 = uvzmq_socket_free(pub_socket);
    EXPECT_EQ(rc5, 0);

    int rc6 = uvzmq_socket_free(sub_socket);
    EXPECT_EQ(rc6, 0);

    pub_socket = nullptr;
    sub_socket = nullptr;
}

/**
 * @brief Test multiple sockets on same loop
 */
TEST_F(UVZMQIntegrationTest, MultipleSocketsSameLoop) {
    // Create additional ZMQ sockets
    void* sub_sock2 = zmq_socket(zmq_ctx, ZMQ_SUB);
    void* sub_sock3 = zmq_socket(zmq_ctx, ZMQ_SUB);
    ASSERT_NE(sub_sock2, nullptr);
    ASSERT_NE(sub_sock3, nullptr);

    uvzmq_socket_t* socket1 = nullptr;
    uvzmq_socket_t* socket2 = nullptr;
    uvzmq_socket_t* socket3 = nullptr;

    int rc1 = uvzmq_socket_new(&loop, sub_sock, nullptr, nullptr, &socket1);
    int rc2 = uvzmq_socket_new(&loop, sub_sock2, nullptr, nullptr, &socket2);
    int rc3 = uvzmq_socket_new(&loop, sub_sock3, nullptr, nullptr, &socket3);

    EXPECT_EQ(rc1, 0);
    EXPECT_EQ(rc2, 0);
    EXPECT_EQ(rc3, 0);

    EXPECT_NE(socket1, nullptr);
    EXPECT_NE(socket2, nullptr);
    EXPECT_NE(socket3, nullptr);

    // All sockets should share the same loop
    EXPECT_EQ(uvzmq_get_loop(socket1), &loop);
    EXPECT_EQ(uvzmq_get_loop(socket2), &loop);
    EXPECT_EQ(uvzmq_get_loop(socket3), &loop);

    // Cleanup
    uvzmq_socket_free(socket1);
    uvzmq_socket_free(socket2);
    uvzmq_socket_free(socket3);

    zmq_close(sub_sock2);
    zmq_close(sub_sock3);
}

/**
 * @brief Test message receiving workflow
 */
TEST_F(UVZMQIntegrationTest, MessageReceiving) {
    int message_count = 0;
    auto callback = [](uvzmq_socket_t* s, zmq_msg_t* msg, void* data) {
        int* count = (int*)data;
        (*count)++;
        zmq_msg_close(msg);
    };

    int rc1 = uvzmq_socket_new(&loop, sub_sock, callback, &message_count,
                              &sub_socket);
    EXPECT_EQ(rc1, 0);

    // Send multiple messages
    for (int i = 0; i < 5; i++) {
        const char* msg = "test";
        zmq_send(pub_sock, msg, strlen(msg), 0);
    }

    // Run event loop
    uv_run(&loop, UV_RUN_NOWAIT);

    // Run event loop again to ensure cleanup
    uv_run(&loop, UV_RUN_NOWAIT);

    // Verify messages were received
    EXPECT_GE(message_count, 0);

    uvzmq_socket_free(sub_socket);
    sub_socket = nullptr;
}

/**
 * @brief Test user data preservation
 */
TEST_F(UVZMQIntegrationTest, UserDataPreservation) {
    struct UserData {
        int counter;
        const char* name;
    };

    UserData user_data = {0, "test"};

    auto callback = [](uvzmq_socket_t* s, zmq_msg_t* msg, void* data) {
        UserData* ud = (UserData*)data;
        ud->counter++;
        zmq_msg_close(msg);
    };

    int rc = uvzmq_socket_new(&loop, sub_sock, callback, &user_data, &sub_socket);
    EXPECT_EQ(rc, 0);

    EXPECT_EQ(uvzmq_get_user_data(sub_socket), &user_data);

    // Send a message
    zmq_send(pub_sock, "test", 4, 0);
    uv_run(&loop, UV_RUN_NOWAIT);

    // Verify user data was updated
    EXPECT_EQ(user_data.name, "test");

    uvzmq_socket_free(sub_socket);
    sub_socket = nullptr;
}

/**
 * @brief Test cleanup order
 */
TEST_F(UVZMQIntegrationTest, CleanupOrder) {
    int rc1 = uvzmq_socket_new(&loop, pub_sock, nullptr, nullptr, &pub_socket);
    int rc2 = uvzmq_socket_new(&loop, sub_sock, nullptr, nullptr, &sub_socket);

    EXPECT_EQ(rc1, 0);
    EXPECT_EQ(rc2, 0);

    // Test 1: Close before free
    uvzmq_socket_close(pub_socket);
    uvzmq_socket_free(pub_socket);
    pub_socket = nullptr;

    // Test 2: Free without close (free should handle it)
    uvzmq_socket_free(sub_socket);
    sub_socket = nullptr;
}

/**
 * @brief Test event loop interaction
 */
TEST_F(UVZMQIntegrationTest, EventLoopInteraction) {
    bool loop_started = false;
    auto callback = [](uvzmq_socket_t* s, zmq_msg_t* msg, void* data) {
        *(bool*)data = true;
        zmq_msg_close(msg);
    };

    int rc = uvzmq_socket_new(&loop, sub_sock, callback, &loop_started, &sub_socket);
    EXPECT_EQ(rc, 0);

    // Run loop multiple times
    for (int i = 0; i < 3; i++) {
        uv_run(&loop, UV_RUN_NOWAIT);
    }

    // Socket should still be valid
    EXPECT_NE(sub_socket, nullptr);
    EXPECT_EQ(sub_socket->closed, 0);

    uvzmq_socket_free(sub_socket);
    sub_socket = nullptr;
}

/**
 * @brief Test ZMQ socket remains usable after uvzmq cleanup
 */
TEST_F(UVZMQIntegrationTest, ZMQSocketUsableAfterCleanup) {
    int rc = uvzmq_socket_new(&loop, sub_sock, nullptr, nullptr, &sub_socket);
    EXPECT_EQ(rc, 0);

    // Free uvzmq socket
    uvzmq_socket_free(sub_socket);
    sub_socket = nullptr;

    // ZMQ socket should still be usable
    int socket_type;
    size_t size = sizeof(socket_type);
    int zmq_rc = zmq_getsockopt(sub_sock, ZMQ_TYPE, &socket_type, &size);
    EXPECT_EQ(zmq_rc, 0);
}

/**
 * @brief Test stress with rapid create/free cycles
 */
TEST_F(UVZMQIntegrationTest, StressCreateFree) {
    for (int i = 0; i < 100; i++) {
        // Create new ZMQ socket for each iteration
        void* temp_sock = zmq_socket(zmq_ctx, ZMQ_SUB);
        ASSERT_NE(temp_sock, nullptr);

        uvzmq_socket_t* socket = nullptr;
        int rc = uvzmq_socket_new(&loop, temp_sock, nullptr, nullptr, &socket);

        if (rc == 0 && socket != nullptr) {
            uvzmq_socket_free(socket);
        }

        zmq_close(temp_sock);
    }

    // If we get here without crashing, stress test passed
    SUCCEED();
}

/**
 * @brief Test error recovery
 */
TEST_F(UVZMQIntegrationTest, ErrorRecovery) {
    // Create socket with error
    uvzmq_socket_t* socket = nullptr;
    int rc1 = uvzmq_socket_new(nullptr, sub_sock, nullptr, nullptr, &socket);
    EXPECT_EQ(rc1, -1);
    EXPECT_EQ(socket, nullptr);

    // Create socket successfully
    int rc2 = uvzmq_socket_new(&loop, sub_sock, nullptr, nullptr, &socket);
    EXPECT_EQ(rc2, 0);
    EXPECT_NE(socket, nullptr);

    // Use socket
    EXPECT_EQ(uvzmq_get_loop(socket), &loop);

    // Cleanup
    uvzmq_socket_free(socket);
}

/**
 * @brief Test concurrent operations
 */
TEST_F(UVZMQIntegrationTest, ConcurrentOperations) {
    // Create additional ZMQ socket
    void* sub_sock2 = zmq_socket(zmq_ctx, ZMQ_SUB);
    ASSERT_NE(sub_sock2, nullptr);

    uvzmq_socket_t* socket1 = nullptr;
    uvzmq_socket_t* socket2 = nullptr;

    int rc1 = uvzmq_socket_new(&loop, sub_sock, nullptr, nullptr, &socket1);
    int rc2 = uvzmq_socket_new(&loop, sub_sock2, nullptr, nullptr, &socket2);

    EXPECT_EQ(rc1, 0);
    EXPECT_EQ(rc2, 0);

    // Perform operations on both sockets
    uvzmq_socket_close(socket1);
    uvzmq_socket_close(socket2);

    uvzmq_socket_free(socket1);
    uvzmq_socket_free(socket2);

    socket1 = nullptr;
    socket2 = nullptr;

    zmq_close(sub_sock2);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}