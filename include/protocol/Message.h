#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

namespace protocol {

constexpr std::size_t kFrameHeaderSize = sizeof(std::uint16_t) + sizeof(std::uint32_t) + sizeof(std::uint32_t);
constexpr std::uint32_t kMaxPayloadLength = 1U << 20; // 1 MiB safety limit

enum class MessageType : std::uint16_t {
    LoginRequest = 1,
    LoginResponse = 2,
    EchoRequest = 3,
    EchoResponse = 4,
    FileReadRequest = 5,
    FileReadResponse = 6,
    FileWriteRequest = 7,
    FileWriteResponse = 8,
    Error = 0xFFFF
};

struct Message {
    MessageType type{MessageType::Error};
    std::uint32_t sequence{0};
    std::string payload;
};

enum class DecodeStatus {
    Ok,
    NotEnoughData,
    InvalidType,
    InvalidLength
};

struct DecodeResult {
    DecodeStatus status{DecodeStatus::NotEnoughData};
    std::optional<Message> message;
    std::uint32_t sequence{0};
};

std::string encode(const Message& message);
DecodeResult decode_next(std::string& buffer);

bool is_valid_type(MessageType type) noexcept;
const char* to_string(MessageType type) noexcept;

Message make_error(std::uint32_t sequence, const std::string& reason);

} // namespace protocol
