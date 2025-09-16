#include "Connection.h"
#include <cstring>


Connection::Connection(int client_fd, Epoll* epoll) {
    m_nClient_fd = client_fd;
    m_pEpoll = epoll;
    m_bClosed = false;
}

Connection::~Connection() {
    if (m_nClient_fd >= 0) {
        close(m_nClient_fd);
    }
}

bool Connection::handle_write() {
    while (!m_sWrite_buffer.empty())
    {
        ssize_t result = write(m_nClient_fd, m_sWrite_buffer.c_str(), m_sWrite_buffer.size());

        if (result < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // 写缓冲区满了，等下次EPOLLOUT再写
                break;
            }
            std::cerr << "写入失败: " << get_fd() << " 错误: " << strerror(errno) << std::endl;
            return false;
        }
        m_sWrite_buffer.erase(0, result);
    }
    return true;
}
bool Connection::handle_read() {

    char buffer[BUFFER_SIZE];

    while (true)
    {
        ssize_t bytes_read = read(m_nClient_fd, buffer, BUFFER_SIZE);
        if (bytes_read > 0) {
            m_sRead_buffer.append(buffer, bytes_read);
        }
        //  已经读取完毕
        else if (bytes_read == 0) {
            std::cout << "客户端关闭连接:" << get_fd() << std::endl;
            return false;
        }
        else {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;
            else {/*  */
                std::cerr << "读取数据错误：" << strerror(errno) << std::endl;
            }
        }
    }
    if (!m_sRead_buffer.empty()) {
        process_request();
    }
    return true;
}


bool Connection::handle_err() {
    int error = 0;
    socklen_t len = sizeof(error);

    if (getsockopt(m_nClient_fd, SOL_SOCKET, SO_ERROR, &error, &len) == 0) {
        //  获取到错误码之后处理 
        //  使用error而不是errno
        if (error) {
            std::cerr << "Socket error :" << strerror(error) << std::endl;
            m_bClosed = true;
            m_pEpoll->del_epoll(get_fd());
            close(get_fd());
            m_nClient_fd = -1;
            return false;
        }
    }

    return true;
}
bool Connection::is_close() {
    return m_bClosed;
}
bool Connection::write_complete() {
    return m_sWrite_buffer.empty();
}