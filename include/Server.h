#ifndef _SEVER_H
#define _SEVER_H
#include <Epoll.h>
#include <sys/socket.h>
#include <iostream>
#include <arpa/inet.h>
#include <unistd.h>
#include <memory>

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
};

#endif //_SEVER_H
