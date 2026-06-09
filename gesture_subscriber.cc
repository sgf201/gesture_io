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
#include <string>
#include <mutex>

#include "pangofly/pangofly.h"
#include "gesture_message.h"
#include "hand_detection.h"
#include "hand_keypoint.h"
#include "setting.h"

using std::cerr;
using std::cout;
using std::endl;
using std::thread;

static std::mutex result_mutex;
static std::vector<BoxInfo> hand_results;
static std::vector<HandKeyPointInfo> hand_keypoint_results;

class GestureSubscriber {
public:
    GestureSubscriber() : running_(false), reader_(nullptr), result_writer_(nullptr), 
                          hd_(nullptr), hk_(nullptr) {}
    
    ~GestureSubscriber() {
        Stop();
    }
    
    int Init(const std::string& input_channel = "gesture_image_channel",
             const std::string& output_channel = "gesture_result_channel",
             const std::string& kmodel_det = "/opt/models/hand_det.kmodel",
             const std::string& kmodel_kp = "/opt/models/handkp_det.kmodel",
             float obj_thresh = 0.4, float nms_thresh = 0.4, int debug_mode = 1) {
        
        node_ = pangofly::CreateNode("gesture_subscriber");
        if (!node_) {
            std::cerr << "Failed to create node" << std::endl;
            return -1;
        }
        
        reader_ = node_->CreateReader<ImageMessage>(input_channel);
        if (!reader_) {
            std::cerr << "Failed to create reader" << std::endl;
            return -1;
        }
        
        result_writer_ = node_->CreateWriter<GestureResult>(output_channel);
        if (!result_writer_) {
            std::cerr << "Failed to create result writer" << std::endl;
            return -1;
        }

        try {
            hd_ = new HandDetection(kmodel_det.c_str(), obj_thresh, nms_thresh, 
                                   {SENSOR_WIDTH, SENSOR_HEIGHT}, 
                                   {SENSOR_CHANNEL, SENSOR_HEIGHT, SENSOR_WIDTH}, 
                                   debug_mode);
            hk_ = new HandKeypoint(kmodel_kp.c_str(), 
                                   {SENSOR_CHANNEL, SENSOR_HEIGHT, SENSOR_WIDTH}, 
                                   debug_mode);
            std::cout << "Loaded hand detection and keypoint models" << std::endl;
        } catch (...) {
            std::cerr << "Failed to load models" << std::endl;
            return -1;
        }
        
        std::cout << "Gesture Subscriber initialized" << std::endl;
        return 0;
    }
    
    int Start() {
        if (running_) return 0;
        
        running_ = true;
        thread_ = std::thread(&GestureSubscriber::RunLoop, this);
        
        std::cout << "Gesture Subscriber started" << std::endl;
        return 0;
    }
    
    int Stop() {
        if (!running_) return 0;
        
        running_ = false;
        if (thread_.joinable()) {
            thread_.join();
        }
        
        reader_.reset();
        result_writer_.reset();
        node_.reset();
        
        if (hd_) delete hd_;
        if (hk_) delete hk_;
        
        std::cout << "Gesture Subscriber stopped" << std::endl;
        return 0;
    }

private:
    void RunLoop() {
        while (running_) {
            ImageMessage msg;
            pangofly::MessageInfo msg_info;
            if (reader_->Read(&msg, &msg_info)) {
                std::cout << "Received image: width=" << msg.width 
                          << ", height=" << msg.height 
                          << ", size=" << msg.data.size() 
                          << ", timestamp=" << msg.timestamp << std::endl;

                cv::Mat ori_img(msg.height, msg.width, CV_8UC3, const_cast<uint8_t*>(msg.data.data()));
                
                result_mutex.lock();
                hand_results.clear();
                hand_keypoint_results.clear();
                
                hd_->pre_process(ori_img);
                hd_->inference();
                hd_->post_process(hand_results);
                
                std::string gesture_name = "none";
                float confidence = 0.0f;
                
                for (auto& r : hand_results) {
                    int w = r.x2 - r.x1 + 1;
                    int h = r.y2 - r.y1 + 1;
                    
                    int length = std::max(w, h) / 2;
                    int cx = (r.x1 + r.x2) / 2;
                    int cy = (r.y1 + r.y2) / 2;
                    int ratio_num = 1.26 * length;

                    int x1_1 = std::max(0, cx - ratio_num);
                    int y1_1 = std::max(0, cy - ratio_num);
                    int x2_1 = std::min(SENSOR_WIDTH - 1, cx + ratio_num);
                    int y2_1 = std::min(SENSOR_HEIGHT - 1, cy + ratio_num);
                    int w_1 = x2_1 - x1_1 + 1;
                    int h_1 = y2_1 - y1_1 + 1;
                    
                    struct Bbox bbox = {x: (float)x1_1, y: (float)y1_1, w: (float)w_1, h: (float)h_1};
                    hk_->pre_process(ori_img, bbox);
                    hk_->inference();
                    
                    HandKeyPointInfo post_process_result;
                    hk_->post_process(bbox, post_process_result);
                    hand_keypoint_results.push_back(post_process_result);
                    
                    auto angle_list = hand_angle(post_process_result.pred.data());
                    gesture_name = h_gesture(angle_list);
                    confidence = r.score;
                }
                result_mutex.unlock();

                int gesture_id = get_gesture_id(gesture_name);
                
                GestureResult result;
                result.timestamp = msg.timestamp;
                result.gesture_id = gesture_id;
                result.confidence = confidence;
                result.gesture_name.resize(gesture_name.size() + 1);
                memcpy(result.gesture_name.data(), gesture_name.c_str(), gesture_name.size() + 1);
                
                bool success = result_writer_->Write(result);
                std::cout << "Published gesture result: " << gesture_name 
                          << " (id: " << gesture_id << ", confidence: " << confidence << ")" << std::endl;
            }
            
            usleep(33000);
        }
    }
    
    int get_gesture_id(const std::string& name) {
        if (name == "palm") return 0;
        if (name == "fist") return 1;
        if (name == "five") return 2;
        if (name == "yeah") return 3;
        if (name == "ok") return 4;
        return -1;
    }
    private:
    std::atomic<bool> running_;
    std::unique_ptr<pangofly::Node> node_;
    std::shared_ptr<pangofly::Reader<ImageMessage>> reader_;
    std::shared_ptr<pangofly::Writer<GestureResult>> result_writer_;
    std::thread thread_;
    
    HandDetection* hd_;
    HandKeypoint* hk_;
};

int main(int argc, char* argv[]) {
    std::string kmodel_det = "/opt/models/hand_det.kmodel";
    std::string kmodel_kp = "/opt/models/handkp_det.kmodel";
    float obj_thresh = 0.4;
    float nms_thresh = 0.4;
    int debug_mode = 1;
    
    if (argc >= 2) kmodel_det = argv[1];
    if (argc >= 3) kmodel_kp = argv[2];
    if (argc >= 4) obj_thresh = atof(argv[3]);
    if (argc >= 5) nms_thresh = atof(argv[4]);
    if (argc >= 6) debug_mode = atoi(argv[5]);
    
    GestureSubscriber subscriber;
    
    if (subscriber.Init("gesture_image_channel", "gesture_result_channel",
                        kmodel_det, kmodel_kp, obj_thresh, nms_thresh, debug_mode) != 0) {
        return -1;
    }
    
    if (subscriber.Start() != 0) {
        return -1;
    }
    
    std::cout << "Press Enter to stop..." << std::endl;
    std::cin.get();
    
    subscriber.Stop();
    return 0;
}