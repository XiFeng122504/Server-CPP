#include "Connection.h"

#include <cstring>
#include <sys/epoll.h>

#include "Epoll.h"
#include "protocol/Message.h"
#include "protocol/RequestDispatcher.h"

Connection::Connection(int client_fd, Epoll* epoll, protocol::RequestDispatcher* dispatcher) {
    m_nClient_fd = client_fd;
    m_pEpoll = epoll;
    m_dispatcher = dispatcher;
    m_bClosed = false;
}

Connection::~Connection() {
    if (m_dispatcher) {
        m_dispatcher->notify_connection_closed(*this);
    }
    if (m_nClient_fd >= 0) {
        close(m_nClient_fd);
    }
}

bool Connection::handle_write() {
    while (!m_sWrite_buffer.empty()) {
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

    if (m_sWrite_buffer.empty() && m_pEpoll) {
        m_pEpoll->mod_epoll(get_fd(), EPOLLIN | EPOLLRDHUP | EPOLLET);
    }
    return true;
}
bool Connection::handle_read() {

    char buffer[BUFFER_SIZE];

    while (true) {
        ssize_t bytes_read = read(m_nClient_fd, buffer, BUFFER_SIZE);
        if (bytes_read > 0) {
            m_sRead_buffer.append(buffer, bytes_read);
        }
        //  已经读取完毕
        else if (bytes_read == 0) {
            std::cout << "客户端关闭连接:" << get_fd() << std::endl;
            m_bClosed = true;
            return false;
        }
        else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            }

            std::cerr << "读取数据错误：" << strerror(errno) << std::endl;
            m_bClosed = true;
            return false;
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

int Connection::get_fd() {
    return m_nClient_fd;
}

void Connection::process_request() {
    if (m_sRead_buffer.empty()) {
        return;
    }

    if (!m_dispatcher) {
        m_sWrite_buffer.append(m_sRead_buffer);
        m_sRead_buffer.clear();

        if (!handle_write()) {
            if (m_pEpoll) {
                m_pEpoll->del_epoll(get_fd());
            }
            m_bClosed = true;
            return;
        }

        if (!m_sWrite_buffer.empty() && m_pEpoll) {
            m_pEpoll->mod_epoll(get_fd(), EPOLLIN | EPOLLOUT | EPOLLRDHUP | EPOLLET);
        }
        return;
    }

    std::vector<protocol::Message> messages;
    auto parse_result = protocol::extract_messages(m_sRead_buffer, messages);
    if (!parse_result.success) {
        protocol::Message error{protocol::MessageType::Error};
        error.payload.assign(parse_result.error_message.begin(), parse_result.error_message.end());
        send_message(error);
        m_bClosed = true;
        if (m_pEpoll) {
            m_pEpoll->del_epoll(get_fd());
        }
        return;
    }

    if (parse_result.needs_more_data) {
        // 等待更多数据
    }

    for (auto& message : messages) {
        m_dispatcher->dispatch(message, *this);
    }
}

void Connection::send_message(const protocol::Message& message) {
    auto encoded = protocol::encode_message(message);
    m_sWrite_buffer.append(encoded);

    if (!handle_write()) {
        if (m_pEpoll) {
            m_pEpoll->del_epoll(get_fd());
        }
        m_bClosed = true;
        return;
    }

    if (!m_sWrite_buffer.empty() && m_pEpoll) {
        m_pEpoll->mod_epoll(get_fd(), EPOLLIN | EPOLLOUT | EPOLLRDHUP | EPOLLET);
    }
}

void Connection::append_raw_response(const std::vector<uint8_t>& bytes) {
    if (bytes.empty()) {
        return;
    }
    m_sWrite_buffer.append(reinterpret_cast<const char*>(bytes.data()), bytes.size());

    if (!handle_write()) {
        if (m_pEpoll) {
            m_pEpoll->del_epoll(get_fd());
        }
        m_bClosed = true;
        return;
    }

    if (!m_sWrite_buffer.empty() && m_pEpoll) {
        m_pEpoll->mod_epoll(get_fd(), EPOLLIN | EPOLLOUT | EPOLLRDHUP | EPOLLET);
    }
}

bool Connection::is_authenticated() const {
    return m_session.has_value();
}

void Connection::set_session(const std::string& username, const std::string& role) {
    m_session = SessionInfo{username, role};
}

void Connection::clear_session() {
    m_session.reset();
}
