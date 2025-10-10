#include "Epoll.h"
#include <Connection.h>


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


void Epoll::stop() {
    stopFlag = true;

    if (epoll_fd >= 0) {
        close(epoll_fd);
        epoll_fd = -1;
    }

    connections_map.clear();
    listener_fd = -1;
    accept_callback = nullptr;
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
    stopFlag = false;
    //  事件循环一共处理多少个事件。
    const int MAX_EVENTS = 64;
    std::vector<epoll_event> events(MAX_EVENTS);

    //  stopFlag本来是true，即不停止的，一直循环。
    while (!stopFlag) {
        //  epoll等待epollfd的事件，events.data，好像都没有啊？
        int n = epoll_wait(epoll_fd, events.data(), MAX_EVENTS, -1);

        //  n == -1，是信号中断对吗？
        if (n == -1) {
            // 被信号中断是常见情况，不应该退出
            if (errno == EINTR) {
                continue;
            }
            if (stopFlag) {
                break;
            }
            std::cerr << "epoll_wait error: " << errno << std::endl;
            break;
        }

        // 处理所有就绪的事件
        for (int i = 0; i < n; ++i) {
            if (events[i].data.ptr == nullptr) {
                if (accept_callback) {
                    accept_callback();
                }
                continue;
            }
            // 获取存储在data.ptr中的Connection对象
            Connection* conn = static_cast<Connection*>(events[i].data.ptr);
            if (!conn) {
                std::cerr << "Invalid connection pointer" << std::endl;
                continue;
            }

            uint32_t current_events = events[i].events;

            bool should_close = false;

            if (current_events & EPOLLIN) {
                if (!conn->handle_read()) {
                    should_close = true;
                }
            }

            if (!should_close && (current_events & EPOLLOUT)) {
                if (!conn->handle_write()) {
                    std::cerr << "Handle write failed, fd: " << conn->get_fd() << std::endl;
                    should_close = true;
                } else if (conn->write_complete()) {
                    mod_epoll(conn->get_fd(), EPOLLIN | EPOLLRDHUP | EPOLLET);
                }
            }

            if (should_close || (current_events & (EPOLLERR | EPOLLHUP | EPOLLRDHUP))) {
                del_epoll(conn->get_fd());
            }
        }


    }



}

bool Epoll::register_listener(int fd, std::function<void()> callback) {
    if (epoll_fd < 0) {
        return false;
    }
    epoll_event ev{};
    ev.events = EPOLLIN;
    ev.data.ptr = nullptr;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev) == -1) {
        std::cerr << "epoll_ctl: add listener failed" << std::endl;
        return false;
    }
    listener_fd = fd;
    accept_callback = std::move(callback);
    return true;
}

void Epoll::unregister_listener() {
    if (epoll_fd < 0 || listener_fd == -1) {
        return;
    }
    if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, listener_fd, nullptr) == -1) {
        std::cerr << "epoll_ctl: del listener failed" << std::endl;
    }
    listener_fd = -1;
    accept_callback = nullptr;
}