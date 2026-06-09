/* Copyright (c) 2025, Canaan Bright Sight Co., Ltd
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 * CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <iostream>
#include <memory>
#include <unistd.h>
#include <thread>
#include <atomic>
#include <chrono>
#include <vector>

#include "pangofly/pangofly.h"
#include "gesture_message.h"
#include "setting.h"
#include "sensor_buf_manager.h"

using std::cerr;
using std::cout;
using std::endl;
using std::thread;

class GesturePublisher {
public:
    GesturePublisher() : running_(false), writer_(nullptr) {}
    
    ~GesturePublisher() {
        Stop();
    }
    
    int Init(const std::string& channel_name = "gesture_image_channel") {
        node_ = pangofly::CreateNode("gesture_publisher");
        if (!node_) {
            std::cerr << "Failed to create node" << std::endl;
            return -1;
        }
        
        writer_ = node_->CreateWriter<ImageMessage>(channel_name);
        if (!writer_) {
            std::cerr << "Failed to create writer" << std::endl;
            return -1;
        }
        
        std::cout << "Gesture Publisher initialized" << std::endl;
        return 0;
    }
    
    int Start() {
        if (running_) return 0;
        
        running_ = true;
        thread_ = std::thread(&GesturePublisher::RunLoop, this);
        
        std::cout << "Gesture Publisher started, capturing camera..." << std::endl;
        return 0;
    }
    
    int Stop() {
        if (!running_) return 0;
        
        running_ = false;
        if (thread_.joinable()) {
            thread_.join();
        }
        
        writer_.reset();
        node_.reset();
        
        std::cout << "Gesture Publisher stopped" << std::endl;
        return 0;
    }

private:
    void RunLoop() {
        struct v4l2_drm_context context;
        #define BUFFER_NUM 3

        v4l2_drm_default_context(&context);
        context.device = kd_mpi_get_vvcam_video00() + 1;
        context.display = false;
        context.width = SENSOR_WIDTH;
        context.height = SENSOR_HEIGHT;
        context.video_format = v4l2_fourcc('B', 'G', '3', 'P');
        context.buffer_num = BUFFER_NUM;
        
        if (v4l2_drm_setup(&context, 1, NULL)) {
            cerr << "v4l2_drm_setup error" << endl;
            return;
        }
        if (v4l2_drm_start(&context)) {
            cerr << "v4l2_drm_start error" << endl;
            return;
        }

        std::vector<std::tuple<int, void*>> tensors;
        for (unsigned i = 0; i < BUFFER_NUM; i++) {
            tensors.push_back({context.buffers[i].fd, context.buffers[i].mmap});
        }
        SensorBufManager sensor_buf = SensorBufManager({SENSOR_CHANNEL, SENSOR_HEIGHT, SENSOR_WIDTH}, tensors);

        while (running_) {
            int ret = v4l2_drm_dump(&context, 1000);
            if (ret) {
                perror("v4l2_drm_dump error");
                continue;
            }

            runtime_tensor& img_data = sensor_buf.get_buf_for_index(context.vbuffer.index);
            auto img_data_buf = img_data.impl()->to_host().unwrap()->buffer().as_host().unwrap().map(map_access_::map_read).unwrap().buffer();
            
            size_t data_size = SENSOR_WIDTH * SENSOR_HEIGHT * SENSOR_CHANNEL;
            
            ImageMessage msg;
            msg.timestamp = std::chrono::system_clock::now().time_since_epoch().count();
            msg.width = SENSOR_WIDTH;
            msg.height = SENSOR_HEIGHT;
            msg.format = 1;
            msg.data.resize(data_size);
            memcpy(msg.data.data(), img_data_buf.data(), data_size);
            
            bool success = writer_->Write(msg);
            std::cout << "Published image: width=" << msg.width << ", height=" << msg.height 
                      << ", size=" << msg.data.size() << ", success=" << success << std::endl;

            v4l2_drm_dump_release(&context);
        }

        v4l2_drm_stop(&context);
    }
    private:
    std::atomic<bool> running_;
    std::unique_ptr<pangofly::Node> node_;
    std::shared_ptr<pangofly::Writer<ImageMessage>> writer_;
    std::thread thread_;
};

int main() {
    GesturePublisher publisher;
    
    if (publisher.Init() != 0) {
        return -1;
    }
    
    if (publisher.Start() != 0) {
        return -1;
    }
    
    std::cout << "Press Enter to stop..." << std::endl;
    std::cin.get();
    
    publisher.Stop();
    return 0;
}