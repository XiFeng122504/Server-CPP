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
            std::cerr << "epoll_wait error: " << errno << std::endl;
            break;
        }

        // 处理所有就绪的事件
        for (int i = 0; i < n; ++i) {
            // 获取存储在data.ptr中的Connection对象
            Connection* conn = static_cast<Connection*>(events[i].data.ptr);
            if (!conn) {
                std::cerr << "Invalid connection pointer" << std::endl;
                continue;
            }

            uint32_t current_events = events[i].events;

            // 处理错误或挂起事件
            if (current_events & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) {
                std::cerr << "Connection error or closed by peer, fd: " << conn->get_fd() << std::endl;
                del_epoll(conn->get_fd()); // 移除连接
                continue;
            }

            // 处理可读事件
            if (current_events & EPOLLIN) {
                if (!conn->handle_read()) {
                    std::cerr << "Handle read failed, fd: " << conn->get_fd() << std::endl;
                    del_epoll(conn->get_fd());
                    continue;
                }
            }

            // 处理可写事件
            if (current_events & EPOLLOUT) {
                if (!conn->handle_write()) {
                    std::cerr << "Handle write failed, fd: " << conn->get_fd() << std::endl;
                    del_epoll(conn->get_fd());
                    continue;
                }

                // 如果数据已全部发送完毕，可以取消关注EPOLLOUT事件
                if (conn->write_complete()) {
                    mod_epoll(conn->get_fd(), EPOLLIN);
                }
            }
        }


    }



}