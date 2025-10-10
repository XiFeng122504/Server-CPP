#include "handlers/EchoHandler.h"

#include "Connection.h"

bool EchoHandler::handle(Connection& connection, const std::string& payload) {
    if (!connection.session().has_value()) {
        connection.send_frame("ERROR", "NOT_LOGGED_IN");
        return false;
    }

    connection.send_frame("ECHO_OK", payload);
    return true;
}
