#ifndef _EPOLL_H
#define _EPOLL_H
#include <map>
#include <memory>
#include <Connection.h>

class Epoll {
public:
    Epoll();
    ~Epoll();

    void add_epoll(int fd, std::shared_ptr<Connection>);
    void del_epoll(int fd);
    void mod_epoll(int fd, uint32_t events);
    void loop();

private:
    int epoll_fd;
    std::map<int, std::shared_ptr<Connection>> connections_map;

};

#endif//_EPOLL_H
