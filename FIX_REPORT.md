# 修复报告

## 问题概述
- 在构建项目时，链接阶段出现 `undefined reference to Connection::get_fd()` 的错误，导致可执行文件无法生成。

## 处理措施
- 在 `src/Connection.cpp` 中新增 `Connection::get_fd` 的函数实现，返回内部维护的文件描述符 `m_nClient_fd`。

## 效益分析
- 解决链接错误，使得项目可以成功完成编译与链接流程。
- 保证 `Connection` 类对外提供的接口完整，实现读取、写入、错误处理流程中的文件描述符访问。

