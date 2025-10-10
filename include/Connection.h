#ifndef _CONNECTION_H
#define _CONNECTION_H
#define BUFFER_SIZE 4096
#include <memory>
#include <string>
#include <optional>
#include <unistd.h>
#include <iostream>
#include <errno.h>  // 或 <cerrno>
#include <sys/socket.h>
#include <vector>

#include "Message.h"

class FileUploadService;

#include "UserSession.h"

#include "protocol/Message.h"

class Epoll;

namespace protocol {
class IRequestHandler;
}

class Connection {
public:
    Connection(int client_fd, Epoll* epoll = nullptr, std::unique_ptr<protocol::IRequestHandler> handler = nullptr);
    Connection(int client_fd, Epoll* epoll = nullptr, FileUploadService* upload_service = nullptr);
    ~Connection();

    bool handle_write();
    bool handle_read();
    bool handle_err();
    bool is_close();
    bool write_complete();

    int get_fd();

    void process_request();

    void send_message(const protocol::Message& message);

    void process_message(const std::string& message);

    const std::optional<UserSession>& session() const;
    void set_session(const UserSession& session);
    void clear_session();

    void send_frame(const std::string& type, const std::string& payload = "");
private:
    void queue_frame(const std::string& frame);

    Epoll* m_pEpoll;
    int m_nClient_fd;
    std::vector<uint8_t> m_sRead_buffer;
    std::vector<uint8_t> m_sWrite_buffer;
    bool m_bClosed;
    std::unique_ptr<protocol::IRequestHandler> m_handler;
    std::optional<UserSession> m_session;
};

#endif//_CONNECTION_H
