#include "handlers/EchoHandler.h"

#include "Connection.h"
#include "protocol/Message.h"

void EchoHandler::handle(const protocol::Message& message, Connection& connection) {
    protocol::Message response{protocol::MessageType::EchoRequest};
    response.payload = message.payload;
    connection.send_message(response);
}
