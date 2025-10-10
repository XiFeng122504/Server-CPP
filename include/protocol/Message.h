#ifndef SERVER_CPP_PROTOCOL_MESSAGE_H
#define SERVER_CPP_PROTOCOL_MESSAGE_H

#include <cstdint>
#include <string>
#include <vector>
#include <optional>

namespace protocol {

enum class MessageType : std::uint8_t {
    LoginRequest = 1,
    EchoRequest = 2,
    FileUploadBegin = 3,
    FileUploadChunk = 4,
    FileUploadEnd = 5,
    Ack = 100,
    Error = 101
};

struct Message {
    MessageType type{MessageType::EchoRequest};
    std::vector<std::uint8_t> payload{};
};

struct ParseResult {
    bool success{true};
    bool needs_more_data{false};
    std::string error_message{};
};

std::string encode_message(const Message& message);
ParseResult extract_messages(std::string& buffer, std::vector<Message>& out_messages);

}

#endif // SERVER_CPP_PROTOCOL_MESSAGE_H
