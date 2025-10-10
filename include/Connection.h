#ifndef _CONNECTION_H
#define _CONNECTION_H
#define BUFFER_SIZE 4096
#include <memory>
#include <string>
#include <unistd.h>
#include <iostream>
#include <errno.h>  // 或 <cerrno>
#include <sys/socket.h>
#include <vector>

#include "Message.h"

class FileUploadService;

class Epoll;

class Connection {
public:
    Connection(int client_fd, Epoll* epoll = nullptr, FileUploadService* upload_service = nullptr);
    ~Connection();

    bool handle_write();
    bool handle_read();
    bool handle_err();
    bool is_close();
    bool write_complete();

    int get_fd();

    void process_request();
    void set_upload_service(FileUploadService* service);
private:
    Epoll* m_pEpoll;
    int m_nClient_fd;
    std::vector<uint8_t> m_sRead_buffer;
    std::vector<uint8_t> m_sWrite_buffer;
    bool m_bClosed;
    FileUploadService* m_upload_service;
};

#endif//_CONNECTION_H
