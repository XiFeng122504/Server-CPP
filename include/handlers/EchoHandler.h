#ifndef SERVER_CPP_HANDLERS_ECHOHANDLER_H
#define SERVER_CPP_HANDLERS_ECHOHANDLER_H

#include <string>

class Connection;

class EchoHandler {
public:
    bool handle(Connection& connection, const std::string& payload);
};

#endif // SERVER_CPP_HANDLERS_ECHOHANDLER_H
