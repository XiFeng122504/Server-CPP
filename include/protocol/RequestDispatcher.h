#ifndef SERVER_CPP_PROTOCOL_REQUEST_DISPATCHER_H
#define SERVER_CPP_PROTOCOL_REQUEST_DISPATCHER_H

#include <memory>
#include <unordered_map>

#include "protocol/Message.h"

class Connection;

namespace protocol {

class IRequestHandler {
public:
    virtual ~IRequestHandler() = default;
    virtual void handle(const Message& message, Connection& connection) = 0;
    virtual void handle_connection_closed(Connection& connection) {}
};

class RequestDispatcher {
public:
    void register_handler(MessageType type, std::shared_ptr<IRequestHandler> handler);
    void dispatch(const Message& message, Connection& connection);
    void notify_connection_closed(Connection& connection);
private:
    std::unordered_map<MessageType, std::shared_ptr<IRequestHandler>> m_handlers;
};

}

#endif // SERVER_CPP_PROTOCOL_REQUEST_DISPATCHER_H
