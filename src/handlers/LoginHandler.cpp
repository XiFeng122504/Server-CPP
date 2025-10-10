#include "handlers/LoginHandler.h"

#include <chrono>
#include <sstream>
#include <unordered_map>
#include <utility>

#include "Connection.h"
#include "UserSession.h"

namespace {
std::unordered_map<std::string, LoginHandler::Credential> build_default_credentials() {
    return {
        {"alice", {"password123", {"ECHO", "UPLOAD"}}},
        {"bob", {"hunter2", {"ECHO"}}},
    };
}
} // namespace

LoginHandler::LoginHandler()
    : m_credentials(build_default_credentials()) {}

LoginHandler::LoginHandler(std::unordered_map<std::string, Credential> credentials)
    : m_credentials(std::move(credentials)) {}

bool LoginHandler::handle(Connection& connection, const std::string& payload) {
    if (connection.session().has_value()) {
        connection.send_frame("ERROR", "ALREADY_LOGGED_IN");
        return false;
    }

    std::istringstream iss(payload);
    std::string username;
    std::string password;
    iss >> username >> password;

    if (username.empty() || password.empty()) {
        connection.send_frame("ERROR", "INVALID_LOGIN_FORMAT");
        return false;
    }

    auto iter = m_credentials.find(username);
    if (iter == m_credentials.end() || iter->second.password != password) {
        connection.send_frame("ERROR", "INVALID_CREDENTIALS");
        return false;
    }

    UserSession session{username, std::chrono::system_clock::now(), iter->second.permissions};
    connection.set_session(session);
    connection.send_frame("LOGIN_OK", username);
    return true;
}
