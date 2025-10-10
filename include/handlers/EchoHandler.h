#ifndef SERVER_CPP_HANDLERS_ECHO_HANDLER_H
#define SERVER_CPP_HANDLERS_ECHO_HANDLER_H

#include "protocol/RequestDispatcher.h"

class EchoHandler : public protocol::IRequestHandler {
public:
    void handle(const protocol::Message& message, Connection& connection) override;
};

#endif // SERVER_CPP_HANDLERS_ECHO_HANDLER_H
