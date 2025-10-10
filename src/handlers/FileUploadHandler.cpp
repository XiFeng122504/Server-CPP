#include "handlers/FileUploadHandler.h"

#include <sstream>
#include <string>

#include "Connection.h"

bool FileUploadHandler::handle(Connection& connection, const std::string& payload) {
    if (!connection.session().has_value()) {
        connection.send_frame("ERROR", "NOT_LOGGED_IN");
        return false;
    }

    std::istringstream iss(payload);
    std::string filename;
    iss >> filename;

    std::string data;
    std::getline(iss, data);
    if (!data.empty() && data.front() == ' ') {
        data.erase(0, 1);
    }

    if (filename.empty()) {
        connection.send_frame("ERROR", "INVALID_UPLOAD");
        return false;
    }

    connection.send_frame("UPLOAD_OK", filename + " " + std::to_string(data.size()));
    return true;
}
