# Contributing to UVZMQ

感谢你对 UVZMQ 项目的关注！我们欢迎所有形式的贡献。

## 开发环境

### 系统要求

- Linux 系统（Ubuntu 20.04+ 推荐）
- GCC 7+ 或 Clang 6+
- CMake 3.10+
- Make

### 依赖项

```bash
# 获取子模块
git submodule update --init --recursive
```

## 代码规范

### C 代码规范

- **语言标准**: C99
- **缩进**: 4 空格
- **大括号风格**: K&R 风格
- **行宽**: 120 字符（推荐）

### 代码格式化

项目使用 clang-format 进行代码格式化，请确保你的代码符合规范：

```bash
# 检查代码格式
make check-format

# 自动格式化代码
make format
```

### 编译警告

- 所有代码必须通过 `-Wall -Wextra -Werror` 编译
- 不允许任何编译警告

## 测试要求

### 单元测试

- 所有新功能必须包含对应的单元测试
- 测试覆盖率要求：新增代码覆盖率 ≥ 80%
- 使用 Google Test 框架

### 测试文件位置

```
tests/
├── test_uvzmq_socket_new.cpp
├── test_uvzmq_socket_close.cpp
├── test_uvzmq_socket_free.cpp
├── test_uvzmq_getters.cpp
├── test_uvzmq_error_handling.cpp
├── test_uvzmq_integration.cpp
└── test_uvzmq_edge_cases.cpp
```

### 运行测试

```bash
# 编译测试
mkdir build && cd build
cmake ..
make

# 运行单元测试
ctest --output-on-failure

# 运行 ASAN 测试
mkdir build_asan && cd build_asan
cmake -DUVZMQ_ENABLE_ASAN=ON ..
make
ctest --output-on-failure

# 运行 TSAN 测试
mkdir build_tsan && cd build_tsan
cmake -DUVZMQ_ENABLE_TSAN=ON ..
make
ctest --output-on-failure
```

### 内存安全要求

- 所有测试必须通过 ASAN（AddressSanitizer）检查
- 所有测试必须通过 TSAN（ThreadSanitizer）检查
- 不允许内存泄漏

## 文档要求

### Doxygen 注释

所有公共 API 必须包含完整的 Doxygen 注释：

```c
/**
 * @brief 函数简要说明
 *
 * 详细说明...
 *
 * @param param1 参数1说明
 * @param param2 参数2说明
 * @return 返回值说明
 *
 * @note 注意事项
 *
 * @example
 * 使用示例：
 * @code
 * uvzmq_socket_t* sock;
 * uvzmq_socket_new(&loop, zmq_sock, callback, NULL, &sock);
 * @endcode
 */
int function_name(int param1, void* param2);
```

### 文档更新

- 修改公共 API 时，必须更新 README.md 和 README_CN.md
- 添加新功能时，必须添加对应的示例程序
- 示例程序位置：`examples/`

## 提交规范

### 提交信息格式

```
<type>: <subject>

<body>
```

### 类型（type）

- `feat`: 新功能
- `fix`: 修复 bug
- `docs`: 文档更新
- `style`: 代码格式调整
- `refactor`: 重构
- `test`: 测试相关
- `chore`: 构建工具或辅助工具

### 示例

```
feat: 添加批量消息处理支持

- 新增 UVZMQ_MAX_BATCH_SIZE 宏定义
- 优化 uvzmq_poll_callback 批量处理逻辑
- 性能提升约 30%

Closes #123
```

## Pull Request 流程

1. Fork 项目仓库
2. 创建功能分支：`git checkout -b feature/your-feature`
3. 编写代码和测试
4. 确保所有测试通过（包括 ASAN/TSAN）
5. 确保代码格式正确
6. 提交 Pull Request
7. 等待代码审查

## CI/CD 检查

所有 PR 必须通过以下 CI 检查：

- ✅ 代码格式检查（clang-format）
- ✅ 单元测试（Debug/Release）
- ✅ ASAN 内存安全检查
- ✅ TSAN 线程安全检查
- ✅ Markdown 格式检查

## 设计原则

UVZMQ 遵循以下设计原则，请在开发时遵守：

1. **极简主义** - 只添加必要的功能
2. **透明度优先** - 不隐藏实现细节
3. **零抽象** - 直接暴露 ZMQ API
4. **性能驱动** - 优先考虑性能
5. **用户控制** - 用户完全控制生命周期

## 性能要求

- 不引入显著的性能退化
- 使用 `benchmarks/` 目录中的基准测试验证性能
- 性能退化超过 10% 需要特别说明

## 问题反馈

- Bug 报告：使用 GitHub Issues
- 功能请求：使用 GitHub Discussions
- 安全问题：发送邮件到维护者

## 许可证

通过提交代码，你同意你的代码将按照项目的许可证发布。

## 联系方式

- GitHub Issues: https://github.com/adam-ikari/uvzmq/issues
- GitHub Discussions: https://github.com/adam-ikari/uvzmq/discussions

---

感谢你的贡献！
