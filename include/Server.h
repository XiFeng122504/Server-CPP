#ifndef _SERVER_H
#define _SERVER_H
#include <Epoll.h>
#include <sys/socket.h>
#include <iostream>
#include <arpa/inet.h>
#include <unistd.h>
#include <memory>
#include <map>
#include <atomic>

#include "protocol/RequestDispatcher.h"

class EchoHandler;
class LoginHandler;
class FileUploadService;

/**
 * Server类型，用于创建一个Server服务器，内置Epoll
 */
class Server {
public:
    Server(const int port = 8080, const int max_Connection = 10);
    ~Server();

    /**
     *  开始运行服务器
     */
    bool Start();

    /**
     *  停止运行服务器
     */
    void Stop();

    /**
     *  初始化服务器。
     */
    bool Init();

    /**
     *  设置服务器选项
     *  @param optname 选项名称
     *  @param value 选项值
     *  @param level 选项级别，默认SOL_SOCKET
     */
    bool setOption(int optname, bool value, int level = SOL_SOCKET);

private:
    static bool make_socket_non_blocking(int fd);
    bool apply_pending_options();
    bool register_builtin_handlers();
    //  服务器fd，用于监听其他socket连接本fd
    int m_nServeFd;
    //  是否初始化，判断服务器是否已经进行过初始化
    bool m_isInit = false;
    //  增加一个map存储选项，<value, optname>, bool
    std::map<std::pair<int, int>, bool> m_options;

    //  Epoll实例化对象，用于IO多路复用的epoll方式的封装
    std::unique_ptr<Epoll> m_pEpoll;
    //  sockaddr对象，用于存储PORT和接受IP等操作
    struct sockaddr_in m_sServer_addr;

    //  PORT端口
    int m_nPORT = 8080;
    //  最大连接数
    int m_nMAXCONNECTIONS = 10;
    std::atomic<bool> m_running{false};
    std::shared_ptr<protocol::RequestDispatcher> m_dispatcher;
    std::shared_ptr<EchoHandler> m_echoHandler;
    std::shared_ptr<LoginHandler> m_loginHandler;
    std::shared_ptr<FileUploadService> m_fileUploadService;
    bool m_handlersRegistered{false};
};

#endif //_SERVER_H
