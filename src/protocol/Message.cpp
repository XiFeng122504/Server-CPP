#include "protocol/Message.h"

#include <algorithm>
#include <array>
#include <cstring>

namespace protocol {

namespace {

constexpr std::array<std::pair<MessageType, const char*>, 9> kTypeNames{{
    {MessageType::LoginRequest, "login_request"},
    {MessageType::LoginResponse, "login_response"},
    {MessageType::EchoRequest, "echo_request"},
    {MessageType::EchoResponse, "echo_response"},
    {MessageType::FileReadRequest, "file_read_request"},
    {MessageType::FileReadResponse, "file_read_response"},
    {MessageType::FileWriteRequest, "file_write_request"},
    {MessageType::FileWriteResponse, "file_write_response"},
    {MessageType::Error, "error"},
}};

constexpr bool is_known_type(MessageType type) noexcept {
    switch (type) {
        case MessageType::LoginRequest:
        case MessageType::LoginResponse:
        case MessageType::EchoRequest:
        case MessageType::EchoResponse:
        case MessageType::FileReadRequest:
        case MessageType::FileReadResponse:
        case MessageType::FileWriteRequest:
        case MessageType::FileWriteResponse:
        case MessageType::Error:
            return true;
    }
    return false;
}

} // namespace

std::string encode(const Message& message) {
    std::string frame;
    frame.resize(kFrameHeaderSize + message.payload.size());

    auto type_value = static_cast<std::uint16_t>(message.type);
    frame[0] = static_cast<char>((type_value >> 8) & 0xFF);
    frame[1] = static_cast<char>(type_value & 0xFF);

    auto seq_value = static_cast<std::uint32_t>(message.sequence);
    frame[2] = static_cast<char>((seq_value >> 24) & 0xFF);
    frame[3] = static_cast<char>((seq_value >> 16) & 0xFF);
    frame[4] = static_cast<char>((seq_value >> 8) & 0xFF);
    frame[5] = static_cast<char>(seq_value & 0xFF);

    auto length_value = static_cast<std::uint32_t>(message.payload.size());
    frame[6] = static_cast<char>((length_value >> 24) & 0xFF);
    frame[7] = static_cast<char>((length_value >> 16) & 0xFF);
    frame[8] = static_cast<char>((length_value >> 8) & 0xFF);
    frame[9] = static_cast<char>(length_value & 0xFF);

    if (!message.payload.empty()) {
        std::memcpy(frame.data() + kFrameHeaderSize, message.payload.data(), message.payload.size());
    }

    return frame;
}

DecodeResult decode_next(std::string& buffer) {
    DecodeResult result{};

    if (buffer.size() < kFrameHeaderSize) {
        result.status = DecodeStatus::NotEnoughData;
        return result;
    }

    const unsigned char* raw = reinterpret_cast<const unsigned char*>(buffer.data());
    std::uint16_t type_value = static_cast<std::uint16_t>(raw[0] << 8 | raw[1]);
    std::uint32_t sequence =
        (static_cast<std::uint32_t>(raw[2]) << 24) |
        (static_cast<std::uint32_t>(raw[3]) << 16) |
        (static_cast<std::uint32_t>(raw[4]) << 8) |
        (static_cast<std::uint32_t>(raw[5]));
    std::uint32_t payload_length =
        (static_cast<std::uint32_t>(raw[6]) << 24) |
        (static_cast<std::uint32_t>(raw[7]) << 16) |
        (static_cast<std::uint32_t>(raw[8]) << 8) |
        (static_cast<std::uint32_t>(raw[9]));

    result.sequence = sequence;

    std::size_t frame_size = kFrameHeaderSize + static_cast<std::size_t>(payload_length);
    if (payload_length > kMaxPayloadLength) {
        if (buffer.size() < frame_size) {
            result.status = DecodeStatus::NotEnoughData;
            return result;
        }
        buffer.erase(0, frame_size);
        result.status = DecodeStatus::InvalidLength;
        return result;
    }

    if (buffer.size() < frame_size) {
        result.status = DecodeStatus::NotEnoughData;
        return result;
    }

    MessageType type = static_cast<MessageType>(type_value);
    if (!is_known_type(type)) {
        buffer.erase(0, frame_size);
        result.status = DecodeStatus::InvalidType;
        return result;
    }

    Message message;
    message.type = type;
    message.sequence = sequence;
    if (payload_length > 0) {
        message.payload.assign(buffer.data() + kFrameHeaderSize, payload_length);
    } else {
        message.payload.clear();
    }

    buffer.erase(0, frame_size);
    result.status = DecodeStatus::Ok;
    result.message = std::move(message);
    return result;
}

bool is_valid_type(MessageType type) noexcept {
    return is_known_type(type);
}

const char* to_string(MessageType type) noexcept {
    auto it = std::find_if(kTypeNames.begin(), kTypeNames.end(), [type](const auto& item) {
        return item.first == type;
    });
    if (it != kTypeNames.end()) {
        return it->second;
    }
    return "unknown";
}

Message make_error(std::uint32_t sequence, const std::string& reason) {
    Message message;
    message.type = MessageType::Error;
    message.sequence = sequence;
    message.payload = reason;
    return message;
}

} // namespace protocol
