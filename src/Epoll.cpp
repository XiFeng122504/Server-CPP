#include "Epoll.h"

Epoll::Epoll() {
    //  创建epoll对象，拿到文件描述符
    epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        std::cerr << "epoll_create1 failed" << std::endl;
        exit(EXIT_FAILURE);
    }
}

Epoll::~Epoll() {

}

bool Epoll::del_epoll(int fd) {
    auto it = connections_map.find(fd);
    if (it == connections_map.end()) {
        std::cerr << "del_epoll: fd not found in connections_map\n";
        return false;
    }


    if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, nullptr) == -1) {
        std::cerr << "epoll_ctl: dele failed" << std::endl;
        return false;
    }
    connections_map.erase(fd);
    return true;
}

bool Epoll::mod_epoll(int fd, uint32_t events) {

    auto it = connections_map.find(fd);
    if (it == connections_map.end()) {
        std::cerr << "mod_epoll: fd not found in connections_map\n";
        return false;
    }

    epoll_event ev{};
    ev.events = events;
    ev.data.ptr = it->second.get();

    if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &ev) == -1) {
        std::cerr << "epoll_ctl: mod failed" << std::endl;
        return false;
    }
    return true;
}

bool Epoll::add_epoll(int fd, uint32_t events, std::shared_ptr<Connection> conn) {
    epoll_event ev{};
    ev.events = events;         // 对这个 fd 关注“可读”事件
    ev.data.ptr = conn.get();   // 拿到共享指针的指针。 

    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev) == -1) {
        std::cerr << "epoll_ctl: add failed" << std::endl;
        return false;
    }
    connections_map[fd] = conn;
    return true;
}

void Epoll::loop() {
    //TODO:等待实现
}