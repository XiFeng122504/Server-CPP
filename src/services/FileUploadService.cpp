#include "services/FileUploadService.h"

#include <filesystem>
#include <iostream>

#include "Connection.h"

namespace {
std::string bytes_to_string(const std::vector<std::uint8_t>& bytes) {
    return std::string(bytes.begin(), bytes.end());
}
}

FileUploadService::FileUploadService(const std::string& storage_root)
    : m_storage_root(storage_root) {
    ensure_storage_root();
}

FileUploadService::~FileUploadService() {
    for (auto& [key, session] : m_sessions) {
        if (session.stream.is_open()) {
            session.stream.close();
        }
    }
}

void FileUploadService::handle(const protocol::Message& message, Connection& connection) {
    switch (message.type) {
        case protocol::MessageType::FileUploadBegin:
            handle_begin(message, connection);
            break;
        case protocol::MessageType::FileUploadChunk:
            handle_chunk(message, connection);
            break;
        case protocol::MessageType::FileUploadEnd:
            handle_end(message, connection);
            break;
        default:
            send_error(connection, "unsupported file upload message type");
            break;
    }
}

void FileUploadService::handle_connection_closed(Connection& connection) {
    abort_session(connection, "connection closed", false);
}

bool FileUploadService::ensure_storage_root() {
    namespace fs = std::filesystem;
    std::error_code ec;
    if (m_storage_root.empty()) {
        m_storage_root = "uploads";
    }
    fs::path path = fs::path(m_storage_root).lexically_normal();
    if (fs::exists(path, ec)) {
        if (!fs::is_directory(path, ec)) {
            std::cerr << "Upload path exists but is not a directory: " << path << std::endl;
            return false;
        }
        return true;
    }
    if (!fs::create_directories(path, ec)) {
        std::cerr << "Failed to create upload directory: " << path << " error: " << ec.message() << std::endl;
        return false;
    }
    return true;
}

void FileUploadService::handle_begin(const protocol::Message& message, Connection& connection) {
    if (!connection.is_authenticated()) {
        send_error(connection, "login required before uploading files");
        return;
    }

    if (!ensure_storage_root()) {
        send_error(connection, "server storage unavailable");
        return;
    }

    std::string filename = bytes_to_string(message.payload);
    if (filename.empty()) {
        send_error(connection, "filename missing");
        return;
    }

    namespace fs = std::filesystem;
    fs::path candidate = fs::path(filename).filename();
    fs::path target = fs::path(m_storage_root) / candidate;
    target = target.lexically_normal();

    auto key = session_key(connection);
    auto& session = m_sessions[key];
    if (session.stream.is_open()) {
        session.stream.close();
    }
    session.filename = target.string();
    session.received_bytes = 0;
    session.stream.open(session.filename, std::ios::binary | std::ios::trunc);
    if (!session.stream.is_open()) {
        m_sessions.erase(key);
        send_error(connection, "failed to open destination file");
        return;
    }

    send_ack(connection, "upload started");
}

void FileUploadService::handle_chunk(const protocol::Message& message, Connection& connection) {
    if (!connection.is_authenticated()) {
        send_error(connection, "login required before uploading files");
        return;
    }

    auto key = session_key(connection);
    auto it = m_sessions.find(key);
    if (it == m_sessions.end() || !it->second.stream.is_open()) {
        send_error(connection, "no active upload session");
        return;
    }

    auto& session = it->second;
    if (!message.payload.empty()) {
        session.stream.write(reinterpret_cast<const char*>(message.payload.data()), static_cast<std::streamsize>(message.payload.size()));
        if (!session.stream.good()) {
            abort_session(connection, "failed to write chunk to disk");
            return;
        }
        session.received_bytes += message.payload.size();
    }

    send_ack(connection, "chunk received");
}

void FileUploadService::handle_end(const protocol::Message& message, Connection& connection) {
    (void)message;
    if (!connection.is_authenticated()) {
        send_error(connection, "login required before uploading files");
        return;
    }

    auto key = session_key(connection);
    auto it = m_sessions.find(key);
    if (it == m_sessions.end() || !it->second.stream.is_open()) {
        send_error(connection, "no active upload session to finalize");
        return;
    }

    it->second.stream.close();
    m_sessions.erase(it);
    send_ack(connection, "upload complete");
}

void FileUploadService::abort_session(Connection& connection, const std::string& reason, bool notify_client) {
    auto key = session_key(connection);
    auto it = m_sessions.find(key);
    if (it != m_sessions.end()) {
        if (it->second.stream.is_open()) {
            it->second.stream.close();
            std::error_code ec;
            std::filesystem::remove(it->second.filename, ec);
        }
        m_sessions.erase(it);
    }
    if (notify_client) {
        send_error(connection, reason);
    }
}

void FileUploadService::send_ack(Connection& connection, const std::string& text) {
    protocol::Message ack{protocol::MessageType::Ack};
    ack.payload.assign(text.begin(), text.end());
    connection.send_message(ack);
}

void FileUploadService::send_error(Connection& connection, const std::string& text) {
    protocol::Message error{protocol::MessageType::Error};
    error.payload.assign(text.begin(), text.end());
    connection.send_message(error);
}

std::string FileUploadService::session_key(Connection& connection) const {
    return std::to_string(connection.get_fd());
}
