#include <Server.h>
#include <iostream>
#include <signal.h>

Server* g_server = nullptr;

void signal_handler(int sig) {
    std::cout << "接收到信号: " << sig << ", 停止服务器..." << std::endl;
    if (g_server) {
        g_server->Stop();
    }
    exit(0);
}

int main() {
    // 注册信号处理
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // 创建服务器
    g_server = new Server(8080, 100);

    // 初始化并设置选项
    if (!g_server->Init()) {
        std::cerr << "服务器初始化失败" << std::endl;
        delete g_server;
        return 1;
    }

    // 设置套接字选项
    g_server->setOption(SO_REUSEADDR, true);
    g_server->setOption(SO_KEEPALIVE, true);

    g_server->Init();

    // 启动服务器
    if (!g_server->Start()) {
        std::cerr << "服务器启动失败" << std::endl;
        delete g_server;
        return 1;
    }



    // Start方法中应该包含主事件循环，如果它会立即返回，
    // 则需要在这里添加阻塞代码，比如：


    delete g_server;
    return 0;
}