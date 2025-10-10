#include <cassert>
#include <cerrno>
#include <string>
#include <unistd.h>

#include "Connection.h"

namespace {
std::string read_frame(int fd) {
    std::string result;
    char ch = 0;
    while (true) {
        ssize_t bytes = ::read(fd, &ch, 1);
        if (bytes == 1) {
            if (ch == '\n') {
                break;
            }
            result.push_back(ch);
        } else if (bytes == 0) {
            break;
        } else if (errno == EINTR) {
            continue;
        } else {
            break;
        }
    }
    return result;
}

struct PipePair {
    int read_fd;
    int write_fd;
};

PipePair make_pipe() {
    int fds[2];
    int ret = ::pipe(fds);
    assert(ret == 0);
    return {fds[0], fds[1]};
}

void close_pipe(const PipePair& pipe) {
    ::close(pipe.read_fd);
    ::close(pipe.write_fd);
}

void test_successful_login_and_echo() {
    auto pipe = make_pipe();
    {
        Connection connection(pipe.write_fd, nullptr);
        connection.process_message("LOGIN alice password123");
        std::string login_response = read_frame(pipe.read_fd);
        assert(login_response.rfind("LOGIN_OK", 0) == 0);
        assert(connection.session().has_value());
        assert(connection.session()->username == "alice");

        connection.process_message("ECHO hello world");
        std::string echo_response = read_frame(pipe.read_fd);
        assert(echo_response == "ECHO_OK hello world");
    }
    close_pipe(pipe);
}

void test_repeated_login_rejected() {
    auto pipe = make_pipe();
    {
        Connection connection(pipe.write_fd, nullptr);
        connection.process_message("LOGIN alice password123");
        std::string login_response = read_frame(pipe.read_fd);
        assert(login_response.rfind("LOGIN_OK", 0) == 0);
        assert(connection.session().has_value());

        connection.process_message("LOGIN alice password123");
        std::string error_response = read_frame(pipe.read_fd);
        assert(error_response == "ERROR ALREADY_LOGGED_IN");
    }
    close_pipe(pipe);
}

void test_invalid_credentials() {
    auto pipe = make_pipe();
    {
        Connection connection(pipe.write_fd, nullptr);
        connection.process_message("LOGIN alice wrongpass");
        std::string error_response = read_frame(pipe.read_fd);
        assert(error_response == "ERROR INVALID_CREDENTIALS");
        assert(!connection.session().has_value());
        assert(!connection.is_close());
    }
    close_pipe(pipe);
}

void test_logout_and_access_control() {
    auto pipe = make_pipe();
    {
        Connection connection(pipe.write_fd, nullptr);
        connection.process_message("LOGIN alice password123");
        std::string login_response = read_frame(pipe.read_fd);
        assert(login_response.rfind("LOGIN_OK", 0) == 0);
        assert(connection.session().has_value());

        connection.process_message("LOGOUT");
        std::string logout_response = read_frame(pipe.read_fd);
        assert(logout_response == "LOGOUT_OK BYE");
        assert(!connection.session().has_value());

        connection.process_message("ECHO hi");
        std::string echo_error = read_frame(pipe.read_fd);
        assert(echo_error == "ERROR NOT_LOGGED_IN");
    }
    close_pipe(pipe);
}

} // namespace

int main() {
    test_successful_login_and_echo();
    test_repeated_login_rejected();
    test_invalid_credentials();
    test_logout_and_access_control();
    return 0;
}
