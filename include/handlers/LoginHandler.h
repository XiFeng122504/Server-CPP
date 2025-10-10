#ifndef SERVER_CPP_HANDLERS_LOGINHANDLER_H
#define SERVER_CPP_HANDLERS_LOGINHANDLER_H

#include <string>
#include <unordered_map>
#include <vector>

class Connection;

class LoginHandler {
public:
    struct Credential {
        std::string password;
        std::vector<std::string> permissions;
    };

    LoginHandler();
    explicit LoginHandler(std::unordered_map<std::string, Credential> credentials);

    bool handle(Connection& connection, const std::string& payload);

private:
    std::unordered_map<std::string, Credential> m_credentials;
};

#endif // SERVER_CPP_HANDLERS_LOGINHANDLER_H
