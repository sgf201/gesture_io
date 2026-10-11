#include "udp_command.h"
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

// 串口通信协议（与 main_linux.cc 中一致）
#define PROTOCOL_START "@#"
#define PROTOCOL_END "#@"

// 由 main_linux.cc 提供的串口发送函数
extern void send_command(const std::string& cmd);

UdpCommandServer::UdpCommandServer(int port)
    : port_(port), sockfd_(-1), running_(false) {}

UdpCommandServer::~UdpCommandServer() {
    stop();
}

void UdpCommandServer::start() {
    running_ = true;
    std::thread(&UdpCommandServer::run, this).detach();
}

void UdpCommandServer::stop() {
    running_ = false;
    if (sockfd_ >= 0) {
        shutdown(sockfd_, SHUT_RDWR);
        close(sockfd_);
        sockfd_ = -1;
    }
}

void UdpCommandServer::run() {
    sockfd_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd_ < 0) {
        std::cerr << "[UDP] socket 创建失败" << std::endl;
        return;
    }

    int opt = 1;
    setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port_);

    if (bind(sockfd_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "[UDP] 绑定端口 " << port_ << " 失败" << std::endl;
        close(sockfd_);
        sockfd_ = -1;
        return;
    }

    std::cout << "[UDP] 命令监听已启动，端口 " << port_ << std::endl;

    char buffer[1024];
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    while (running_) {
        ssize_t n = recvfrom(sockfd_, buffer, sizeof(buffer) - 1, 0,
                             (struct sockaddr*)&client_addr, &client_len);
        if (n > 0) {
            buffer[n] = '\0';
            std::string cmd(buffer);
            // 去除尾部空白字符
            while (!cmd.empty() &&
                   (cmd.back() == '\n' || cmd.back() == '\r' ||
                    cmd.back() == ' ' || cmd.back() == '\t')) {
                cmd.pop_back();
            }
            if (!cmd.empty()) {
                std::cout << "[UDP] 收到命令: " << cmd
                          << " 来自 " << inet_ntoa(client_addr.sin_addr)
                          << ":" << ntohs(client_addr.sin_port) << std::endl;
                send_command(cmd);
            }
        } else if (n < 0 && running_) {
            // 非致命错误，继续
            continue;
        }
    }

    if (sockfd_ >= 0) {
        close(sockfd_);
        sockfd_ = -1;
    }
    std::cout << "[UDP] 命令监听已停止" << std::endl;
}
