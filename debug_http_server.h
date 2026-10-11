/* 调试 HTTP 服务：将手位置和识别结果绘制的图片通过 HTTP GET 提供给调试端 */
#ifndef DEBUG_HTTP_SERVER_H
#define DEBUG_HTTP_SERVER_H

#include <string>
#include <atomic>
#include <mutex>
#include <thread>
#include <opencv2/opencv.hpp>

class DebugHttpServer {
public:
    DebugHttpServer(int port = 8080);
    ~DebugHttpServer();

    void start();
    void stop();

    // 更新待分发的调试图片（线程安全）
    void update_frame(const cv::Mat& frame);

private:
    void run();
    void handle_client(int client_fd);

    int port_;
    int server_fd_;
    std::atomic<bool> running_;
    cv::Mat frame_;
    std::mutex frame_mutex_;
};

#endif // DEBUG_HTTP_SERVER_H
