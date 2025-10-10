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
#include <array>
#include <cstring>
#include <sys/epoll.h>
#include <sstream>
#include <vector>
#include <algorithm>
#include <cctype>
#include "Epoll.h"
#include "handlers/LoginHandler.h"
#include "handlers/EchoHandler.h"
#include "handlers/FileUploadHandler.h"

Connection::Connection(int client_fd, Epoll* epoll, FileUploadService* upload_service) {
    m_nClient_fd = client_fd;
    m_pEpoll = epoll;
    m_bClosed = false;
    m_upload_service = upload_service;
}

Connection::~Connection() {
    if (m_nClient_fd >= 0) {
        close(m_nClient_fd);
        if (m_upload_service) {
            m_upload_service->handle_disconnect(m_nClient_fd);
        }
    }
}

bool Connection::handle_write() {
    while (!m_sWrite_buffer.empty()) {
        ssize_t result = write(m_nClient_fd, m_sWrite_buffer.data(), m_sWrite_buffer.size());

        if (result < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // 写缓冲区满了，等下次EPOLLOUT再写
                break;
            }
            std::cerr << "写入失败: " << get_fd() << " 错误: " << strerror(errno) << std::endl;
            return false;
        }
        m_sWrite_buffer.erase(m_sWrite_buffer.begin(), m_sWrite_buffer.begin() + result);
    }
    if (m_sWrite_buffer.empty() && m_pEpoll) {
        m_pEpoll->mod_epoll(get_fd(), EPOLLIN | EPOLLRDHUP | EPOLLET);
    }
    return true;
}
bool Connection::handle_read() {

    std::array<char, BUFFER_SIZE> buffer{};
    bool peer_closed = false;

    while (true)
    {
        ssize_t bytes_read = read(m_nClient_fd, buffer.data(), BUFFER_SIZE);
        if (bytes_read > 0) {
            m_sRead_buffer.insert(m_sRead_buffer.end(), buffer.data(), buffer.data() + bytes_read);
            continue;
        }

        if (bytes_read == 0) {
            peer_closed = true;
            break;
        }

        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            break;
        }

        std::cerr << "读取数据错误：" << strerror(errno) << std::endl;
        m_bClosed = true;
        if (m_upload_service) {
            m_upload_service->handle_disconnect(m_nClient_fd);
        }
        return false;
    }
    if (!m_sRead_buffer.empty()) {
        process_request();
    }

    if (peer_closed) {
        std::cout << "客户端关闭连接:" << get_fd() << std::endl;
        m_bClosed = true;
        if (m_upload_service) {
            m_upload_service->handle_disconnect(m_nClient_fd);
        }
        return false;
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
    size_t newline_pos = std::string::npos;
    while ((newline_pos = m_sRead_buffer.find('\n')) != std::string::npos) {
        std::string message = m_sRead_buffer.substr(0, newline_pos);
        m_sRead_buffer.erase(0, newline_pos + 1);
        process_message(message);
        if (m_bClosed) {
            return;
        }
    }
}

void Connection::process_message(const std::string& message) {
    if (message.empty()) {
        return;
    }
    if (!m_sWrite_buffer.empty() && m_pEpoll) {
        m_pEpoll->mod_epoll(get_fd(), EPOLLIN | EPOLLOUT | EPOLLRDHUP | EPOLLET);

    static LoginHandler login_handler;
    static EchoHandler echo_handler;
    static FileUploadHandler file_upload_handler;

    std::istringstream iss(message);
    std::string command;
    iss >> command;

    std::string payload;
    std::getline(iss, payload);
    if (!payload.empty() && payload.front() == ' ') {
        payload.erase(0, 1);
    }

    std::transform(command.begin(), command.end(), command.begin(), [](unsigned char c) { return std::toupper(c); });

    if (command == "LOGIN") {
        login_handler.handle(*this, payload);
    } else if (command == "LOGOUT") {
        if (!session().has_value()) {
            send_frame("ERROR", "NOT_LOGGED_IN");
            return;
        }
        clear_session();
        send_frame("LOGOUT_OK", "BYE");
    } else if (command == "ECHO") {
        echo_handler.handle(*this, payload);
    } else if (command == "UPLOAD") {
        file_upload_handler.handle(*this, payload);
    } else {
        send_frame("ERROR", "UNKNOWN_COMMAND");
    }
}

const std::optional<UserSession>& Connection::session() const {
    return m_session;
}

void Connection::set_session(const UserSession& session) {
    m_session = session;
}

void Connection::clear_session() {
    m_session.reset();
}

void Connection::send_frame(const std::string& type, const std::string& payload) {
    m_sWrite_buffer.append(type);
    if (!payload.empty()) {
        m_sWrite_buffer.push_back(' ');
        m_sWrite_buffer.append(payload);
    }
    m_sWrite_buffer.push_back('\n');

    while (!m_sRead_buffer.empty()) {
        Message message;
        auto decode_result = Message::try_decode(m_sRead_buffer, message);
        if (decode_result.status == Message::DecodeResult::Incomplete) {
            break;
        }

        if (decode_result.status == Message::DecodeResult::Error) {
            std::cerr << "Failed to decode message for connection " << m_nClient_fd << std::endl;
            if (decode_result.bytes_consumed == 0 || decode_result.bytes_consumed > m_sRead_buffer.size()) {
                m_sRead_buffer.clear();
            } else {
                m_sRead_buffer.erase(m_sRead_buffer.begin(), m_sRead_buffer.begin() + decode_result.bytes_consumed);
            }
            continue;
        }

        if (decode_result.bytes_consumed > 0 && decode_result.bytes_consumed <= m_sRead_buffer.size()) {
            m_sRead_buffer.erase(m_sRead_buffer.begin(), m_sRead_buffer.begin() + decode_result.bytes_consumed);
        } else {
            m_sRead_buffer.clear();
        }

        if (m_upload_service && !m_upload_service->handle_message(m_nClient_fd, message)) {
            std::cerr << "Upload service failed to handle message for connection " << m_nClient_fd << std::endl;
        }
    }
}

void Connection::set_upload_service(FileUploadService* service) {
    m_upload_service = service;
}

void Connection::queue_frame(const std::string& frame) {
    m_sWrite_buffer.append(frame);
}
