#include <cassert>
#include <cstdint>
#include <iostream>

#include "protocol/Message.h"

static void test_valid_frame() {
    protocol::Message request{protocol::MessageType::EchoRequest, 42, "hello"};
    std::string buffer = protocol::encode(request);

    protocol::DecodeResult result = protocol::decode_next(buffer);
    assert(result.status == protocol::DecodeStatus::Ok);
    assert(result.message.has_value());
    assert(result.message->type == protocol::MessageType::EchoRequest);
    assert(result.message->sequence == 42);
    assert(result.message->payload == "hello");
    assert(buffer.empty());
}

static void test_truncated_frame() {
    protocol::Message request{protocol::MessageType::LoginRequest, 7, "user"};
    std::string frame = protocol::encode(request);
    frame.resize(frame.size() - 2); // simulate truncation

    std::string buffer = frame;
    protocol::DecodeResult result = protocol::decode_next(buffer);
    assert(result.status == protocol::DecodeStatus::NotEnoughData);
    assert(!result.message.has_value());
    assert(buffer == frame);
}

static void test_invalid_type() {
    protocol::Message request{protocol::MessageType::EchoRequest, 99, "payload"};
    std::string frame = protocol::encode(request);
    frame[1] = static_cast<char>(0xFE); // change type to unknown value 0x01FE -> invalid

    std::string buffer = frame;
    protocol::DecodeResult result = protocol::decode_next(buffer);
    assert(result.status == protocol::DecodeStatus::InvalidType);
    assert(!result.message.has_value());
    assert(result.sequence == 99);
    assert(buffer.empty());
}

int main() {
    test_valid_frame();
    test_truncated_frame();
    test_invalid_type();
    std::cout << "All protocol tests passed" << std::endl;
    return 0;
}
