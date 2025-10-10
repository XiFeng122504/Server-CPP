#ifndef SERVER_CPP_HANDLERS_FILEUPLOADHANDLER_H
#define SERVER_CPP_HANDLERS_FILEUPLOADHANDLER_H

#include <string>

class Connection;

class FileUploadHandler {
public:
    bool handle(Connection& connection, const std::string& payload);
};

#endif // SERVER_CPP_HANDLERS_FILEUPLOADHANDLER_H
