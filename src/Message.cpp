#include "Message.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace {

constexpr std::uint32_t read_u32(const std::uint8_t* data) {
    return (static_cast<std::uint32_t>(data[0]) << 24) |
           (static_cast<std::uint32_t>(data[1]) << 16) |
           (static_cast<std::uint32_t>(data[2]) << 8) |
           static_cast<std::uint32_t>(data[3]);
}

constexpr std::uint16_t read_u16(const std::uint8_t* data) {
    return static_cast<std::uint16_t>(data[0] << 8 | data[1]);
}

constexpr std::uint64_t read_u64(const std::uint8_t* data) {
    return (static_cast<std::uint64_t>(data[0]) << 56) |
           (static_cast<std::uint64_t>(data[1]) << 48) |
           (static_cast<std::uint64_t>(data[2]) << 40) |
           (static_cast<std::uint64_t>(data[3]) << 32) |
           (static_cast<std::uint64_t>(data[4]) << 24) |
           (static_cast<std::uint64_t>(data[5]) << 16) |
           (static_cast<std::uint64_t>(data[6]) << 8) |
           static_cast<std::uint64_t>(data[7]);
}

void write_u16(std::vector<std::uint8_t>& buffer, std::uint16_t value) {
    buffer.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
    buffer.push_back(static_cast<std::uint8_t>(value & 0xFF));
}

void write_u32(std::vector<std::uint8_t>& buffer, std::uint32_t value) {
    buffer.push_back(static_cast<std::uint8_t>((value >> 24) & 0xFF));
    buffer.push_back(static_cast<std::uint8_t>((value >> 16) & 0xFF));
    buffer.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
    buffer.push_back(static_cast<std::uint8_t>(value & 0xFF));
}

void write_u64(std::vector<std::uint8_t>& buffer, std::uint64_t value) {
    buffer.push_back(static_cast<std::uint8_t>((value >> 56) & 0xFF));
    buffer.push_back(static_cast<std::uint8_t>((value >> 48) & 0xFF));
    buffer.push_back(static_cast<std::uint8_t>((value >> 40) & 0xFF));
    buffer.push_back(static_cast<std::uint8_t>((value >> 32) & 0xFF));
    buffer.push_back(static_cast<std::uint8_t>((value >> 24) & 0xFF));
    buffer.push_back(static_cast<std::uint8_t>((value >> 16) & 0xFF));
    buffer.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
    buffer.push_back(static_cast<std::uint8_t>(value & 0xFF));
}

} // namespace

std::vector<std::uint8_t> Message::encode() const {
    std::vector<std::uint8_t> body;
    body.reserve(128 + payload.size());

    body.push_back(static_cast<std::uint8_t>(type));

    if (file_id.size() > static_cast<std::size_t>(std::numeric_limits<std::uint16_t>::max())) {
        throw std::length_error("file_id too long");
    }
    write_u16(body, static_cast<std::uint16_t>(file_id.size()));
    body.insert(body.end(), file_id.begin(), file_id.end());

    switch (type) {
        case FrameType::FileStart: {
            if (target_path.size() > static_cast<std::size_t>(std::numeric_limits<std::uint16_t>::max())) {
                throw std::length_error("target_path too long");
            }
            write_u16(body, static_cast<std::uint16_t>(target_path.size()));
            body.insert(body.end(), target_path.begin(), target_path.end());
            write_u64(body, total_size);
            write_u32(body, total_chunks);
            write_u32(body, final_checksum);
            break;
        }
        case FrameType::FileChunk: {
            write_u32(body, sequence_number);
            write_u32(body, checksum);
            write_u32(body, static_cast<std::uint32_t>(payload.size()));
            body.insert(body.end(), payload.begin(), payload.end());
            break;
        }
        case FrameType::FileEnd: {
            write_u32(body, final_checksum);
            break;
        }
        case FrameType::FileCancel: {
            if (cancel_reason.size() > static_cast<std::size_t>(std::numeric_limits<std::uint16_t>::max())) {
                throw std::length_error("cancel_reason too long");
            }
            write_u16(body, static_cast<std::uint16_t>(cancel_reason.size()));
            body.insert(body.end(), cancel_reason.begin(), cancel_reason.end());
            break;
        }
        default:
            throw std::runtime_error("Unsupported frame type for encoding");
    }

    std::vector<std::uint8_t> result;
    result.reserve(body.size() + sizeof(std::uint32_t));
    write_u32(result, static_cast<std::uint32_t>(body.size()));
    result.insert(result.end(), body.begin(), body.end());
    return result;
}

Message::DecodeResult Message::try_decode(const std::vector<std::uint8_t>& buffer, Message& message) {
    if (buffer.size() < sizeof(std::uint32_t)) {
        return {DecodeResult::Incomplete, 0};
    }

    return decode_internal(buffer, message);
}

Message::DecodeResult Message::decode_internal(const std::vector<std::uint8_t>& buffer, Message& message) {
    if (buffer.size() < sizeof(std::uint32_t)) {
        return {DecodeResult::Incomplete, 0};
    }

    const auto total_length = read_u32(buffer.data());
    const std::size_t frame_total = static_cast<std::size_t>(total_length) + sizeof(std::uint32_t);

    if (buffer.size() < frame_total) {
        return {DecodeResult::Incomplete, 0};
    }

    if (total_length == 0) {
        return {DecodeResult::Error, sizeof(std::uint32_t)};
    }

    std::size_t offset = sizeof(std::uint32_t);
    if (offset >= buffer.size()) {
        return {DecodeResult::Error, buffer.size()};
    }

    message = {};
    message.type = static_cast<FrameType>(buffer[offset]);
    offset += sizeof(std::uint8_t);

    if (offset + sizeof(std::uint16_t) > buffer.size()) {
        return {DecodeResult::Error, frame_total};
    }

    const auto file_id_len = read_u16(buffer.data() + offset);
    offset += sizeof(std::uint16_t);

    if (offset + file_id_len > buffer.size()) {
        return {DecodeResult::Error, frame_total};
    }
    message.file_id.assign(reinterpret_cast<const char*>(buffer.data() + offset), file_id_len);
    offset += file_id_len;

    switch (message.type) {
        case FrameType::FileStart: {
            if (offset + sizeof(std::uint16_t) > buffer.size()) {
                return {DecodeResult::Error, frame_total};
            }
            const auto path_len = read_u16(buffer.data() + offset);
            offset += sizeof(std::uint16_t);
            if (offset + path_len > buffer.size()) {
                return {DecodeResult::Error, frame_total};
            }
            message.target_path.assign(reinterpret_cast<const char*>(buffer.data() + offset), path_len);
            offset += path_len;

            if (offset + sizeof(std::uint64_t) + sizeof(std::uint32_t) * 2 > buffer.size()) {
                return {DecodeResult::Error, frame_total};
            }
            message.total_size = read_u64(buffer.data() + offset);
            offset += sizeof(std::uint64_t);
            message.total_chunks = read_u32(buffer.data() + offset);
            offset += sizeof(std::uint32_t);
            message.final_checksum = read_u32(buffer.data() + offset);
            offset += sizeof(std::uint32_t);
            break;
        }
        case FrameType::FileChunk: {
            if (offset + sizeof(std::uint32_t) * 3 > buffer.size()) {
                return {DecodeResult::Error, frame_total};
            }
            message.sequence_number = read_u32(buffer.data() + offset);
            offset += sizeof(std::uint32_t);
            message.checksum = read_u32(buffer.data() + offset);
            offset += sizeof(std::uint32_t);
            const auto payload_len = read_u32(buffer.data() + offset);
            offset += sizeof(std::uint32_t);
            if (offset + payload_len > buffer.size()) {
                return {DecodeResult::Error, frame_total};
            }
            message.payload.assign(buffer.begin() + offset, buffer.begin() + offset + payload_len);
            offset += payload_len;
            break;
        }
        case FrameType::FileEnd: {
            if (offset + sizeof(std::uint32_t) > buffer.size()) {
                return {DecodeResult::Error, frame_total};
            }
            message.final_checksum = read_u32(buffer.data() + offset);
            offset += sizeof(std::uint32_t);
            break;
        }
        case FrameType::FileCancel: {
            if (offset + sizeof(std::uint16_t) > buffer.size()) {
                return {DecodeResult::Error, frame_total};
            }
            const auto reason_len = read_u16(buffer.data() + offset);
            offset += sizeof(std::uint16_t);
            if (offset + reason_len > buffer.size()) {
                return {DecodeResult::Error, frame_total};
            }
            message.cancel_reason.assign(reinterpret_cast<const char*>(buffer.data() + offset), reason_len);
            offset += reason_len;
            break;
        }
        default:
            return {DecodeResult::Error, frame_total};
    }

    return {DecodeResult::Success, frame_total};
}
