/**
 * @file test_uvzmq_getters.cpp
 * @brief Unit tests for uvzmq getter functions
 */

#define UVZMQ_IMPLEMENTATION
#include "../include/uvzmq.h"

#include <gtest/gtest.h>
#include <zmq.h>
#include <uv.h>

class UVZMQGettersTest : public ::testing::Test {
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
        uv_loop_close(&loop);
    }

    uv_loop_t loop;
    void* zmq_ctx;
    void* zmq_sock;
    uvzmq_socket_t* socket;
};

/**
 * @brief Test uvzmq_get_zmq_socket
 */
TEST_F(UVZMQGettersTest, GetZMQSocket) {
    void* result = uvzmq_get_zmq_socket(socket);
    EXPECT_EQ(result, zmq_sock);
}

/**
 * @brief Test uvzmq_get_zmq_socket with NULL
 */
TEST_F(UVZMQGettersTest, GetZMQSocketNull) {
    void* result = uvzmq_get_zmq_socket(nullptr);
    EXPECT_EQ(result, nullptr);
}

/**
 * @brief Test uvzmq_get_loop
 */
TEST_F(UVZMQGettersTest, GetLoop) {
    uv_loop_t* result = uvzmq_get_loop(socket);
    EXPECT_EQ(result, &loop);
}

/**
 * @brief Test uvzmq_get_loop with NULL
 */
TEST_F(UVZMQGettersTest, GetLoopNull) {
    uv_loop_t* result = uvzmq_get_loop(nullptr);
    EXPECT_EQ(result, nullptr);
}

/**
 * @brief Test uvzmq_get_user_data
 */
TEST_F(UVZMQGettersTest, GetUserData) {
    int user_data = 42;
    socket->user_data = &user_data;

    void* result = uvzmq_get_user_data(socket);
    EXPECT_EQ(result, &user_data);
}

/**
 * @brief Test uvzmq_get_user_data with NULL
 */
TEST_F(UVZMQGettersTest, GetUserDataNull) {
    void* result = uvzmq_get_user_data(nullptr);
    EXPECT_EQ(result, nullptr);
}

/**
 * @brief Test uvzmq_get_user_data returns NULL initially
 */
TEST_F(UVZMQGettersTest, GetUserDataInitial) {
    void* result = uvzmq_get_user_data(socket);
    EXPECT_EQ(result, nullptr);
}

/**
 * @brief Test uvzmq_get_fd
 */
TEST_F(UVZMQGettersTest, GetFD) {
    int result = uvzmq_get_fd(socket);
    EXPECT_GT(result, 0);
}

/**
 * @brief Test uvzmq_get_fd with NULL
 */
TEST_F(UVZMQGettersTest, GetFDNull) {
    int result = uvzmq_get_fd(nullptr);
    EXPECT_EQ(result, -1);
}

/**
 * @brief Test uvzmq_get_fd matches ZMQ socket FD
 */
TEST_F(UVZMQGettersTest, GetFDMatchesZMQFD) {
    int uvzmq_fd = uvzmq_get_fd(socket);

    int zmq_fd;
    size_t fd_size = sizeof(zmq_fd);
    int rc = zmq_getsockopt(zmq_sock, ZMQ_FD, &zmq_fd, &fd_size);
    ASSERT_EQ(rc, 0);

    EXPECT_EQ(uvzmq_fd, zmq_fd);
}

/**
 * @brief Test all getters return valid values
 */
TEST_F(UVZMQGettersTest, AllGettersValid) {
    int user_data = 123;

    socket->user_data = &user_data;

    void* zmq_sock_result = uvzmq_get_zmq_socket(socket);
    uv_loop_t* loop_result = uvzmq_get_loop(socket);
    void* user_data_result = uvzmq_get_user_data(socket);
    int fd_result = uvzmq_get_fd(socket);

    EXPECT_EQ(zmq_sock_result, zmq_sock);
    EXPECT_EQ(loop_result, &loop);
    EXPECT_EQ(user_data_result, &user_data);
    EXPECT_GT(fd_result, 0);
}

/**
 * @brief Test getters after close
 */
TEST_F(UVZMQGettersTest, GettersAfterClose) {
    uvzmq_socket_close(socket);

    void* zmq_sock_result = uvzmq_get_zmq_socket(socket);
    uv_loop_t* loop_result = uvzmq_get_loop(socket);
    void* user_data_result = uvzmq_get_user_data(socket);
    int fd_result = uvzmq_get_fd(socket);

    EXPECT_EQ(zmq_sock_result, zmq_sock);
    EXPECT_EQ(loop_result, &loop);
    EXPECT_EQ(user_data_result, nullptr);
    EXPECT_GT(fd_result, 0);
}

/**
 * @brief Test getters after free
 */
TEST_F(UVZMQGettersTest, GettersAfterFree) {
    void* saved_zmq_sock = socket->zmq_sock;
    uv_loop_t* saved_loop = socket->loop;
    int saved_fd = socket->zmq_fd;

    uvzmq_socket_free(socket);
    socket = nullptr;

    // All getters should handle NULL gracefully
    void* zmq_sock_result = uvzmq_get_zmq_socket(socket);
    uv_loop_t* loop_result = uvzmq_get_loop(socket);
    void* user_data_result = uvzmq_get_user_data(socket);
    int fd_result = uvzmq_get_fd(socket);

    EXPECT_EQ(zmq_sock_result, nullptr);
    EXPECT_EQ(loop_result, nullptr);
    EXPECT_EQ(user_data_result, nullptr);
    EXPECT_EQ(fd_result, -1);
}

/**
 * @brief Test getter with pointer user data
 */
TEST_F(UVZMQGettersTest, GetUserDataWithPointer) {
    char buffer[128];
    socket->user_data = buffer;

    void* result = uvzmq_get_user_data(socket);
    EXPECT_EQ(result, buffer);
}

/**
 * @brief Test getter with complex user data
 */
TEST_F(UVZMQGettersTest, GetUserDataWithComplexType) {
    struct ComplexData {
        int a;
        double b;
        char c;
    };

    ComplexData data = {1, 2.5, 'x'};
    socket->user_data = &data;

    void* result = uvzmq_get_user_data(socket);
    EXPECT_EQ(result, &data);

    ComplexData* casted = (ComplexData*)result;
    EXPECT_EQ(casted->a, 1);
    EXPECT_DOUBLE_EQ(casted->b, 2.5);
    EXPECT_EQ(casted->c, 'x');
}

/**
 * @brief Test getter functions are inline
 */
TEST_F(UVZMQGettersTest, InlineGetters) {
    // This test verifies that getters can be called multiple times
    // efficiently (inline functions should be fast)

    for (int i = 0; i < 1000; i++) {
        uvzmq_get_zmq_socket(socket);
        uvzmq_get_loop(socket);
        uvzmq_get_user_data(socket);
        uvzmq_get_fd(socket);
    }

    // If we get here without hanging or crashing, inline getters work
    SUCCEED();
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}