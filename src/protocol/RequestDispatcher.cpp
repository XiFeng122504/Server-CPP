#include "protocol/RequestDispatcher.h"

#include <iostream>

#include "Connection.h"

namespace protocol {

void RequestDispatcher::register_handler(MessageType type, std::shared_ptr<IRequestHandler> handler) {
    m_handlers[type] = std::move(handler);
}

void RequestDispatcher::dispatch(const Message& message, Connection& connection) {
    auto it = m_handlers.find(message.type);
    if (it == m_handlers.end()) {
        std::cerr << "Unhandled message type: " << static_cast<int>(message.type) << std::endl;
        return;
    }
    it->second->handle(message, connection);
}

void RequestDispatcher::notify_connection_closed(Connection& connection) {
    for (auto& [type, handler] : m_handlers) {
        if (handler) {
            handler->handle_connection_closed(connection);
        }
    }
}

}
