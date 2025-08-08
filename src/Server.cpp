#include <Server.h>

/// @brife 构造函数，设置值为初始值
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
        perror("this server has init.");
        return false;
    }

    //  创建套接字
    m_nServeFd = socket(AF_INET, SOCK_STREAM, 0);
    if (m_nServeFd < 0) {
        std::cout << "无法创建套接字" << std::endl;
        return false;
    }

    //  新建epoll对象
    m_pEpoll = std::make_unique<Epoll>();



    //  配置服务器地址
    m_sServer_addr.sin_family = AF_INET;
    m_sServer_addr.sin_addr.s_addr = INADDR_ANY;
    m_sServer_addr.sin_port = htons(m_nPORT);

    //  bind操作，绑定套接字到端口
    if (bind(m_nServeFd, (struct sockaddr*)&m_sServer_addr, sizeof(m_sServer_addr)) < 0) {
        std::cerr << "绑定失败\n";
        close(m_nServeFd);
        return false;
    }
    return true;

}

bool Server::Start() {
    if (listen(m_nServeFd, m_nMAXCONNECTIONS) < 0) {
        std::cerr << "监听失败";
        return false;
    }
    return true;
}

void Server::Stop() {

}
