#include "protocol/Message.h"

#include <arpa/inet.h>
#include <cstring>

namespace protocol {

namespace {
constexpr std::size_t HEADER_SIZE = sizeof(std::uint8_t) + sizeof(std::uint32_t);
}

std::string encode_message(const Message& message) {
    std::string encoded;
    encoded.resize(HEADER_SIZE + message.payload.size());

    encoded[0] = static_cast<char>(message.type);

    std::uint32_t length = static_cast<std::uint32_t>(message.payload.size());
    std::uint32_t network_length = htonl(length);
    std::memcpy(encoded.data() + 1, &network_length, sizeof(network_length));

    if (!message.payload.empty()) {
        std::memcpy(encoded.data() + HEADER_SIZE, message.payload.data(), message.payload.size());
    }

    return encoded;
}

ParseResult extract_messages(std::string& buffer, std::vector<Message>& out_messages) {
    ParseResult result;
    std::size_t offset = 0;
    while (buffer.size() - offset >= HEADER_SIZE) {
        auto type = static_cast<MessageType>(static_cast<unsigned char>(buffer[offset]));

        std::uint32_t network_length{};
        std::memcpy(&network_length, buffer.data() + offset + 1, sizeof(network_length));
        std::uint32_t length = ntohl(network_length);

        if (length > (10 * 1024 * 1024)) {
            result.success = false;
            result.error_message = "message payload exceeds 10MB limit";
            return result;
        }

        if (buffer.size() - offset - HEADER_SIZE < length) {
            result.needs_more_data = true;
            break;
        }

        Message message;
        message.type = type;
        if (length > 0) {
            message.payload.resize(length);
            std::memcpy(message.payload.data(), buffer.data() + offset + HEADER_SIZE, length);
        }
        out_messages.emplace_back(std::move(message));
        offset += HEADER_SIZE + length;
    }

    if (offset > 0) {
        buffer.erase(0, offset);
    }

    return result;
}

}
