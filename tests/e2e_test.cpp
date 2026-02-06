#include "../include/uvzmq.h"
#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <string.h>

class UVZMQE2ETest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

static void server_thread_func(const char* endpoint, int port, bool* server_ready, bool* server_done) {
    uv_loop_t loop;
    uv_loop_init(&loop);

    uvzmq_context_t *ctx = nullptr;
    int rc = uvzmq_context_new(&loop, &ctx);
    ASSERT_EQ(UVZMQ_OK, rc);

    uvzmq_socket_t *sock = nullptr;
    rc = uvzmq_socket_new(ctx, UVZMQ_REP, &sock);
    ASSERT_EQ(UVZMQ_OK, rc);

    char bind_addr[64];
    snprintf(bind_addr, sizeof(bind_addr), "%s:%d", endpoint, port);
    rc = uvzmq_bind(sock, bind_addr);
    ASSERT_EQ(UVZMQ_OK, rc);

    *server_ready = true;

    int msg_count = 0;
    while (msg_count < 10 && !*server_done) {
        char *recv_data = nullptr;
        rc = uvzmq_recv_string(sock, &recv_data, 0);
        if (rc == UVZMQ_OK && recv_data != nullptr) {
            char reply[128];
            snprintf(reply, sizeof(reply), "Reply: %s", recv_data);
            rc = uvzmq_send_string(sock, reply, 0);
            UVZMQ_FREE(recv_data);
            msg_count++;
        }
    }

    uvzmq_socket_free(sock);
    uvzmq_context_free(ctx);
    uv_loop_close(&loop);
}

TEST_F(UVZMQE2ETest, ReqRepMultiThread) {
    const char *endpoint = "tcp://127.0.0.1";
    const int port = 5600;
    bool server_ready = false;
    bool server_done = false;

    std::thread server_thread(server_thread_func, endpoint, port, &server_ready, &server_done);

    while (!server_ready) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    uv_loop_t loop;
    uv_loop_init(&loop);

    uvzmq_context_t *ctx = nullptr;
    int rc = uvzmq_context_new(&loop, &ctx);
    ASSERT_EQ(UVZMQ_OK, rc);

    uvzmq_socket_t *sock = nullptr;
    rc = uvzmq_socket_new(ctx, UVZMQ_REQ, &sock);
    ASSERT_EQ(UVZMQ_OK, rc);

    rc = uvzmq_setsockopt_int(sock, ZMQ_RCVTIMEO, 2000);
    ASSERT_EQ(UVZMQ_OK, rc);

    char connect_addr[64];
    snprintf(connect_addr, sizeof(connect_addr), "%s:%d", endpoint, port);
    rc = uvzmq_connect(sock, connect_addr);
    ASSERT_EQ(UVZMQ_OK, rc);

    for (int i = 0; i < 10; i++) {
        char msg[64];
        snprintf(msg, sizeof(msg), "Message %d", i);

        rc = uvzmq_send_string(sock, msg, 0);
        EXPECT_EQ(UVZMQ_OK, rc);

        char *reply = nullptr;
        rc = uvzmq_recv_string(sock, &reply, 0);
        EXPECT_EQ(UVZMQ_OK, rc);
        EXPECT_NE(nullptr, reply);

        char expected_reply[128];
        snprintf(expected_reply, sizeof(expected_reply), "Reply: %s", msg);
        EXPECT_STREQ(expected_reply, reply);

        UVZMQ_FREE(reply);
    }

    uvzmq_socket_free(sock);
    uvzmq_context_free(ctx);
    uv_loop_close(&loop);

    server_done = true;
    server_thread.join();
}

static void pub_sub_server_func(const char* endpoint, int port, bool* server_ready, bool* server_done) {
    uv_loop_t loop;
    uv_loop_init(&loop);

    uvzmq_context_t *ctx = nullptr;
    int rc = uvzmq_context_new(&loop, &ctx);
    ASSERT_EQ(UVZMQ_OK, rc);

    uvzmq_socket_t *sock = nullptr;
    rc = uvzmq_socket_new(ctx, UVZMQ_PUB, &sock);
    ASSERT_EQ(UVZMQ_OK, rc);

    char bind_addr[64];
    snprintf(bind_addr, sizeof(bind_addr), "%s:%d", endpoint, port);
    rc = uvzmq_bind(sock, bind_addr);
    ASSERT_EQ(UVZMQ_OK, rc);

    *server_ready = true;

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    for (int i = 0; i < 10 && !*server_done; i++) {
        char msg[64];
        snprintf(msg, sizeof(msg), "TopicA Message %d", i);
        rc = uvzmq_send_string(sock, msg, 0);
        if (rc == UVZMQ_OK) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }

    uvzmq_socket_free(sock);
    uvzmq_context_free(ctx);
    uv_loop_close(&loop);
}

TEST_F(UVZMQE2ETest, PubSubMultiThread) {
    const char *endpoint = "tcp://127.0.0.1";
    const int port = 5601;
    bool server_ready = false;
    bool server_done = false;

    std::thread server_thread(pub_sub_server_func, endpoint, port, &server_ready, &server_done);

    while (!server_ready) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    uv_loop_t loop;
    uv_loop_init(&loop);

    uvzmq_context_t *ctx = nullptr;
    int rc = uvzmq_context_new(&loop, &ctx);
    ASSERT_EQ(UVZMQ_OK, rc);

    uvzmq_socket_t *sock = nullptr;
    rc = uvzmq_socket_new(ctx, UVZMQ_SUB, &sock);
    ASSERT_EQ(UVZMQ_OK, rc);

    rc = uvzmq_setsockopt_bin(sock, ZMQ_SUBSCRIBE, "", 0);
    ASSERT_EQ(UVZMQ_OK, rc);

    rc = uvzmq_setsockopt_int(sock, ZMQ_RCVTIMEO, 2000);
    ASSERT_EQ(UVZMQ_OK, rc);

    char connect_addr[64];
    snprintf(connect_addr, sizeof(connect_addr), "%s:%d", endpoint, port);
    rc = uvzmq_connect(sock, connect_addr);
    ASSERT_EQ(UVZMQ_OK, rc);

    int msg_count = 0;
    auto start = std::chrono::steady_clock::now();
    while (msg_count < 10) {
        char *msg = nullptr;
        rc = uvzmq_recv_string(sock, &msg, 0);
        if (rc == UVZMQ_OK && msg != nullptr) {
            msg_count++;
            UVZMQ_FREE(msg);
        } else {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
            if (elapsed > 3000) {
                break;
            }
        }
    }

    EXPECT_GE(msg_count, 5);

    uvzmq_socket_free(sock);
    uvzmq_context_free(ctx);
    uv_loop_close(&loop);

    server_done = true;
    server_thread.join();
}

static void push_pull_server_func(const char* endpoint, int port, bool* server_ready, bool* server_done) {
    uv_loop_t loop;
    uv_loop_init(&loop);

    uvzmq_context_t *ctx = nullptr;
    int rc = uvzmq_context_new(&loop, &ctx);
    ASSERT_EQ(UVZMQ_OK, rc);

    uvzmq_socket_t *sock = nullptr;
    rc = uvzmq_socket_new(ctx, UVZMQ_PULL, &sock);
    ASSERT_EQ(UVZMQ_OK, rc);

    char bind_addr[64];
    snprintf(bind_addr, sizeof(bind_addr), "%s:%d", endpoint, port);
    rc = uvzmq_bind(sock, bind_addr);
    ASSERT_EQ(UVZMQ_OK, rc);

    *server_ready = true;

    int msg_count = 0;
    while (msg_count < 10 && !*server_done) {
        char *recv_data = nullptr;
        rc = uvzmq_recv_string(sock, &recv_data, 0);
        if (rc == UVZMQ_OK && recv_data != nullptr) {
            UVZMQ_FREE(recv_data);
            msg_count++;
        }
    }

    uvzmq_socket_free(sock);
    uvzmq_context_free(ctx);
    uv_loop_close(&loop);
}

TEST_F(UVZMQE2ETest, PushPullMultiThread) {
    const char *endpoint = "tcp://127.0.0.1";
    const int port = 5602;
    bool server_ready = false;
    bool server_done = false;

    std::thread server_thread(push_pull_server_func, endpoint, port, &server_ready, &server_done);

    while (!server_ready) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    uv_loop_t loop;
    uv_loop_init(&loop);

    uvzmq_context_t *ctx = nullptr;
    int rc = uvzmq_context_new(&loop, &ctx);
    ASSERT_EQ(UVZMQ_OK, rc);

    uvzmq_socket_t *sock = nullptr;
    rc = uvzmq_socket_new(ctx, UVZMQ_PUSH, &sock);
    ASSERT_EQ(UVZMQ_OK, rc);

    char connect_addr[64];
    snprintf(connect_addr, sizeof(connect_addr), "%s:%d", endpoint, port);
    rc = uvzmq_connect(sock, connect_addr);
    ASSERT_EQ(UVZMQ_OK, rc);

    for (int i = 0; i < 10; i++) {
        char msg[64];
        snprintf(msg, sizeof(msg), "Push Message %d", i);
        rc = uvzmq_send_string(sock, msg, 0);
        EXPECT_EQ(UVZMQ_OK, rc);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    uvzmq_socket_free(sock);
    uvzmq_context_free(ctx);
    uv_loop_close(&loop);

    server_done = true;
    server_thread.join();
}

TEST_F(UVZMQE2ETest, LargeMessageTransfer) {
    const char *endpoint = "tcp://127.0.0.1";
    const int port = 5603;
    bool server_ready = false;
    bool server_done = false;

    std::thread server_thread(server_thread_func, endpoint, port, &server_ready, &server_done);

    while (!server_ready) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    uv_loop_t loop;
    uv_loop_init(&loop);

    uvzmq_context_t *ctx = nullptr;
    int rc = uvzmq_context_new(&loop, &ctx);
    ASSERT_EQ(UVZMQ_OK, rc);

    uvzmq_socket_t *sock = nullptr;
    rc = uvzmq_socket_new(ctx, UVZMQ_REQ, &sock);
    ASSERT_EQ(UVZMQ_OK, rc);

    rc = uvzmq_setsockopt_int(sock, ZMQ_RCVTIMEO, 5000);
    ASSERT_EQ(UVZMQ_OK, rc);

    char connect_addr[64];
    snprintf(connect_addr, sizeof(connect_addr), "%s:%d", endpoint, port);
    rc = uvzmq_connect(sock, connect_addr);
    ASSERT_EQ(UVZMQ_OK, rc);

    const size_t large_msg_size = 1024 * 1024;
    char *large_msg = (char*)UVZMQ_MALLOC(large_msg_size + 1);
    ASSERT_NE(nullptr, large_msg);

    for (size_t i = 0; i < large_msg_size; i++) {
        large_msg[i] = 'A' + (i % 26);
    }
    large_msg[large_msg_size] = '\0';

    rc = uvzmq_send_string(sock, large_msg, 0);
    EXPECT_EQ(UVZMQ_OK, rc);

    char *reply = nullptr;
    rc = uvzmq_recv_string(sock, &reply, 0);
    EXPECT_EQ(UVZMQ_OK, rc);
    EXPECT_NE(nullptr, reply);
    UVZMQ_FREE(reply);

    UVZMQ_FREE(large_msg);

    uvzmq_socket_free(sock);
    uvzmq_context_free(ctx);
    uv_loop_close(&loop);

    server_done = true;
    server_thread.join();
}