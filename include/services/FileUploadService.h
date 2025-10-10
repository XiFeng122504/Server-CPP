#ifndef SERVER_CPP_SERVICES_FILE_UPLOAD_SERVICE_H
#define SERVER_CPP_SERVICES_FILE_UPLOAD_SERVICE_H

#include <fstream>
#include <string>
#include <unordered_map>
#include <cstdint>

#include "protocol/RequestDispatcher.h"

struct FileUploadSession {
    std::string filename;
    std::ofstream stream;
    std::size_t received_bytes{0};
};

class FileUploadService : public protocol::IRequestHandler {
public:
    explicit FileUploadService(const std::string& storage_root);
    ~FileUploadService() override;

    void handle(const protocol::Message& message, Connection& connection) override;
    void handle_connection_closed(Connection& connection) override;

private:
    bool ensure_storage_root();
    void handle_begin(const protocol::Message& message, Connection& connection);
    void handle_chunk(const protocol::Message& message, Connection& connection);
    void handle_end(const protocol::Message& message, Connection& connection);
    void abort_session(Connection& connection, const std::string& reason, bool notify_client = true);
    void send_ack(Connection& connection, const std::string& text);
    void send_error(Connection& connection, const std::string& text);

    std::string session_key(Connection& connection) const;

    std::string m_storage_root;
    std::unordered_map<std::string, FileUploadSession> m_sessions;
};

#endif // SERVER_CPP_SERVICES_FILE_UPLOAD_SERVICE_H
