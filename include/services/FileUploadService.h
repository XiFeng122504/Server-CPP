#ifndef SERVER_CPP_FILEUPLOADSERVICE_H
#define SERVER_CPP_FILEUPLOADSERVICE_H

#include "Message.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>

class FileUploadService {
public:
    struct UploadSession {
        std::filesystem::path destination_path;
        std::filesystem::path temporary_path;
        std::ofstream temp_stream;
        std::uint32_t expected_chunks = 0;
        std::uint64_t total_size = 0;
        std::uint32_t expected_final_checksum = 0;
        std::uint32_t next_sequence_to_write = 0;
        std::uint64_t written_bytes = 0;
        std::uint32_t running_crc = 0xFFFFFFFFu;
        std::map<std::uint32_t, std::vector<std::uint8_t>> pending_chunks;
        std::unordered_set<std::uint32_t> received_sequences;
        std::chrono::steady_clock::time_point last_activity;
    };

    explicit FileUploadService(std::filesystem::path storage_root,
                               std::chrono::seconds session_timeout = std::chrono::seconds(60));

    bool handle_message(int connection_id, const Message& message);
    void handle_disconnect(int connection_id);
    void cleanup_expired();

private:
    std::filesystem::path m_storage_root;
    std::filesystem::path m_canonical_root;
    std::chrono::seconds m_session_timeout;
    std::mutex m_mutex;
    std::unordered_map<int, UploadSession> m_sessions;

    static std::uint32_t crc32_update(std::uint32_t crc, const std::uint8_t* data, std::size_t length);
    static std::uint32_t crc32_finalize(std::uint32_t crc);
    static std::uint32_t crc32_compute(const std::vector<std::uint8_t>& data);

    bool handle_file_start(int connection_id, const Message& message);
    bool handle_file_chunk(int connection_id, const Message& message);
    bool handle_file_end(int connection_id, const Message& message);
    bool handle_file_cancel(int connection_id, const Message& message);

    void flush_pending_chunks(UploadSession& session);
    void remove_session_locked(int connection_id, bool delete_temp_file);
    void cleanup_expired_locked();
};

#endif // SERVER_CPP_FILEUPLOADSERVICE_H
