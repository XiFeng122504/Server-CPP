#ifndef _CONNECTION_H
#define _CONNECTION_H
#define BUFFER_SIZE 4096
#include <string>
#include <unistd.h>
#include <iostream>
#include <errno.h>  // 或 <cerrno>
#include <sys/socket.h>

#include "Epoll.h"

class Connection {
public:
    Connection(int client_fd, Epoll* epoll = nullptr);
    ~Connection();

    bool handle_write();
    bool handle_read();
    bool handle_err();
    bool is_close();
    bool write_complete();

    int get_fd();

    void process_request() {};
private:
    Epoll* m_pEpoll;
    int m_nClient_fd;
    std::string m_sRead_buffer;
    std::string m_sWrite_buffer;
    bool m_bClosed;
};

#endif//_CONNECTION_H
