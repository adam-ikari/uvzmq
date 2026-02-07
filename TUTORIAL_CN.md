# UVZMQ 教程

使用 UVZMQ 集成 ZeroMQ 和 libuv 的完整指南。

## 目录

1. [简介](#简介)
2. [安装](#安装)
3. [基本用法](#基本用法)
4. [常见模式](#常见模式)
5. [最佳实践](#最佳实践)
6. [故障排查](#故障排查)

---

## 简介

UVZMQ 是一个极简的库，用于桥接 ZeroMQ 和 libuv，实现基于事件的 ZMQ 消息传递。它只提供必要的集成层，同时让你完全访问 ZMQ API。

### 核心概念

- **事件驱动**: 使用 libuv 的 `uv_poll` 实现高效 I/O
- **批量处理**: 每个事件最多处理 1000 条消息
- **零拷贝**: 兼容 ZMQ 的零拷贝消息传递
- **单文件库**: 单头文件集成，无构建依赖

---

## 安装

### 方式 1: 克隆并使用

```bash
git clone https://github.com/yourusername/uvzmq.git
cd uvzmq
```

### 方式 2: 复制头文件

将 `include/uvzmq.h` 复制到你的项目目录。

### 要求

- C99 兼容编译器（GCC、Clang）
- libuv (v1.x)
- ZeroMQ (v4.x)

---

## 基本用法

### 步骤 1: 包含头文件

在**一个**源文件中：

```c
#define UVZMQ_IMPLEMENTATION
#include "uvzmq.h"
```

在其他源文件中：

```c
#include "uvzmq.h"
```

### 步骤 2: 创建基本服务器

```c
#include "uvzmq.h"
#include <zmq.h>
#include <uv.h>
#include <stdio.h>
#include <string.h>

// 回调函数 - 收到消息时调用
void on_recv(uvzmq_socket_t* socket, zmq_msg_t* msg, void* user_data) {
    // 获取消息数据
    size_t size = zmq_msg_size(msg);
    const char* data = (const char*)zmq_msg_data(msg);
    
    printf("收到: %.*s\n", (int)size, data);
    
    // 回显（零拷贝）
    zmq_msg_send(msg, uvzmq_get_zmq_socket(socket), 0);
    
    // 重要：关闭消息
    zmq_msg_close(msg);
}

int main(void) {
    // 初始化 libuv
    uv_loop_t loop;
    uv_loop_init(&loop);
    
    // 创建 ZMQ 上下文和 socket
    void* zmq_ctx = zmq_ctx_new();
    void* zmq_sock = zmq_socket(zmq_ctx, ZMQ_REP);
    zmq_bind(zmq_sock, "tcp://*:5555");
    
    // 集成到 libuv
    uvzmq_socket_t* uvzmq_sock = NULL;
    int rc = uvzmq_socket_new(&loop, zmq_sock, on_recv, NULL, &uvzmq_sock);
    if (rc != 0) {
        fprintf(stderr, "创建 UVZMQ socket 失败\n");
        return 1;
    }
    
    // 运行事件循环
    printf("服务器运行在 tcp://*:5555\n");
    uv_run(&loop, UV_RUN_DEFAULT);
    
    // 清理
    uvzmq_socket_free(uvzmq_sock);
    zmq_close(zmq_sock);
    zmq_ctx_term(zmq_ctx);
    uv_loop_close(&loop);
    
    return 0;
}
```

### 步骤 3: 创建基本客户端

```c
#include <zmq.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    void* context = zmq_ctx_new();
    void* socket = zmq_socket(context, ZMQ_REQ);
    zmq_connect(socket, "tcp://localhost:5555");
    
    // 发送消息
    const char* msg = "来自客户端的问候";
    zmq_send(socket, msg, strlen(msg), 0);
    
    // 接收回复
    char buffer[256];
    int size = zmq_recv(socket, buffer, sizeof(buffer) - 1, 0);
    if (size > 0) {
        buffer[size] = '\0';
        printf("收到: %s\n", buffer);
    }
    
    // 清理
    zmq_close(socket);
    zmq_ctx_term(context);
    
    return 0;
}
```

---

## 常见模式

### 模式 1: 请求-回复 (REQ/REP)

参见 `examples/req_rep.c` 获取完整示例。

```c
// 服务器
void* server_sock = zmq_socket(ctx, ZMQ_REP);
zmq_bind(server_sock, "tcp://*:5555");

// 客户端
void* client_sock = zmq_socket(ctx, ZMQ_REQ);
zmq_connect(client_sock, "tcp://localhost:5555");
```

### 模式 2: 发布-订阅 (PUB/SUB)

参见 `examples/pub_sub.c` 获取完整示例。

```c
// 发布者
void* pub_sock = zmq_socket(ctx, ZMQ_PUB);
zmq_bind(pub_sock, "tcp://*:5556");

// 订阅者
void* sub_sock = zmq_socket(ctx, ZMQ_SUB);
zmq_connect(sub_sock, "tcp://localhost:5556");
zmq_setsockopt(sub_sock, ZMQ_SUBSCRIBE, "", 0);  // 订阅所有消息
```

### 模式 3: 推送-拉取 (PUSH/PULL)

参见 `examples/test_push_pull.c` 获取完整示例。

```c
// 推送者
void* push_sock = zmq_socket(ctx, ZMQ_PUSH);
zmq_bind(push_sock, "tcp://*:5557");

// 拉取者
void* pull_sock = zmq_socket(ctx, ZMQ_PULL);
zmq_connect(pull_sock, "tcp://localhost:5557");
```

### 模式 4: 多线程

参见 `examples/multi_thread.c` 获取完整示例。

每个线程必须有自己独立的 `uvzmq_socket_t` 实例。

---

## 最佳实践

### 1. 始终关闭消息

```c
void on_recv(uvzmq_socket_t* socket, zmq_msg_t* msg, void* user_data) {
    // 处理消息
    zmq_msg_send(msg, uvzmq_get_zmq_socket(socket), 0);
    
    // 必须关闭以避免内存泄漏
    zmq_msg_close(msg);
}
```

### 2. 正确的清理顺序

```c
// 正确的顺序：
uvzmq_socket_free(uvzmq_sock);  // 1. 释放 UVZMQ
zmq_close(zmq_sock);              // 2. 关闭 ZMQ socket
zmq_ctx_term(zmq_ctx);            // 3. 终止上下文
uv_loop_close(&loop);             // 4. 关闭循环
```

### 3. 错误处理

```c
int rc = uvzmq_socket_new(&loop, zmq_sock, on_recv, NULL, &uvzmq_sock);
if (rc != 0) {
    // 检查系统错误
    perror("uvzmq_socket_new");
    
    // 或检查 ZMQ 错误
    int zmq_err = zmq_errno();
    fprintf(stderr, "ZMQ 错误: %s\n", zmq_strerror(zmq_err));
    
    return 1;
}
```

### 4. 线程安全

每个 `uvzmq_socket_t` 必须由单个线程使用：

```c
// 线程 1
uvzmq_socket_t* socket1 = NULL;
uvzmq_socket_new(&loop1, zmq_sock1, callback1, NULL, &socket1);

// 线程 2
uvzmq_socket_t* socket2 = NULL;
uvzmq_socket_new(&loop2, zmq_sock2, callback2, NULL, &socket2);

// 绝不要在线程间共享 socket1
```

### 5. 使用零拷贝提升性能

```c
void on_recv(uvzmq_socket_t* socket, zmq_msg_t* msg, void* user_data) {
    // 重用消息而不是复制
    zmq_msg_send(msg, uvzmq_get_zmq_socket(socket), 0);
    
    // 发送后仍需关闭
    zmq_msg_close(msg);
}
```

---

## 故障排查

### 问题: "段错误 (Segmentation fault)"

**原因**: 可能是双重释放或使用后释放。

**解决方案**: 确保正确的清理顺序和消息关闭：

```c
// 始终关闭消息
zmq_msg_close(msg);

// 始终在 ZMQ socket 之前释放 socket
uvzmq_socket_free(uvzmq_sock);
zmq_close(zmq_sock);
```

### 问题: "没有收到消息"

**原因**: 缺少订阅或错误的 socket 类型。

**解决方案**:

```c
// 对于 PUB/SUB，必须订阅：
zmq_setsockopt(sub_sock, ZMQ_SUBSCRIBE, "", 0);

// 检查 socket 类型是否匹配：
// REQ ↔ REP
// PUB ↔ SUB
// PUSH ↔ PULL
```

### 问题: "内存泄漏"

**原因**: 回调中未关闭 `zmq_msg_t`。

**解决方案**:

```c
void on_recv(uvzmq_socket_t* socket, zmq_msg_t* msg, void* user_data) {
    // 处理消息
    // ...
    
    // 必须关闭！
    zmq_msg_close(msg);
}
```

### 问题: "uvzmq_socket_new 返回 -1"

**原因**: 无效参数或初始化失败。

**解决方案**:

```c
// 检查参数
if (!loop || !zmq_sock || !socket) {
    fprintf(stderr, "参数无效\n");
    return;
}

// 检查错误详情
if (uvzmq_socket_new(loop, zmq_sock, on_recv, NULL, &socket) != 0) {
    perror("uvzmq_socket_new");
    // 检查 ZMQ 错误
    fprintf(stderr, "ZMQ 错误: %s\n", zmq_strerror(zmq_errno()));
}
```

---

## 性能技巧

### 1. 理解 libuv 运行模式

libuv 的 `uv_run()` 函数有多种运行模式，选择合适的模式对性能和行为有重要影响：

#### UV_RUN_DEFAULT

```c
uv_run(&loop, UV_RUN_DEFAULT);
```

- **行为**: 运行循环直到没有活动句柄
- **特点**: 阻塞式，直到所有工作完成
- **用途**: 服务器应用，需要持续运行直到所有任务完成
- **注意**: 如果有定时器或其他周期性任务，循环不会退出

#### UV_RUN_ONCE

```c
while (keep_running) {
    uv_run(&loop, UV_RUN_ONCE);
}
```

- **行为**: 运行一个事件循环迭代
- **特点**: 处理一次事件后返回
- **用途**: 需要与其他逻辑混合使用
- **优势**: 可以控制循环执行，方便添加自定义逻辑

#### UV_RUN_NOWAIT

```c
uv_run(&loop, UV_RUN_NOWAIT);
```

- **行为**: 立即返回，不等待任何事件
- **特点**: 非阻塞，检查是否有事件就绪
- **用途**: 轮询式处理，需要快速检查的场合
- **注意**: 可能消耗 CPU 资源

#### 性能对比

```c
// 示例 1: UV_RUN_DEFAULT（推荐用于服务器）
uv_run(&loop, UV_RUN_DEFAULT);  // 最简单，CPU 利用率低

// 示例 2: UV_RUN_ONCE（推荐用于需要控制的场景）
while (keep_running) {
    uv_run(&loop, UV_RUN_ONCE);  // 灵活，CPU 利用率适中
    // 可以添加其他逻辑
}

// 示例 3: UV_RUN_NOWAIT（仅用于特殊场景）
while (keep_running) {
    uv_run(&loop, UV_RUN_NOWAIT);  // 消耗 CPU
    usleep(1000);  // 需要手动控制
}
```

#### 最佳实践

```c
// 服务器应用（推荐）
uv_run(&loop, UV_RUN_DEFAULT);

// 需要优雅关闭的应用
int keep_running = 1;
while (keep_running) {
    uv_run(&loop, UV_RUN_ONCE);
    // 检查关闭信号
}

// 测试或调试
for (int i = 0; i < 10; i++) {
    uv_run(&loop, UV_RUN_ONCE);
}
```

### 2. 使用批量处理

UVZMQ 自动批量处理最多 1000 条消息/事件。对于超高吞吐量，可以调整常量：

```c
#define UVZMQ_MAX_BATCH_SIZE 2000  // 增加批量大小
#define UVZMQ_BATCH_CHECK_INTERVAL 100  // 减少检查频率
```

### 2. 优化 Socket 选项

```c
// 设置高水位标记以获得更好的吞吐量
int hwm = 10000;
zmq_setsockopt(sock, ZMQ_SNDHWM, &hwm, sizeof(hwm));
zmq_setsockopt(sock, ZMQ_RCVHWM, &hwm, sizeof(hwm));
```

### 3. 使用零拷贝

避免复制消息数据：

```c
// 好：零拷贝
zmq_msg_send(msg, socket, 0);

// 差：复制数据
char* data = malloc(size);
memcpy(data, zmq_msg_data(msg), size);
zmq_send(socket, data, size, 0);
free(data);
```

---

## 其他资源

- **API 文档**: 运行 `make docs` 生成 API 文档
- **示例**: 参见 `examples/` 目录
- **测试**: 参见 `tests/` 目录获取单元测试
- **ZMQ 指南**: http://zguide.zeromq.org/
- **libuv 指南**: https://docs.libuv.org/

---

## 贡献

欢迎贡献！请确保：

- 代码遵循 C99 标准
- 所有函数都有适当的错误检查
- 示例展示最佳实践
- 测试覆盖新功能