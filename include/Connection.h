#ifndef _CONNECTION_H
#define _CONNECTION_H
#define BUFFER_SIZE 4096
#include <memory>
#include <string>
#include <optional>
#include <vector>
#include <cstdint>
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
class RequestDispatcher;
struct Message;
}

namespace protocol {
class IRequestHandler;
}

class Connection {
public:
    struct SessionInfo {
        std::string username;
        std::string role;
    };

    Connection(int client_fd, Epoll* epoll = nullptr, protocol::RequestDispatcher* dispatcher = nullptr);
    ~Connection();

    bool handle_write();
    bool handle_read();
    bool handle_err();
    bool is_close();
    bool write_complete();

    int get_fd();

    void send_message(const protocol::Message& message);
    void append_raw_response(const std::vector<uint8_t>& bytes);

    bool is_authenticated() const;
    void set_session(const std::string& username, const std::string& role = "user");
    void clear_session();
    const std::optional<SessionInfo>& session() const { return m_session; }

    void process_request();
private:
    void queue_frame(const std::string& frame);

    Epoll* m_pEpoll;
    protocol::RequestDispatcher* m_dispatcher;
    int m_nClient_fd;
    std::vector<uint8_t> m_sRead_buffer;
    std::vector<uint8_t> m_sWrite_buffer;
    bool m_bClosed;
    std::optional<SessionInfo> m_session;
};

#endif//_CONNECTION_H
