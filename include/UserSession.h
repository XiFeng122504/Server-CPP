#ifndef SERVER_CPP_USERSESSION_H
#define SERVER_CPP_USERSESSION_H

#include <chrono>
#include <string>
#include <vector>

struct UserSession {
    std::string username;
    std::chrono::system_clock::time_point authenticated_at;
    std::vector<std::string> permissions;
};

#endif // SERVER_CPP_USERSESSION_H
