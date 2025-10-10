#include "Connection.h"
#include <array>
#include <cstring>
#include <sys/epoll.h>
#include "Epoll.h"
#include "services/FileUploadService.h"

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

    if (m_upload_service) {
        m_upload_service->cleanup_expired();
    }

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
