#include "debug_http_server.h"
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <vector>

DebugHttpServer::DebugHttpServer(int port)
    : port_(port), server_fd_(-1), running_(false) {}

DebugHttpServer::~DebugHttpServer() {
    stop();
}

void DebugHttpServer::start() {
    running_ = true;
    std::thread(&DebugHttpServer::run, this).detach();
}

void DebugHttpServer::stop() {
    running_ = false;
    if (server_fd_ >= 0) {
        shutdown(server_fd_, SHUT_RDWR);
        close(server_fd_);
        server_fd_ = -1;
    }
}

void DebugHttpServer::update_frame(const cv::Mat& frame) {
    std::lock_guard<std::mutex> lock(frame_mutex_);
    frame.copyTo(frame_);
}

void DebugHttpServer::handle_client(int client_fd) {
    char buffer[2048];
    ssize_t n = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
    if (n <= 0) {
        close(client_fd);
        return;
    }
    buffer[n] = '\0';

    // 仅处理 GET 请求
    if (strncmp(buffer, "GET ", 4) != 0) {
        const char* resp = "HTTP/1.1 405 Method Not Allowed\r\nContent-Length: 0\r\n\r\n";
        send(client_fd, resp, strlen(resp), 0);
        close(client_fd);
        return;
    }

    std::lock_guard<std::mutex> lock(frame_mutex_);
    if (frame_.empty()) {
        const char* resp = "HTTP/1.1 503 Service Unavailable\r\n"
                           "Content-Type: text/plain\r\n"
                           "Content-Length: 28\r\n\r\n"
                           "No frame available yet.\r\n";
        send(client_fd, resp, strlen(resp), 0);
        close(client_fd);
        return;
    }

    // 编码为 JPEG
    std::vector<uchar> jpeg_buf;
    std::vector<int> params = {cv::IMWRITE_JPEG_QUALITY, 80};
    cv::imencode(".jpg", frame_, jpeg_buf, params);

    std::string header =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: image/jpeg\r\n"
        "Content-Length: " + std::to_string(jpeg_buf.size()) + "\r\n"
        "Cache-Control: no-store\r\n"
        "Connection: close\r\n\r\n";

    send(client_fd, header.c_str(), header.size(), 0);
    send(client_fd, jpeg_buf.data(), jpeg_buf.size(), 0);
    close(client_fd);
}

void DebugHttpServer::run() {
    server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd_ < 0) {
        std::cerr << "[HTTP] socket 创建失败" << std::endl;
        return;
    }

    int opt = 1;
    setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port_);

    if (bind(server_fd_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "[HTTP] 绑定端口 " << port_ << " 失败" << std::endl;
        close(server_fd_);
        server_fd_ = -1;
        return;
    }

    if (listen(server_fd_, 8) < 0) {
        std::cerr << "[HTTP] listen 失败" << std::endl;
        close(server_fd_);
        server_fd_ = -1;
        return;
    }

    std::cout << "[HTTP] 调试图片服务已启动，端口 " << port_
              << " (GET / 获取最新调试图片)" << std::endl;

    while (running_) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd_, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd >= 0) {
            handle_client(client_fd);
        }
    }

    if (server_fd_ >= 0) {
        close(server_fd_);
        server_fd_ = -1;
    }
    std::cout << "[HTTP] 调试图片服务已停止" << std::endl;
}
