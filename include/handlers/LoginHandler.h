#ifndef SERVER_CPP_HANDLERS_LOGIN_HANDLER_H
#define SERVER_CPP_HANDLERS_LOGIN_HANDLER_H

#include <unordered_map>
#include <string>

#include "protocol/RequestDispatcher.h"

class LoginHandler : public protocol::IRequestHandler {
public:
    LoginHandler();
    void handle(const protocol::Message& message, Connection& connection) override;
private:
    std::unordered_map<std::string, std::string> m_credentials;
};

#endif // SERVER_CPP_HANDLERS_LOGIN_HANDLER_H
