/* UDP 命令转发服务：监听 UDP 端口，将收到的控制命令转发到串口 */
#ifndef UDP_COMMAND_H
#define UDP_COMMAND_H

#include <string>
#include <atomic>
#include <thread>

class UdpCommandServer {
public:
    UdpCommandServer(int port = 50000);
    ~UdpCommandServer();

    void start();
    void stop();

private:
    void run();

    int port_;
    int sockfd_;
    std::atomic<bool> running_;
};

#endif // UDP_COMMAND_H
