#define UVZMQ_IMPLEMENTATION
#include <gtest/gtest.h>
#include <uv.h>
#include <zmq.h>
#include "../include/uvzmq.h"

/**
 * Test Suite: UVZMQ Edge Cases and Stress Tests
 */

class UVZMQEdgeCasesTest : public ::testing::Test {
protected:
    void SetUp() override {
        uv_loop_init(&loop);
        zmq_ctx = zmq_ctx_new();
        ASSERT_NE(zmq_ctx, nullptr);
    }

    void TearDown() override {
        uv_loop_close(&loop);
        zmq_ctx_term(zmq_ctx);
    }

    uv_loop_t loop;
    void* zmq_ctx = nullptr;
};

TEST_F(UVZMQEdgeCasesTest, BasicOperation) {
    void* zmq_sock = zmq_socket(zmq_ctx, ZMQ_REP);
    ASSERT_NE(zmq_sock, nullptr);
    zmq_bind(zmq_sock, "tcp://127.0.0.1:5560");

    uvzmq_socket_t* socket = nullptr;
    int rc = uvzmq_socket_new(&loop, zmq_sock, nullptr, nullptr, &socket);
    EXPECT_EQ(rc, 0);
    EXPECT_NE(socket, nullptr);

    uvzmq_socket_free(socket);
    zmq_close(zmq_sock);
}

TEST_F(UVZMQEdgeCasesTest, NullLoop) {
    void* zmq_sock = zmq_socket(zmq_ctx, ZMQ_REP);
    ASSERT_NE(zmq_sock, nullptr);

    uvzmq_socket_t* socket = nullptr;
    int rc = uvzmq_socket_new(nullptr, zmq_sock, nullptr, nullptr, &socket);
    EXPECT_EQ(rc, -1);
    EXPECT_EQ(socket, nullptr);

    zmq_close(zmq_sock);
}

TEST_F(UVZMQEdgeCasesTest, NullZMQSocket) {
    uvzmq_socket_t* socket = nullptr;
    int rc = uvzmq_socket_new(&loop, nullptr, nullptr, nullptr, &socket);
    EXPECT_EQ(rc, -1);
    EXPECT_EQ(socket, nullptr);
}

TEST_F(UVZMQEdgeCasesTest, NullOutputPointer) {
    void* zmq_sock = zmq_socket(zmq_ctx, ZMQ_REP);
    int rc = uvzmq_socket_new(&loop, zmq_sock, nullptr, nullptr, nullptr);
    EXPECT_EQ(rc, -1);
    zmq_close(zmq_sock);
}

TEST_F(UVZMQEdgeCasesTest, NullCallback) {
    void* zmq_sock = zmq_socket(zmq_ctx, ZMQ_REP);
    zmq_bind(zmq_sock, "tcp://127.0.0.1:5561");

    uvzmq_socket_t* socket = nullptr;
    int rc = uvzmq_socket_new(&loop, zmq_sock, nullptr, nullptr, &socket);
    EXPECT_EQ(rc, 0);
    EXPECT_NE(socket, nullptr);

    uvzmq_socket_free(socket);
    zmq_close(zmq_sock);
}

TEST_F(UVZMQEdgeCasesTest, MultipleSocketsSameLoop) {
    void* sock1 = zmq_socket(zmq_ctx, ZMQ_REP);
    void* sock2 = zmq_socket(zmq_ctx, ZMQ_REP);
    zmq_bind(sock1, "tcp://127.0.0.1:5562");
    zmq_bind(sock2, "tcp://127.0.0.1:5563");

    uvzmq_socket_t* uvzmq_sock1 = nullptr;
    uvzmq_socket_t* uvzmq_sock2 = nullptr;

    int rc1 = uvzmq_socket_new(&loop, sock1, nullptr, nullptr, &uvzmq_sock1);
    int rc2 = uvzmq_socket_new(&loop, sock2, nullptr, nullptr, &uvzmq_sock2);

    EXPECT_EQ(rc1, 0);
    EXPECT_EQ(rc2, 0);
    EXPECT_NE(uvzmq_sock1, nullptr);
    EXPECT_NE(uvzmq_sock2, nullptr);
    EXPECT_NE(uvzmq_sock1, uvzmq_sock2);

    uvzmq_socket_free(uvzmq_sock1);
    uvzmq_socket_free(uvzmq_sock2);
    zmq_close(sock1);
    zmq_close(sock2);
}

TEST_F(UVZMQEdgeCasesTest, RapidCreateFree) {
    for (int i = 0; i < 10; i++) {
        void* zmq_sock = zmq_socket(zmq_ctx, ZMQ_REP);
        zmq_bind(zmq_sock, "tcp://127.0.0.1:5564");

        uvzmq_socket_t* socket = nullptr;
        int rc = uvzmq_socket_new(&loop, zmq_sock, nullptr, nullptr, &socket);
        EXPECT_EQ(rc, 0);

        uvzmq_socket_free(socket);
        zmq_close(zmq_sock);
    }
}

TEST_F(UVZMQEdgeCasesTest, CloseWithoutStop) {
    void* zmq_sock = zmq_socket(zmq_ctx, ZMQ_REP);
    zmq_bind(zmq_sock, "tcp://127.0.0.1:5565");

    uvzmq_socket_t* socket = nullptr;
    uvzmq_socket_new(&loop, zmq_sock, nullptr, nullptr, &socket);
    ASSERT_GT(uvzmq_get_fd(socket), 0);

    int rc = uvzmq_socket_close(socket);
    EXPECT_EQ(rc, 0);
    EXPECT_EQ(1, socket->closed);

    uvzmq_socket_free(socket);
    zmq_close(zmq_sock);
}

TEST_F(UVZMQEdgeCasesTest, DoubleClose) {
    void* zmq_sock = zmq_socket(zmq_ctx, ZMQ_REP);
    zmq_bind(zmq_sock, "tcp://127.0.0.1:5566");

    uvzmq_socket_t* socket = nullptr;
    uvzmq_socket_new(&loop, zmq_sock, nullptr, nullptr, &socket);

    int rc1 = uvzmq_socket_close(socket);
    int rc2 = uvzmq_socket_close(socket);

    EXPECT_EQ(rc1, 0);
    EXPECT_EQ(rc2, -1);

    uvzmq_socket_free(socket);
    zmq_close(zmq_sock);
}

TEST_F(UVZMQEdgeCasesTest, DoubleFree) {
    void* zmq_sock = zmq_socket(zmq_ctx, ZMQ_REP);
    zmq_bind(zmq_sock, "tcp://127.0.0.1:5567");

    uvzmq_socket_t* socket = nullptr;
    uvzmq_socket_new(&loop, zmq_sock, nullptr, nullptr, &socket);

    int rc1 = uvzmq_socket_free(socket);
    socket = nullptr; // User must set to nullptr after free
    int rc2 = uvzmq_socket_free(socket); // Free nullptr should fail

    EXPECT_EQ(rc1, 0);
    EXPECT_EQ(rc2, -1);

    zmq_close(zmq_sock);
}

TEST_F(UVZMQEdgeCasesTest, GettersOnNullSocket) {
    EXPECT_EQ(uvzmq_get_zmq_socket(nullptr), nullptr);
    EXPECT_EQ(uvzmq_get_loop(nullptr), nullptr);
    EXPECT_EQ(uvzmq_get_user_data(nullptr), nullptr);
    EXPECT_EQ(uvzmq_get_fd(nullptr), -1);
}

TEST_F(UVZMQEdgeCasesTest, GettersAfterFree) {
    void* zmq_sock = zmq_socket(zmq_ctx, ZMQ_REP);
    zmq_bind(zmq_sock, "tcp://127.0.0.1:5568");

    uvzmq_socket_t* socket = nullptr;
    uvzmq_socket_new(&loop, zmq_sock, nullptr, nullptr, &socket);

    uvzmq_socket_free(socket);

    EXPECT_EQ(uvzmq_get_zmq_socket(socket), zmq_sock);
    EXPECT_EQ(uvzmq_get_loop(socket), &loop);
    EXPECT_EQ(uvzmq_get_user_data(socket), nullptr);

    zmq_close(zmq_sock);
}

TEST_F(UVZMQEdgeCasesTest, UserDataPreservation) {
    int test_data = 42;
    void* zmq_sock = zmq_socket(zmq_ctx, ZMQ_REP);
    zmq_bind(zmq_sock, "tcp://127.0.0.1:5569");

    uvzmq_socket_t* socket = nullptr;
    uvzmq_socket_new(&loop, zmq_sock, nullptr, &test_data, &socket);

    EXPECT_EQ(uvzmq_get_user_data(socket), (void*)&test_data);

    uvzmq_socket_free(socket);
    zmq_close(zmq_sock);
}

TEST_F(UVZMQEdgeCasesTest, ZeroUserData) {
    void* zmq_sock = zmq_socket(zmq_ctx, ZMQ_REP);
    zmq_bind(zmq_sock, "tcp://127.0.0.1:5570");

    uvzmq_socket_t* socket = nullptr;
    uvzmq_socket_new(&loop, zmq_sock, nullptr, (void*)0, &socket);

    EXPECT_EQ(uvzmq_get_user_data(socket), (void*)0);

    uvzmq_socket_free(socket);
    zmq_close(zmq_sock);
}

TEST_F(UVZMQEdgeCasesTest, LargeUserData) {
    char large_data[1024];
    memset(large_data, 'A', sizeof(large_data));

    void* zmq_sock = zmq_socket(zmq_ctx, ZMQ_REP);
    zmq_bind(zmq_sock, "tcp://127.0.0.1:5571");

    uvzmq_socket_t* socket = nullptr;
    uvzmq_socket_new(&loop, zmq_sock, nullptr, large_data, &socket);

    EXPECT_EQ(uvzmq_get_user_data(socket), large_data);

    uvzmq_socket_free(socket);
    zmq_close(zmq_sock);
}

TEST_F(UVZMQEdgeCasesTest, IPCTransport) {
    void* zmq_sock = zmq_socket(zmq_ctx, ZMQ_REP);
    zmq_bind(zmq_sock, "ipc:///tmp/uvzmq_test_ipc");

    uvzmq_socket_t* socket = nullptr;
    int rc = uvzmq_socket_new(&loop, zmq_sock, nullptr, nullptr, &socket);

    EXPECT_EQ(rc, 0);
    EXPECT_GE(uvzmq_get_fd(socket), 0);

    uvzmq_socket_free(socket);
    zmq_close(zmq_sock);
}

TEST_F(UVZMQEdgeCasesTest, INPROCTransport) {
    void* zmq_sock = zmq_socket(zmq_ctx, ZMQ_REP);
    zmq_bind(zmq_sock, "inproc://uvzmq_test");

    uvzmq_socket_t* socket = nullptr;
    int rc = uvzmq_socket_new(&loop, zmq_sock, nullptr, nullptr, &socket);

    EXPECT_EQ(rc, 0);
    EXPECT_GE(uvzmq_get_fd(socket), 0);

    uvzmq_socket_free(socket);
    zmq_close(zmq_sock);
}

TEST_F(UVZMQEdgeCasesTest, ErrorRecovery) {
    void* zmq_sock = zmq_socket(zmq_ctx, ZMQ_REP);
    zmq_bind(zmq_sock, "tcp://127.0.0.1:5572");

    uvzmq_socket_t* socket = nullptr;
    int rc = uvzmq_socket_new(&loop, zmq_sock, nullptr, nullptr, &socket);
    EXPECT_EQ(rc, 0);

    uvzmq_socket_close(socket);
    uvzmq_socket_free(socket);

    uvzmq_socket_t* socket2 = nullptr;
    rc = uvzmq_socket_new(&loop, zmq_sock, nullptr, nullptr, &socket2);
    EXPECT_EQ(rc, 0);

    uvzmq_socket_free(socket2);
    zmq_close(zmq_sock);
}

TEST_F(UVZMQEdgeCasesTest, PollHandleCleanupOrder) {
    void* zmq_sock = zmq_socket(zmq_ctx, ZMQ_REP);
    zmq_bind(zmq_sock, "tcp://127.0.0.1:5573");

    uvzmq_socket_t* socket = nullptr;
    uvzmq_socket_new(&loop, zmq_sock, nullptr, nullptr, &socket);

    uv_poll_t* poll_handle = socket->poll_handle;
    EXPECT_NE(poll_handle, nullptr);

    uvzmq_socket_free(socket);
    EXPECT_EQ(socket->poll_handle, nullptr);

    zmq_close(zmq_sock);
}

TEST_F(UVZMQEdgeCasesTest, AllSocketTypes) {
    int socket_types[] = {ZMQ_PAIR, ZMQ_REQ, ZMQ_REP, ZMQ_DEALER, ZMQ_ROUTER, ZMQ_PULL, ZMQ_PUSH, ZMQ_PUB, ZMQ_SUB, ZMQ_XPUB, ZMQ_XSUB};
    const char* type_names[] = {"PAIR", "REQ", "REP", "DEALER", "ROUTER", "PULL", "PUSH", "PUB", "SUB", "XPUB", "XSUB"};
    int num_types = sizeof(socket_types) / sizeof(socket_types[0]);

    for (int i = 0; i < num_types; i++) {
        void* zmq_sock = zmq_socket(zmq_ctx, socket_types[i]);
        char bind_addr[64];
        snprintf(bind_addr, sizeof(bind_addr), "ipc:///tmp/uvzmq_test_%s_%d", type_names[i], i);
        zmq_bind(zmq_sock, bind_addr);

        uvzmq_socket_t* socket = nullptr;
        int rc = uvzmq_socket_new(&loop, zmq_sock, nullptr, nullptr, &socket);
        EXPECT_EQ(rc, 0) << "Failed for socket type: " << type_names[i];
        EXPECT_NE(socket, nullptr) << "Null socket for type: " << type_names[i];
        EXPECT_GE(uvzmq_get_fd(socket), 0) << "Invalid FD for type: " << type_names[i];

        uvzmq_socket_free(socket);
        zmq_close(zmq_sock);
    }
}