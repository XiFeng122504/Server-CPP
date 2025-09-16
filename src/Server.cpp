#include <Server.h>
#include <cstring>
#include <cerrno>

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

    //  应用所有选项，
    for (const auto& [key, value] : m_options) {
        int level = key.first;
        int optname = key.second;
        int optval = value ? 1 : 0;

        if (setsockopt(m_nServeFd, level, optname, &optval, sizeof(optval)) < 0) {
            std::cerr << "设置选项失败: " << strerror(errno) << std::endl;
            close(m_nServeFd);
            m_nServeFd = -1;
            return false;
        }
    }


    //  配置服务器地址
    m_sServer_addr.sin_family = AF_INET;
    m_sServer_addr.sin_addr.s_addr = INADDR_ANY;
    m_sServer_addr.sin_port = htons(m_nPORT);

    //  bind操作，绑定套接字到端口
    if (bind(m_nServeFd, (struct sockaddr*)&m_sServer_addr, sizeof(m_sServer_addr)) < 0) {
        std::cerr << "绑定失败: " << strerror(errno) << std::endl;
        close(m_nServeFd);
        return false;
    }
    m_isInit = true;

    //  新建epoll对象
    m_pEpoll = std::make_unique<Epoll>();

    return true;
}

bool Server::Start() {
    if (listen(m_nServeFd, m_nMAXCONNECTIONS) < 0) {
        std::cerr << "监听失败: " << strerror(errno) << std::endl;
        return false;
    }
    return true;
}

void Server::Stop() {
    // TODO: Implement server stop logic, such as closing sockets and cleaning up resources.
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

    std::cout << "the server is stopped" << std::endl;
}

bool Server::setOption(int optname, bool value, int level) {
    if (m_isInit) { //  说明已经初始化了，那你setOption也没有意义了。
        std::cerr << "[debug]" << "the server has Init, the setoption was invalid~" << std::endl;
        return false;
    }
    if (m_nServeFd < 0) {
        std::cerr << "[debug]" << "the server's fd was invalid, the setoption was failed" << std::endl;
        return false;
    }
    m_options[{level, optname}] = value;
    return true;
}