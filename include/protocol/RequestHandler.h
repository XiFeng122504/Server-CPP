#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <unordered_map>

#include "protocol/Message.h"

class Connection;

namespace protocol {

struct MessageTypeHash {
    std::size_t operator()(MessageType type) const noexcept {
        return static_cast<std::size_t>(type);
    }
};

class IRequestHandler {
public:
    virtual ~IRequestHandler() = default;
    virtual std::optional<Message> handle(const Message& request, Connection& connection) = 0;
};

class TableRequestHandler;

class RequestHandlerFactory {
public:
    using HandlerFunc = std::function<std::optional<Message>(const Message&, Connection&)>;

    RequestHandlerFactory();

    void register_handler(MessageType type, HandlerFunc func);

    std::unique_ptr<IRequestHandler> create() const;

    bool empty() const noexcept;

private:
    std::shared_ptr<std::unordered_map<MessageType, HandlerFunc, MessageTypeHash>> m_table;
};

class TableRequestHandler : public IRequestHandler {
public:
    explicit TableRequestHandler(std::shared_ptr<const std::unordered_map<MessageType, RequestHandlerFactory::HandlerFunc, MessageTypeHash>> table);
    std::optional<Message> handle(const Message& request, Connection& connection) override;

private:
    std::shared_ptr<const std::unordered_map<MessageType, RequestHandlerFactory::HandlerFunc, MessageTypeHash>> m_table;
};

} // namespace protocol

inline protocol::RequestHandlerFactory::RequestHandlerFactory()
    : m_table(std::make_shared<std::unordered_map<protocol::MessageType, HandlerFunc, protocol::MessageTypeHash>>()) {}

inline void protocol::RequestHandlerFactory::register_handler(protocol::MessageType type, HandlerFunc func) {
    (*m_table)[type] = std::move(func);
}

inline std::unique_ptr<protocol::IRequestHandler> protocol::RequestHandlerFactory::create() const {
    return std::make_unique<protocol::TableRequestHandler>(m_table);
}

inline bool protocol::RequestHandlerFactory::empty() const noexcept {
    return m_table->empty();
}

inline protocol::TableRequestHandler::TableRequestHandler(std::shared_ptr<const std::unordered_map<protocol::MessageType, protocol::RequestHandlerFactory::HandlerFunc, protocol::MessageTypeHash>> table)
    : m_table(std::move(table)) {}

inline std::optional<protocol::Message> protocol::TableRequestHandler::handle(const protocol::Message& request, Connection& connection) {
    auto it = m_table->find(request.type);
    if (it == m_table->end()) {
        return protocol::make_error(request.sequence, std::string("unknown command: ") + protocol::to_string(request.type));
    }
    return it->second(request, connection);
}
