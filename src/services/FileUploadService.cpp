#include "services/FileUploadService.h"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <system_error>

FileUploadService::FileUploadService(std::filesystem::path storage_root,
                                     std::chrono::seconds session_timeout)
    : m_storage_root(std::move(storage_root)), m_session_timeout(session_timeout) {
    if (m_storage_root.empty()) {
        m_storage_root = std::filesystem::current_path() / "uploads";
    }

    std::error_code ec;
    if (!std::filesystem::exists(m_storage_root, ec)) {
        if (!std::filesystem::create_directories(m_storage_root, ec)) {
            std::cerr << "Failed to create storage root: " << m_storage_root << ", error: "
                      << ec.message() << std::endl;
        }
    }

    m_canonical_root = std::filesystem::weakly_canonical(m_storage_root, ec);
    if (ec) {
        m_canonical_root = m_storage_root;
    }
}

bool FileUploadService::handle_message(int connection_id, const Message& message) {
    std::lock_guard<std::mutex> lock(m_mutex);
    cleanup_expired_locked();

    switch (message.type) {
        case FrameType::FileStart:
            return handle_file_start(connection_id, message);
        case FrameType::FileChunk:
            return handle_file_chunk(connection_id, message);
        case FrameType::FileEnd:
            return handle_file_end(connection_id, message);
        case FrameType::FileCancel:
            return handle_file_cancel(connection_id, message);
    }
    return false;
}

void FileUploadService::handle_disconnect(int connection_id) {
    std::lock_guard<std::mutex> lock(m_mutex);
    remove_session_locked(connection_id, true);
}

void FileUploadService::cleanup_expired() {
    std::lock_guard<std::mutex> lock(m_mutex);
    cleanup_expired_locked();
}

std::uint32_t FileUploadService::crc32_update(std::uint32_t crc, const std::uint8_t* data, std::size_t length) {
    static std::uint32_t table[256];
    static bool table_computed = false;

    if (!table_computed) {
        for (std::uint32_t i = 0; i < 256; ++i) {
            std::uint32_t c = i;
            for (int k = 0; k < 8; ++k) {
                if (c & 1) {
                    c = 0xEDB88320u ^ (c >> 1);
                } else {
                    c >>= 1;
                }
            }
            table[i] = c;
        }
        table_computed = true;
    }

    std::uint32_t c = crc;
    for (std::size_t i = 0; i < length; ++i) {
        c = table[(c ^ data[i]) & 0xFFu] ^ (c >> 8);
    }
    return c;
}

std::uint32_t FileUploadService::crc32_finalize(std::uint32_t crc) {
    return crc ^ 0xFFFFFFFFu;
}

std::uint32_t FileUploadService::crc32_compute(const std::vector<std::uint8_t>& data) {
    return crc32_finalize(crc32_update(0xFFFFFFFFu, data.data(), data.size()));
}

bool FileUploadService::handle_file_start(int connection_id, const Message& message) {
    auto now = std::chrono::steady_clock::now();

    auto it = m_sessions.find(connection_id);
    if (it != m_sessions.end()) {
        remove_session_locked(connection_id, true);
    }

    std::filesystem::path relative_path = std::filesystem::path(message.target_path);
    std::error_code ec;
    std::filesystem::path resolved = std::filesystem::weakly_canonical(m_storage_root / relative_path, ec);
    if (ec) {
        std::cerr << "Failed to resolve destination path: " << ec.message() << std::endl;
        return false;
    }

    auto root_it = m_canonical_root.begin();
    auto root_end = m_canonical_root.end();
    auto resolved_it = resolved.begin();
    auto resolved_end = resolved.end();
    for (; root_it != root_end; ++root_it, ++resolved_it) {
        if (resolved_it == resolved_end || *root_it != *resolved_it) {
            std::cerr << "Rejected file start for connection " << connection_id
                      << ": path escapes storage root." << std::endl;
            return false;
        }
    }

    if (resolved_it == resolved_end) {
        std::cerr << "Rejected file start for connection " << connection_id
                  << ": invalid destination." << std::endl;
        return false;
    }

    if (!std::filesystem::exists(resolved.parent_path(), ec)) {
        std::cerr << "Destination directory does not exist for " << resolved << std::endl;
        return false;
    }

    UploadSession session;
    session.destination_path = resolved;
    session.temporary_path = resolved;
    session.temporary_path += ".upload";
    session.expected_chunks = message.total_chunks;
    session.total_size = message.total_size;
    session.expected_final_checksum = message.final_checksum;
    session.last_activity = now;

    session.temp_stream.open(session.temporary_path, std::ios::binary | std::ios::trunc);
    if (!session.temp_stream.is_open()) {
        std::cerr << "Failed to open temporary file: " << session.temporary_path << std::endl;
        return false;
    }

    m_sessions.emplace(connection_id, std::move(session));
    std::cout << "Initialized upload session for connection " << connection_id
              << ", file id: " << message.file_id << std::endl;
    return true;
}

bool FileUploadService::handle_file_chunk(int connection_id, const Message& message) {
    auto it = m_sessions.find(connection_id);
    if (it == m_sessions.end()) {
        std::cerr << "Received chunk without active session for connection " << connection_id << std::endl;
        return false;
    }

    UploadSession& session = it->second;
    session.last_activity = std::chrono::steady_clock::now();

    const std::uint32_t computed_checksum = crc32_compute(message.payload);
    if (computed_checksum != message.checksum) {
        std::cerr << "Checksum mismatch for connection " << connection_id
                  << " sequence " << message.sequence_number << std::endl;
        return false;
    }

    if (session.received_sequences.count(message.sequence_number)) {
        std::cout << "Duplicate chunk sequence " << message.sequence_number
                  << " for connection " << connection_id << " ignored." << std::endl;
        return true;
    }

    session.received_sequences.insert(message.sequence_number);
    session.pending_chunks.emplace(message.sequence_number, message.payload);

    flush_pending_chunks(session);
    std::cout << "Processed chunk " << message.sequence_number << " for connection " << connection_id << std::endl;
    return true;
}

bool FileUploadService::handle_file_end(int connection_id, const Message& message) {
    auto it = m_sessions.find(connection_id);
    if (it == m_sessions.end()) {
        std::cerr << "Received end without active session for connection " << connection_id << std::endl;
        return false;
    }

    UploadSession& session = it->second;
    session.last_activity = std::chrono::steady_clock::now();

    flush_pending_chunks(session);

    if (session.expected_chunks != 0 && session.next_sequence_to_write != session.expected_chunks) {
        std::cerr << "Not all chunks received for connection " << connection_id << std::endl;
        remove_session_locked(connection_id, true);
        return false;
    }

    if (session.total_size != 0 && session.total_size != session.written_bytes) {
        std::cerr << "Written bytes mismatch for connection " << connection_id << std::endl;
        remove_session_locked(connection_id, true);
        return false;
    }

    const std::uint32_t computed_final = crc32_finalize(session.running_crc);
    if (computed_final != message.final_checksum || computed_final != session.expected_final_checksum) {
        std::cerr << "Final checksum mismatch for connection " << connection_id << std::endl;
        remove_session_locked(connection_id, true);
        return false;
    }

    session.temp_stream.flush();
    session.temp_stream.close();

    std::error_code ec;
    if (std::filesystem::exists(session.destination_path, ec) && !ec) {
        std::filesystem::remove(session.destination_path, ec);
        if (ec) {
            std::cerr << "Failed to remove existing destination file: " << ec.message() << std::endl;
            remove_session_locked(connection_id, true);
            return false;
        }
    }

    std::filesystem::rename(session.temporary_path, session.destination_path, ec);
    if (ec) {
        std::cerr << "Failed to move temporary file to destination: " << ec.message() << std::endl;
        remove_session_locked(connection_id, true);
        return false;
    }

    std::cout << "Upload completed for connection " << connection_id
              << " -> " << session.destination_path << std::endl;
    m_sessions.erase(it);
    return true;
}

bool FileUploadService::handle_file_cancel(int connection_id, const Message& message) {
    auto it = m_sessions.find(connection_id);
    if (it == m_sessions.end()) {
        std::cerr << "Cancel without session for connection " << connection_id << std::endl;
        return false;
    }

    std::cout << "Upload canceled for connection " << connection_id
              << ": " << message.cancel_reason << std::endl;
    remove_session_locked(connection_id, true);
    return true;
}

void FileUploadService::flush_pending_chunks(UploadSession& session) {
    auto it = session.pending_chunks.find(session.next_sequence_to_write);
    while (it != session.pending_chunks.end()) {
        const auto& data = it->second;
        session.temp_stream.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
        if (!session.temp_stream.good()) {
            std::cerr << "Failed to write chunk sequence " << session.next_sequence_to_write << std::endl;
            break;
        }

        session.running_crc = crc32_update(session.running_crc, data.data(), data.size());
        session.written_bytes += data.size();
        session.pending_chunks.erase(it);
        ++session.next_sequence_to_write;
        it = session.pending_chunks.find(session.next_sequence_to_write);
    }
}

void FileUploadService::remove_session_locked(int connection_id, bool delete_temp_file) {
    auto it = m_sessions.find(connection_id);
    if (it == m_sessions.end()) {
        return;
    }

    UploadSession& session = it->second;
    if (session.temp_stream.is_open()) {
        session.temp_stream.close();
    }

    if (delete_temp_file) {
        std::error_code ec;
        std::filesystem::remove(session.temporary_path, ec);
    }

    m_sessions.erase(it);
}

void FileUploadService::cleanup_expired_locked() {
    if (m_session_timeout.count() <= 0) {
        return;
    }

    auto now = std::chrono::steady_clock::now();
    for (auto it = m_sessions.begin(); it != m_sessions.end();) {
        if (now - it->second.last_activity > m_session_timeout) {
            std::cout << "Cleaning up expired upload session for connection " << it->first << std::endl;
            if (it->second.temp_stream.is_open()) {
                it->second.temp_stream.close();
            }
            std::error_code ec;
            std::filesystem::remove(it->second.temporary_path, ec);
            it = m_sessions.erase(it);
        } else {
            ++it;
        }
    }
}
