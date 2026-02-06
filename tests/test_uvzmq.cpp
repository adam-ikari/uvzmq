#include "../include/uvzmq.h"
#include <gtest/gtest.h>
#include <string.h>

class UVZMQTest : public ::testing::Test {
protected:
    uv_loop_t loop;
    uvzmq_context_t *ctx;

    void SetUp() override {
        uv_loop_init(&loop);
        int rc = uvzmq_context_new(&loop, &ctx);
        ASSERT_EQ(UVZMQ_OK, rc);
        ASSERT_NE(nullptr, ctx);
    }

    void TearDown() override {
        uvzmq_context_free(ctx);
        uv_loop_close(&loop);
    }
};

TEST_F(UVZMQTest, ContextCreate) {
    uvzmq_context_t *test_ctx = nullptr;
    int rc = uvzmq_context_new(&loop, &test_ctx);
    EXPECT_EQ(UVZMQ_OK, rc);
    EXPECT_NE(nullptr, test_ctx);
    EXPECT_EQ(&loop, uvzmq_context_get_loop(test_ctx));

    rc = uvzmq_context_free(test_ctx);
    EXPECT_EQ(UVZMQ_OK, rc);
}

TEST_F(UVZMQTest, SocketCreate) {
    uvzmq_socket_t *sock = nullptr;
    int rc = uvzmq_socket_new(ctx, UVZMQ_REQ, &sock);
    EXPECT_EQ(UVZMQ_OK, rc);
    EXPECT_NE(nullptr, sock);
    EXPECT_EQ(ctx, uvzmq_socket_get_context(sock));

    rc = uvzmq_socket_free(sock);
    EXPECT_EQ(UVZMQ_OK, rc);
}

TEST_F(UVZMQTest, SocketCreateAllTypes) {
    uvzmq_socket_type_t types[] = {
        UVZMQ_PAIR, UVZMQ_PUB, UVZMQ_SUB, UVZMQ_REQ, UVZMQ_REP,
        UVZMQ_DEALER, UVZMQ_ROUTER, UVZMQ_PULL, UVZMQ_PUSH,
        UVZMQ_XPUB, UVZMQ_XSUB, UVZMQ_STREAM
    };

    for (size_t i = 0; i < sizeof(types) / sizeof(types[0]); i++) {
        uvzmq_socket_t *sock = nullptr;
        int rc = uvzmq_socket_new(ctx, types[i], &sock);
        EXPECT_EQ(UVZMQ_OK, rc) << "Failed to create socket type " << types[i];
        EXPECT_NE(nullptr, sock);

        rc = uvzmq_socket_free(sock);
        EXPECT_EQ(UVZMQ_OK, rc);
    }
}

TEST_F(UVZMQTest, SocketSetsockoptGetInt) {
    uvzmq_socket_t *sock = nullptr;
    int rc = uvzmq_socket_new(ctx, UVZMQ_REQ, &sock);
    ASSERT_EQ(UVZMQ_OK, rc);

    rc = uvzmq_setsockopt_int(sock, ZMQ_LINGER, 100);
    EXPECT_EQ(UVZMQ_OK, rc);

    int value = 0;
    rc = uvzmq_getsockopt_int(sock, ZMQ_LINGER, &value);
    EXPECT_EQ(UVZMQ_OK, rc);
    EXPECT_EQ(100, value);

    uvzmq_socket_free(sock);
}

TEST_F(UVZMQTest, BindConnect) {
    uvzmq_socket_t *rep_sock = nullptr;
    int rc = uvzmq_socket_new(ctx, UVZMQ_REP, &rep_sock);
    ASSERT_EQ(UVZMQ_OK, rc);

    rc = uvzmq_bind(rep_sock, "tcp://*:5555");
    EXPECT_EQ(UVZMQ_OK, rc);

    uvzmq_socket_t *req_sock = nullptr;
    rc = uvzmq_socket_new(ctx, UVZMQ_REQ, &req_sock);
    ASSERT_EQ(UVZMQ_OK, rc);

    rc = uvzmq_setsockopt_int(req_sock, ZMQ_RCVTIMEO, 1000);
    EXPECT_EQ(UVZMQ_OK, rc);

    rc = uvzmq_connect(req_sock, "tcp://127.0.0.1:5555");
    EXPECT_EQ(UVZMQ_OK, rc);

    uvzmq_socket_free(req_sock);
    uvzmq_socket_free(rep_sock);
}

TEST_F(UVZMQTest, SendRecvString) {
    uvzmq_socket_t *rep_sock = nullptr;
    int rc = uvzmq_socket_new(ctx, UVZMQ_REP, &rep_sock);
    ASSERT_EQ(UVZMQ_OK, rc);

    rc = uvzmq_bind(rep_sock, "tcp://*:5556");
    EXPECT_EQ(UVZMQ_OK, rc);

    uvzmq_socket_t *req_sock = nullptr;
    rc = uvzmq_socket_new(ctx, UVZMQ_REQ, &req_sock);
    ASSERT_EQ(UVZMQ_OK, rc);

    rc = uvzmq_setsockopt_int(req_sock, ZMQ_RCVTIMEO, 1000);
    EXPECT_EQ(UVZMQ_OK, rc);

    rc = uvzmq_connect(req_sock, "tcp://127.0.0.1:5556");
    EXPECT_EQ(UVZMQ_OK, rc);

    const char *send_data = "Hello from REQ";
    rc = uvzmq_send_string(req_sock, send_data, 0);
    EXPECT_EQ(UVZMQ_OK, rc);

    char *recv_data = nullptr;
    rc = uvzmq_recv_string(rep_sock, &recv_data, 0);
    EXPECT_EQ(UVZMQ_OK, rc);
    EXPECT_NE(nullptr, recv_data);
    EXPECT_STREQ(send_data, recv_data);
    UVZMQ_FREE(recv_data);

    const char *reply_data = "Reply from REP";
    rc = uvzmq_send_string(rep_sock, reply_data, 0);
    EXPECT_EQ(UVZMQ_OK, rc);

    rc = uvzmq_recv_string(req_sock, &recv_data, 0);
    EXPECT_EQ(UVZMQ_OK, rc);
    EXPECT_NE(nullptr, recv_data);
    EXPECT_STREQ(reply_data, recv_data);
    UVZMQ_FREE(recv_data);

    uvzmq_socket_free(req_sock);
    uvzmq_socket_free(rep_sock);
}

TEST_F(UVZMQTest, SendRecvData) {
    uvzmq_socket_t *rep_sock = nullptr;
    int rc = uvzmq_socket_new(ctx, UVZMQ_REP, &rep_sock);
    ASSERT_EQ(UVZMQ_OK, rc);

    rc = uvzmq_bind(rep_sock, "tcp://*:5557");
    EXPECT_EQ(UVZMQ_OK, rc);

    uvzmq_socket_t *req_sock = nullptr;
    rc = uvzmq_socket_new(ctx, UVZMQ_REQ, &req_sock);
    ASSERT_EQ(UVZMQ_OK, rc);

    rc = uvzmq_setsockopt_int(req_sock, ZMQ_RCVTIMEO, 1000);
    EXPECT_EQ(UVZMQ_OK, rc);

    rc = uvzmq_connect(req_sock, "tcp://127.0.0.1:5557");
    EXPECT_EQ(UVZMQ_OK, rc);

    const char *send_data = "Binary data test";
    rc = uvzmq_send_data(req_sock, send_data, strlen(send_data), 0);
    EXPECT_EQ(UVZMQ_OK, rc);

    char recv_buffer[128];
    size_t bytes_received = 0;
    rc = uvzmq_recv_data(rep_sock, recv_buffer, sizeof(recv_buffer), &bytes_received, 0);
    EXPECT_EQ(UVZMQ_OK, rc);
    EXPECT_EQ(strlen(send_data), bytes_received);
    EXPECT_EQ(0, memcmp(send_data, recv_buffer, bytes_received));

    uvzmq_socket_free(req_sock);
    uvzmq_socket_free(rep_sock);
}

TEST_F(UVZMQTest, Poll) {
    uvzmq_socket_t *rep_sock = nullptr;
    int rc = uvzmq_socket_new(ctx, UVZMQ_REP, &rep_sock);
    ASSERT_EQ(UVZMQ_OK, rc);

    rc = uvzmq_bind(rep_sock, "tcp://*:5558");
    EXPECT_EQ(UVZMQ_OK, rc);

    int events = uvzmq_poll(rep_sock, UVZMQ_POLLIN, 100);
    EXPECT_GE(events, 0);

    uvzmq_socket_free(rep_sock);
}

TEST_F(UVZMQTest, SocketClose) {
    uvzmq_socket_t *sock = nullptr;
    int rc = uvzmq_socket_new(ctx, UVZMQ_REQ, &sock);
    ASSERT_EQ(UVZMQ_OK, rc);

    rc = uvzmq_socket_close(sock);
    EXPECT_EQ(UVZMQ_OK, rc);

    rc = uvzmq_socket_close(sock);
    EXPECT_EQ(UVZMQ_OK, rc);

    uvzmq_socket_free(sock);
}

TEST_F(UVZMQTest, StrError) {
    EXPECT_STREQ("No error", uvzmq_strerror(UVZMQ_OK));
    EXPECT_STREQ("Out of memory", uvzmq_strerror(UVZMQ_ENOMEM));
    EXPECT_STREQ("Invalid argument", uvzmq_strerror(UVZMQ_EINVAL));
}

TEST_F(UVZMQTest, NullParameters) {
    uvzmq_context_t *null_ctx = nullptr;
    int rc = uvzmq_context_new(nullptr, &null_ctx);
    EXPECT_EQ(UVZMQ_EINVAL, rc);

    rc = uvzmq_context_new(&loop, nullptr);
    EXPECT_EQ(UVZMQ_EINVAL, rc);

    rc = uvzmq_context_free(nullptr);
    EXPECT_EQ(UVZMQ_EINVAL, rc);

    uvzmq_socket_t *null_sock = nullptr;
    rc = uvzmq_socket_new(nullptr, UVZMQ_REQ, &null_sock);
    EXPECT_EQ(UVZMQ_EINVAL, rc);

    rc = uvzmq_socket_new(ctx, UVZMQ_REQ, nullptr);
    EXPECT_EQ(UVZMQ_EINVAL, rc);

    rc = uvzmq_bind(nullptr, "tcp://*:5559");
    EXPECT_EQ(UVZMQ_EINVAL, rc);

    rc = uvzmq_connect(nullptr, "tcp://127.0.0.1:5559");
    EXPECT_EQ(UVZMQ_EINVAL, rc);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}