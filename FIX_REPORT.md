# 修复报告

## 问题概述
1. 在构建项目时，链接阶段出现 `undefined reference to Connection::get_fd()` 的错误，导致可执行文件无法生成。
2. 服务器初始化后立即返回，既没有接受连接也没有进入事件循环，无法实现回声功能；`Connection::process_request` 也未将收到的数据写回客户端。
3. 合并过程导致 `Connection::handle_read` 在遇到除 `EAGAIN`/`EWOULDBLOCK` 之外的错误时只记录日志、不退出循环，出现 CPU 打满且连接无法释放的问题。
4. 最近一次合并遗留了作用域冲突，`make_socket_non_blocking` 在 `Server.cpp` 中不可见，导致编译阶段报错并阻断构建流程。

## 处理措施
1. 在 `src/Connection.cpp` 中新增 `Connection::get_fd` 的函数实现，返回内部维护的文件描述符 `m_nClient_fd`。
2. 对服务器事件驱动框架进行重构：
   - 在 `src/Epoll.cpp` / `include/Epoll.h` 中为监听套接字注册回调，并在事件循环中统一处理连接的读写事件。
   - 在 `src/Server.cpp` / `include/Server.h` 中应用延迟的套接字选项、将描述符切换为非阻塞、进入阻塞式 `epoll` 循环以接受客户端连接，并在 `Stop` 中正确终止循环。
   - 在 `src/Connection.cpp` 中补充 `process_request` 的“回声”逻辑，保证读到的数据被写回客户端，同时根据写缓冲动态调整 epoll 关注的事件。
   - 更新 `src/main.cpp` 中的服务器启动流程，确保在初始化前配置所需的套接字选项。
3. 修复 `Connection::handle_read` 的错误处理逻辑：遇到不可恢复的读错误或对端关闭时立即标记连接已关闭并返回 `false`，让 epoll 正确摘除该连接。
4. 将 `make_socket_non_blocking` 封装为 `Server` 的私有静态成员，由类内部统一调用，避免自由函数在匿名命名空间中被意外隐藏。

## 效益分析
1. 解决链接错误，使得项目可以成功完成编译与链接流程，`Connection` 类的接口完整可用。
2. 服务器能够稳定监听端口、接受客户端连接并按照“回声”策略回复数据，同时支持信号触发的优雅退出，为后续扩展业务逻辑提供了可靠的事件循环基础。
3. 修复读事件处理后，可避免在出现网络异常时的死循环和 CPU 飙升，保障服务器资源的可用性。
4. 统一非阻塞配置的实现入口，构建流程恢复正常，也方便后续维护时复用该工具函数。

