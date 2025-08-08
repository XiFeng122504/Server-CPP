#include <iostream>
#include <sys/socket.h>
#include <cstring>
#include <netinet/in.h>
#include <unistd.h>


const int PORT           = 8080;
const int MAXCONNECTIONS = 10;
const int BUFFERSIZE     = 1024;


int main(){
    //  创建套接字
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(server_fd < 0){
        std::cout<< "无法创建套接字" << std::endl;
        return 1;
    }

    //  配置服务器地址
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    //  bind操作，绑定套接字到端口
    if(bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) > 0 ){
        std::cerr << "绑定失败\n";
        close(server_fd);
        return 1;
    }

    //  开始监听
    if(listen(server_fd, MAXCONNECTIONS) < 0){
        std::cerr << "监听失败";
        return 1;
    }

    std::cout << "现在服务器正在监听" << PORT << "端口:\n";

    // 接受客户端请求 
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
    if(client_fd < 0){
        std::cerr << "接受连接失败了\n";
        close(client_fd);
        return 1;
    }
    
    std::cout << "客户端已经连接了" << std::endl;

    //  回声处理
    char buffer[BUFFERSIZE];
    while(true){
        memset(buffer,0, BUFFERSIZE);

        //  接收客户端数据
        ssize_t btyes_received = recv(client_fd, buffer, BUFFERSIZE, 0);
        if(btyes_received <= 0){
            std::cerr << "接受错误或连接关闭" << std::endl;
            break;
        }


        std::cout << "收到消息:" << buffer << std::endl;

        //  发送相同数据
        ssize_t bytes_sent = send(client_fd, buffer, btyes_received, 0);
        //  这里要注意，是输入了多少个，就通过btyes_received重新发送回去。
        
        if(bytes_sent < 0){
            std::cerr << "发送失败\n";
            break;
        }
        
        if(strncmp(buffer, "exit", 4) == 0){
            std::cout << "收到退出命令\n";
            break;
        }


    }
    
    close(client_fd);
    close(server_fd);
    std::cout << "服务器已关闭\n";

    return 0;
}
