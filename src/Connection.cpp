#include "Connection.h"
#include <cstring>


Connection::Connection(Epoll* epoll) {
    m_pEpoll = epoll;
}


bool Connection::handle_write() {



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
            std::cerr << "Socket error :" << strerror(errno) << std::endl;
            m_bClosed = true;
            m_pEpoll->del_epoll(get_fd());
            close(get_fd());
            return false;
        }
    }

    return true;
}
bool Connection::is_close() {
    return m_bClosed;
}
bool Connection::write_complete() {
    return true;
}