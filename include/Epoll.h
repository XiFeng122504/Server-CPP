#ifndef _EPOLL_H
#define _EPOLL_H
#include <map>
#include <memory>
#include <Connection.h>
#include <sys/epoll.h>
#include <iostream>

class Epoll {
public:
    Epoll();
    ~Epoll();

    bool add_epoll(int fd, uint32_t events, std::shared_ptr<Connection>);
    bool del_epoll(int fd);
    bool mod_epoll(int fd, uint32_t events);
    void loop();

private:
    int epoll_fd;
    std::map<int, std::shared_ptr<Connection>> connections_map;

};

#endif//_EPOLL_H
