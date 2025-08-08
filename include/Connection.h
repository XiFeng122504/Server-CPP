#ifndef _CONNECTION_H
#define _CONNECTION_H
#include <string>

class Connection {
public:
    Connection();
    ~Connection();

    bool handle_write();
    bool handle_read();
    bool handle_err();
    bool is_close();
    int get_fd();

private:
    int client_fd;
    std::string read_buffer;
    std::string write_buffer;
    bool closed;
};

#endif//_CONNECTION_H
