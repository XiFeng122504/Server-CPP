#ifndef SERVER_CPP_MESSAGE_H
#define SERVER_CPP_MESSAGE_H

#include <cstdint>
#include <string>
#include <vector>

enum class FrameType : std::uint8_t {
    FileStart = 1,
    FileChunk = 2,
    FileEnd = 3,
    FileCancel = 4,
};

class Message {
public:
    struct DecodeResult {
        enum Status {
            Success,
            Incomplete,
            Error,
        } status;
        std::size_t bytes_consumed = 0;
    };

    FrameType type = FrameType::FileChunk;
    std::string file_id;
    std::uint32_t sequence_number = 0;
    std::uint32_t checksum = 0;
    std::string target_path;
    std::uint64_t total_size = 0;
    std::uint32_t total_chunks = 0;
    std::uint32_t final_checksum = 0;
    std::string cancel_reason;
    std::vector<std::uint8_t> payload;

    [[nodiscard]] std::vector<std::uint8_t> encode() const;
    static DecodeResult try_decode(const std::vector<std::uint8_t>& buffer, Message& message);

private:
    static DecodeResult decode_internal(const std::vector<std::uint8_t>& buffer, Message& message);
};

#endif // SERVER_CPP_MESSAGE_H
