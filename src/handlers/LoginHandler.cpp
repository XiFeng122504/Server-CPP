#include "handlers/LoginHandler.h"

#include "Connection.h"
#include "protocol/Message.h"

LoginHandler::LoginHandler() {
    // 简单的内存凭证表，实际项目可替换为数据库/配置文件
    m_credentials.emplace("alice", "password123");
    m_credentials.emplace("bob", "hunter2");
}

void LoginHandler::handle(const protocol::Message& message, Connection& connection) {
    std::string credentials(message.payload.begin(), message.payload.end());
    auto separator = credentials.find(':');
    if (separator == std::string::npos) {
        protocol::Message error{protocol::MessageType::Error};
        std::string text = "invalid login payload";
        error.payload.assign(text.begin(), text.end());
        connection.send_message(error);
        return;
    }

    std::string username = credentials.substr(0, separator);
    std::string password = credentials.substr(separator + 1);

    auto it = m_credentials.find(username);
    if (it == m_credentials.end() || it->second != password) {
        protocol::Message error{protocol::MessageType::Error};
        std::string text = "authentication failed";
        error.payload.assign(text.begin(), text.end());
        connection.send_message(error);
        return;
    }

    connection.set_session(username);

    protocol::Message ack{protocol::MessageType::Ack};
    std::string text = "login success";
    ack.payload.assign(text.begin(), text.end());
    connection.send_message(ack);
}
