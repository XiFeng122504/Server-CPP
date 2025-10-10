#include "Connection.h"

#include <cstring>
#include <sys/epoll.h>

#include "Epoll.h"
#include "protocol/Message.h"
#include "protocol/RequestHandler.h"

Connection::Connection(int client_fd, Epoll* epoll, std::unique_ptr<protocol::IRequestHandler> handler) {
    m_nClient_fd = client_fd;
    m_pEpoll = epoll;
    m_bClosed = false;
    m_handler = std::move(handler);
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
    if (m_sWrite_buffer.empty() && m_pEpoll) {
        m_pEpoll->mod_epoll(get_fd(), EPOLLIN | EPOLLRDHUP | EPOLLET);
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
            if (m_pEpoll) {
                m_pEpoll->del_epoll(get_fd());
            }
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
    using namespace protocol;

    while (true) {
        DecodeResult result = decode_next(m_sRead_buffer);
        switch (result.status) {
            case DecodeStatus::NotEnoughData:
                if (!m_sWrite_buffer.empty()) {
                    if (!handle_write()) {
                        if (m_pEpoll) {
                            m_pEpoll->del_epoll(get_fd());
                        }
                        m_bClosed = true;
                        return;
                    }
                }
                return;
            case DecodeStatus::InvalidType:
                send_message(make_error(result.sequence, "unsupported message type"));
                if (m_bClosed) {
                    return;
                }
                continue;
            case DecodeStatus::InvalidLength:
                send_message(make_error(result.sequence, "invalid payload length"));
                if (m_bClosed) {
                    return;
                }
                continue;
            case DecodeStatus::Ok:
                break;
        }

        if (!result.message.has_value()) {
            continue;
        }

        const Message& message = *result.message;
        if (m_handler) {
            std::optional<Message> response = m_handler->handle(message, *this);
            if (response.has_value()) {
                send_message(*response);
                if (m_bClosed) {
                    return;
                }
            }
        }
    }
}

void Connection::send_message(const protocol::Message& message) {
    queue_frame(protocol::encode(message));
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

void Connection::queue_frame(const std::string& frame) {
    m_sWrite_buffer.append(frame);
}
