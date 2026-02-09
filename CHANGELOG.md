# Changelog

本文档记录 UVZMQ 项目的所有重要变更。

格式遵循 [Keep a Changelog](https://keepachangelog.com/zh-CN/1.0.0/)，
版本号遵循 [语义化版本](https://semver.org/lang/zh-CN/)。

## [Unreleased]

### Added
- 完整的单元测试套件（94 个测试用例）
- ASAN/TSAN 内存安全检测
- 代码覆盖率报告生成
- 贡献指南（CONTRIBUTING.md）
- 变更日志（CHANGELOG.md）

### Fixed
- 修复 CMake 构建依赖顺序问题
- 修复内存泄漏问题（异步清理顺序）
- 修复 EINTR 错误处理
- 修复覆盖率报告生成问题
- 修复 macOS/Windows 平台编译问题（类型重复定义）

### Changed
- 简化 CI/CD 配置，专注于 Linux 平台
- 移除 Codecov 上传（服务不稳定）
- 调整头文件包含顺序，避免类型冲突
- 在 ASAN/TSAN 模式下禁用 -Werror

### Performance
- 批量消息处理优化（最多 1000 条消息/批次）
- 使用 uv_poll 替代定时器轮询（性能提升 200 倍）
- 零拷贝消息支持

## [0.1.0] - 2026-01-07

### Added
- 初始版本发布
- 核心功能：
  - `uvzmq_socket_new()` - 创建 socket
  - `uvzmq_socket_close()` - 关闭 socket
  - `uvzmq_socket_free()` - 释放 socket
  - `uvzmq_get_zmq_socket()` - 获取 ZMQ socket
  - `uvzmq_get_loop()` - 获取 libuv loop
  - `uvzmq_get_user_data()` - 获取用户数据
  - `uvzmq_get_fd()` - 获取文件描述符
- 30 个示例程序
- 完整的中英文文档
- Doxygen API 文档

### Performance
- 基准测试：~0.056ms/消息
- 对比定时器轮询：~11.2ms/消息
- 性能提升：~200x

### Documentation
- README.md（英文）
- README_CN.md（中文）
- docs/en/tutorial.md（英文教程）
- docs/zh/tutorial.md（中文教程）
- Doxygen 生成的 API 文档

---

## 版本说明

### 版本号格式
- 主版本号：不兼容的 API 修改
- 次版本号：向下兼容的功能性新增
- 修订版本号：向下兼容的问题修正

### 发布周期
- 无固定发布周期
- 功能完善或问题修复时发布
- 建议每月至少一次代码审查

### 发布类型
- **Alpha**：早期开发版本，API 可能变化
- **Beta**：功能基本完成，API 稳定
- **RC**：候选发布版本，准备正式发布
- **Stable**：稳定版本，适合生产使用

---

## 贡献者指南

如果你想添加变更到 CHANGELOG.md，请遵循以下格式：

### 新增功能
```markdown
### Added
- 功能描述
```

### 修复问题
```markdown
### Fixed
- 问题描述和修复方法
```

### 性能改进
```markdown
### Performance
- 性能改进描述和提升数据
```

### 破坏性变更
```markdown
### Breaking Changes
- 破坏性变更描述
- 迁移指南
```

---

## 相关链接

- [贡献指南](CONTRIBUTING.md)
- [README](README.md)
- [GitHub Releases](https://github.com/adam-ikari/uvzmq/releases)
- [问题追踪](https://github.com/adam-ikari/uvzmq/issues)