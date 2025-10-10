#include <Server.h>
#include <Connection.h>
#include <cstring>
#include <cerrno>
#include <filesystem>
#include <fcntl.h>
#include <sys/epoll.h>

bool Server::make_socket_non_blocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        std::cerr << "fcntl(F_GETFL) failed: " << strerror(errno) << std::endl;
        return false;
    }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        std::cerr << "fcntl(F_SETFL) failed: " << strerror(errno) << std::endl;
        return false;
    }
    return true;
}

/// @brief 构造函数，设置值为初始值
///
///
Server::Server(const int port, const int max_Connection) {
    this->m_nServeFd = -1;
    this->m_pEpoll = nullptr;
    this->m_nMAXCONNECTIONS = max_Connection;
    this->m_nPORT = port;
}

Server::~Server() {

}

bool Server::Init() {
    if (m_isInit) {
        std::cerr << "this server has init." << std::endl;
        return false;
    }

    //  创建套接字
    m_nServeFd = socket(AF_INET, SOCK_STREAM, 0);
    if (m_nServeFd < 0) {
        std::cerr << "无法创建套接字" << std::endl;
        return false;
    }

    if (!Server::make_socket_non_blocking(m_nServeFd)) {
        close(m_nServeFd);
        m_nServeFd = -1;
        return false;
    }

    //  配置服务器地址
    m_sServer_addr.sin_family = AF_INET;
    m_sServer_addr.sin_addr.s_addr = INADDR_ANY;
    m_sServer_addr.sin_port = htons(m_nPORT);

    if (!apply_pending_options()) {
        close(m_nServeFd);
        m_nServeFd = -1;
        return false;
    }

    //  bind操作，绑定套接字到端口
    if (bind(m_nServeFd, (struct sockaddr*)&m_sServer_addr, sizeof(m_sServer_addr)) < 0) {
        std::cerr << "绑定失败: " << strerror(errno) << std::endl;
        close(m_nServeFd);
        return false;
    }
    m_isInit = true;

    //  新建epoll对象
    m_pEpoll = std::make_unique<Epoll>();
    m_upload_service = std::make_shared<FileUploadService>(std::filesystem::current_path() / "uploads");

    return true;
}

bool Server::Start() {
    if (!m_isInit) {
        std::cerr << "服务器尚未初始化" << std::endl;
        return false;
    }

    if (listen(m_nServeFd, m_nMAXCONNECTIONS) < 0) {
        std::cerr << "监听失败: " << strerror(errno) << std::endl;
        return false;
    }

    auto accept_handler = [this]() {
        while (m_running.load()) {
            sockaddr_in client_addr{};
            socklen_t client_len = sizeof(client_addr);
            int client_fd = accept(m_nServeFd, reinterpret_cast<sockaddr*>(&client_addr), &client_len);
            if (client_fd == -1) {
                if (errno == EINTR) {
                    continue;
                }
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    break;
                }
                if (!m_running.load()) {
                    break;
                }
                std::cerr << "接受连接失败: " << strerror(errno) << std::endl;
                break;
            }

            if (!Server::make_socket_non_blocking(client_fd)) {
                close(client_fd);
                continue;
            }

            auto conn = std::make_shared<Connection>(client_fd, m_pEpoll.get(), m_upload_service.get());
            if (!m_pEpoll->add_epoll(client_fd, EPOLLIN | EPOLLRDHUP | EPOLLET, conn)) {
                close(client_fd);
            }
        }
    };

    if (!m_pEpoll->register_listener(m_nServeFd, accept_handler)) {
        std::cerr << "注册监听套接字失败" << std::endl;
        return false;
    }

    std::cout << "服务器已启动，等待连接..." << std::endl;
    m_running = true;
    m_pEpoll->loop();
    m_running = false;

    return true;
}

void Server::Stop() {
    // TODO: Implement server stop logic, such as closing sockets and cleaning up resources.
    m_running = false;
    if (m_nServeFd >= 0) {
        //  说明此时还未关闭套接字
        close(m_nServeFd);
        m_nServeFd = -1;
    }

    //  epoll 在监听事件，因此在等待结束，所以我们实现一个函数将他Stop
    if (m_pEpoll) {
        m_pEpoll->stop();
    }

    //  重置epoll
    m_pEpoll.reset();

    //  重置服务器得状态。
    m_isInit = false;

    if (m_upload_service) {
        m_upload_service->cleanup_expired();
        m_upload_service.reset();
    }

    std::cout << "the server is stopped" << std::endl;
}

bool Server::setOption(int optname, bool value, int level) {
    m_options[{level, optname}] = value;

    if (m_nServeFd < 0) {
        return true;
    }

    int optval = value ? 1 : 0;
    if (setsockopt(m_nServeFd, level, optname, &optval, sizeof(optval)) < 0) {
        std::cerr << "设置选项失败: " << strerror(errno) << std::endl;
        return false;
    }
    return true;
}

bool Server::apply_pending_options() {
    if (m_nServeFd < 0) {
        return false;
    }

    for (const auto& [key, value] : m_options) {
        int optval = value ? 1 : 0;
        if (setsockopt(m_nServeFd, key.first, key.second, &optval, sizeof(optval)) < 0) {
            std::cerr << "设置选项失败: " << strerror(errno) << std::endl;
            return false;
        }
    }
    return true;
}