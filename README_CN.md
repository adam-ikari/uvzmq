# UVZMQ

ZeroMQ的极简libuv集成库。

UVZMQ**只做一件事**：使用`uv_poll`将ZMQ套接字集成到libuv事件循环中。所有其他ZMQ操作（send、recv、poll、setsockopt等）都应该直接使用ZMQ API。

## 特性

- ✅ **极简API** - 仅提供必要的函数
- ✅ **事件驱动** - 使用libuv的`uv_poll`实现高效I/O
- ✅ **批量处理** - 针对高吞吐量场景优化
- ✅ **零拷贝支持** - 兼容ZMQ的零拷贝消息
- ✅ **C99标准** - 支持GCC和Clang编译器

## 快速开始

UVZMQ是一个**仅头文件库**。在其中一个源文件中包含实现宏和头文件：

```c
// 在你的一个源文件中
#define UVZMQ_IMPLEMENTATION
#include "uvzmq.h"

// 在其他源文件中
#include "uvzmq.h"
```

然后在你的代码中使用：

```c
#include "uvzmq.h"
#include <zmq.h>
#include <uv.h>

// 回调函数
void on_recv(uvzmq_socket_t *s, zmq_msg_t *msg, void *data) {
    // 回显（零拷贝）
    zmq_msg_send(msg, uvzmq_get_zmq_socket(s), 0);
    
    // 重要：关闭消息以避免内存泄漏
    zmq_msg_close(msg);
}

int main(void) {
    // 创建ZMQ上下文和套接字
    void *zmq_ctx = zmq_ctx_new();
    void *zmq_sock = zmq_socket(zmq_ctx, ZMQ_REP);
    zmq_bind(zmq_sock, "tcp://*:5555");

    // 创建libuv循环
    uv_loop_t loop;
    uv_loop_init(&loop);

    // 与libuv集成
    uvzmq_socket_t *uvzmq_sock = NULL;
    uvzmq_socket_new(&loop, zmq_sock, on_recv, NULL, &uvzmq_sock);

    // 运行事件循环
    int keep_running = 1;
    while (keep_running) {
        uv_run(&loop, UV_RUN_ONCE);
    }

    // 清理
    uvzmq_socket_free(uvzmq_sock);
    zmq_close(zmq_sock);
    zmq_ctx_term(zmq_ctx);
    uv_loop_close(&loop);
    
    return 0;
}
```

**重要：** 只在**一个**源文件中定义`UVZMQ_IMPLEMENTATION`。所有其他文件应该只包含`"uvzmq.h"`。

## 构建

### 前置要求

- C99兼容编译器（GCC或Clang）
- CMake 3.10或更高版本
- libuv（作为子模块包含）
- ZeroMQ 4.x（作为子模块包含）

### 构建步骤

```bash
git submodule update --init --recursive
mkdir build && cd build
cmake ..
make
```

### 构建选项

```bash
# 禁用示例
cmake -DUVZMQ_BUILD_EXAMPLES=OFF ..

# 禁用基准测试
cmake -DUVZMQ_BUILD_BENCHMARKS=OFF ..
```

## API参考

### 核心函数

#### `uvzmq_socket_new`

初始化UVZMQ套接字并集成到libuv事件循环。

```c
int uvzmq_socket_new(uv_loop_t *loop,
                     void *zmq_sock,
                     uvzmq_recv_callback on_recv,
                     void *user_data,
                     uvzmq_socket_t **socket);
```

**参数：**
- `loop` - libuv事件循环
- `zmq_sock` - 现有的ZMQ套接字
- `on_recv` - 套接字可读时的回调
- `user_data` - 传递给回调的用户数据
- `socket` - [输出] 指向接收创建的uvzmq套接字的指针

**返回值：** 成功返回`0`，失败返回`-1`

**错误诊断：** 失败时，你可以通过以下方式诊断错误：
- 检查`errno`获取系统级错误
- 调用`zmq_errno()`获取ZMQ错误代码
- 调用`zmq_strerror(zmq_errno())`获取错误消息

#### `uvzmq_socket_close`

停止套接字的事件处理而不释放资源。

```c
int uvzmq_socket_close(uvzmq_socket_t *socket);
```

**注意：** 这会停止libuv对套接字的轮询，但套接字仍然有效。当你想要临时停止事件处理而不销毁套接字时很有用。要永久移除套接字，使用`uvzmq_socket_free()`。

#### `uvzmq_socket_free`

释放UVZMQ套接字资源。

```c
int uvzmq_socket_free(uvzmq_socket_t *socket);
```

**注意：** 这**不会**关闭底层的ZMQ套接字。你必须自己调用`zmq_close()`。

### 工具函数

#### `uvzmq_get_zmq_socket`

获取底层的ZMQ套接字。

```c
void *uvzmq_get_zmq_socket(uvzmq_socket_t *socket);
```

#### `uvzmq_get_loop`

获取libuv循环。

```c
uv_loop_t *uvzmq_get_loop(uvzmq_socket_t *socket);
```

#### `uvzmq_get_user_data`

获取用户数据。

```c
void *uvzmq_get_user_data(uvzmq_socket_t *socket);
```

#### `uvzmq_get_fd`

获取ZMQ文件描述符。

```c
int uvzmq_get_fd(uvzmq_socket_t *socket);
```

### 错误处理

所有函数成功返回`0`，失败返回`-1`。要诊断错误：

```c
if (uvzmq_socket_new(loop, zmq_sock, on_recv, NULL, &sock) != 0) {
    // 检查系统errno
    perror("uvzmq_socket_new");
    
    // 或检查ZMQ错误
    int zmq_err = zmq_errno();
    fprintf(stderr, "ZMQ error: %s\n", zmq_strerror(zmq_err));
}
```

常见错误来源：
- 无效参数（NULL指针）
- ZMQ套接字未正确初始化
- ZMQ套接字未绑定/连接
- libuv循环分配失败

## 线程安全

UVZMQ**不是线程安全的**。每个`uvzmq_socket_t`必须只由单个线程使用。

对于多线程应用程序：
- 为每个线程创建单独的`uvzmq_socket_t`实例
- 使用单独的ZMQ上下文或适当配置`ZMQ_IO_THREADS`
- 不要在线程间共享`uvzmq_socket_t`或`zmq_sock`

## 回调要求

`on_recv`回调**必须**在处理后关闭`zmq_msg_t`：

```c
void on_recv(uvzmq_socket_t *socket, zmq_msg_t *msg, void *user_data) {
    // 处理消息
    zmq_msg_send(msg, uvzmq_get_zmq_socket(socket), 0);
    
    // 必需：关闭消息以避免内存泄漏
    zmq_msg_close(msg);
}
```

## 示例

查看`examples/`目录获取完整示例：

- `simple.c` - 基本的REQ/REP模式
- `best_practices.c` - 包含信号处理的完整示例
- `test_*.c` - 各种测试用例

## 性能

UVZMQ相比基于定时器的轮询提供了显著的性能提升：

- **基于定时器**：每条消息约11.2ms
- **事件驱动（UVZMQ）**：每条消息约0.056ms
- **提升**：约200倍

对于大消息（>1KB），UVZMQ实现了与原生ZMQ相当的性能，仅由于libuv回调基础设施而有5-8%的开销。

## 设计理念

UVZMQ遵循以下原则：

1. **极简** - 仅提供libuv事件循环集成
2. **直接** - 用户直接与ZMQ API交互
3. **透明** - 没有隐藏的抽象或魔法
4. **高效** - 零不必要的开销

## 许可证

MIT许可证

## 贡献

欢迎贡献！请确保：

- 代码遵循C99标准
- 没有C11特性（为了更广泛的兼容性）
- 所有函数都有适当的错误检查
- 示例展示最佳实践