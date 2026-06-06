/* Copyright (c) 2024
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
#include "gesture_io_control.h"
#include <iostream>
#include <chrono>

using namespace std;

GestureIOControl::GestureIOControl(const char *kmodel_det, float obj_thresh, float nms_thresh,
                                   const char *kmodel_kp, int debug_mode)
    : gpio_led1_(nullptr), gpio_led2_(nullptr), gpio_led3_(nullptr), gpio_relay_(nullptr),
      last_gesture_(GESTURE_NONE), debug_mode_(debug_mode) {
    FrameCHWSize image_size = {AI_FRAME_CHANNEL, AI_FRAME_HEIGHT, AI_FRAME_WIDTH};
    hand_detection_ = make_unique<HandDetection>((char*)kmodel_det, obj_thresh, nms_thresh, image_size, debug_mode);
    hand_keypoint_ = make_unique<HandKeypoint>((char*)kmodel_kp, image_size, debug_mode);
}

GestureIOControl::~GestureIOControl() {
    DeinitGPIO();
}

bool GestureIOControl::InitGPIO() {
    int ret;
    
    ret = drv_fpioa_set_pin_func(GPIO_PIN_LED1, static_cast<fpioa_func_t>(GPIO0 + GPIO_PIN_LED1));
    if (ret != 0) return false;
    ret = drv_gpio_inst_create(GPIO_PIN_LED1, &gpio_led1_);
    if (ret != 0) return false;
    ret = drv_gpio_mode_set(gpio_led1_, GPIO_DM_OUTPUT);
    if (ret != 0) return false;
    drv_gpio_value_set(gpio_led1_, GPIO_PV_LOW);
    
    ret = drv_fpioa_set_pin_func(GPIO_PIN_LED2, static_cast<fpioa_func_t>(GPIO0 + GPIO_PIN_LED2));
    if (ret != 0) return false;
    ret = drv_gpio_inst_create(GPIO_PIN_LED2, &gpio_led2_);
    if (ret != 0) return false;
    ret = drv_gpio_mode_set(gpio_led2_, GPIO_DM_OUTPUT);
    if (ret != 0) return false;
    drv_gpio_value_set(gpio_led2_, GPIO_PV_LOW);
    
    ret = drv_fpioa_set_pin_func(GPIO_PIN_LED3, static_cast<fpioa_func_t>(GPIO0 + GPIO_PIN_LED3));
    if (ret != 0) return false;
    ret = drv_gpio_inst_create(GPIO_PIN_LED3, &gpio_led3_);
    if (ret != 0) return false;
    ret = drv_gpio_mode_set(gpio_led3_, GPIO_DM_OUTPUT);
    if (ret != 0) return false;
    drv_gpio_value_set(gpio_led3_, GPIO_PV_LOW);
    
    ret = drv_fpioa_set_pin_func(GPIO_PIN_RELAY, static_cast<fpioa_func_t>(GPIO0 + GPIO_PIN_RELAY));
    if (ret != 0) return false;
    ret = drv_gpio_inst_create(GPIO_PIN_RELAY, &gpio_relay_);
    if (ret != 0) return false;
    ret = drv_gpio_mode_set(gpio_relay_, GPIO_DM_OUTPUT);
    if (ret != 0) return false;
    drv_gpio_value_set(gpio_relay_, GPIO_PV_LOW);
    
    cout << "GPIO initialized successfully" << endl;
    return true;
}

void GestureIOControl::DeinitGPIO() {
    if (gpio_led1_) {
        drv_gpio_value_set(gpio_led1_, GPIO_PV_LOW);
        drv_gpio_inst_destroy(&gpio_led1_);
    }
    if (gpio_led2_) {
        drv_gpio_value_set(gpio_led2_, GPIO_PV_LOW);
        drv_gpio_inst_destroy(&gpio_led2_);
    }
    if (gpio_led3_) {
        drv_gpio_value_set(gpio_led3_, GPIO_PV_LOW);
        drv_gpio_inst_destroy(&gpio_led3_);
    }
    if (gpio_relay_) {
        drv_gpio_value_set(gpio_relay_, GPIO_PV_LOW);
        drv_gpio_inst_destroy(&gpio_relay_);
    }
}

void GestureIOControl::pre_process(runtime_tensor &input_tensor) {
    hand_detection_->pre_process(input_tensor);
}

void GestureIOControl::inference() {
    hand_detection_->inference();
}

GestureResult GestureIOControl::post_process(FrameCHWSize image_size) {
    GestureResult result;
    result.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    result.type = GESTURE_NONE;
    result.confidence = 0.0f;
    result.gesture_name = "none";
    
    vector<BoxInfo> detection_results;
    hand_detection_->post_process(image_size, detection_results);
    
    if (detection_results.empty()) {
        if (debug_mode_ > 0) {
            cout << "No hand detected" << endl;
        }
        return result;
    }
    
    for (auto r : detection_results) {
        int w = r.x2 - r.x1 + 1;
        int h = r.y2 - r.y1 + 1;
        int length = std::max(w, h) / 2;
        int cx = (r.x1 + r.x2) / 2;
        int cy = (r.y1 + r.y2) / 2;
        int ratio_num = 1.26 * length;
        
        int x1_1 = std::max(0, cx - ratio_num);
        int y1_1 = std::max(0, cy - ratio_num);
        int x2_1 = std::min(AI_FRAME_WIDTH - 1, cx + ratio_num);
        int y2_1 = std::min(AI_FRAME_HEIGHT - 1, cy + ratio_num);
        int w_1 = x2_1 - x1_1 + 1;
        int h_1 = y2_1 - y1_1 + 1;
        
        struct Bbox bbox = {x: x1_1, y: y1_1, w: w_1, h: h_1};
        
        runtime_tensor dummy_tensor;
        hand_keypoint_->pre_process(dummy_tensor, bbox);
        hand_keypoint_->inference();
        hand_keypoint_->post_process(bbox);
        
        std::vector<double> angle_list = hand_keypoint_->hand_angle();
        std::string gesture = hand_keypoint_->h_gesture(angle_list);
        
        result.gesture_name = gesture;
        result.type = StringToGestureType(gesture);
        result.confidence = r.score;
        
        if (debug_mode_ > 0) {
            cout << "Hand detected at [" << r.x1 << "," << r.y1 << "," << r.x2 << "," << r.y2 << "] "
                 << "Gesture: " << gesture << " (score: " << r.score << ")" << endl;
        }
        
        break;
    }
    
    return result;
}

void GestureIOControl::UpdateIO(GestureResult result) {
    if (result.type == GESTURE_NONE) {
        ResetIO();
        last_gesture_ = GESTURE_NONE;
        return;
    }
    
    if (result.type == last_gesture_) {
        return;
    }
    
    ResetIO();
    last_gesture_ = result.type;
    
    switch (result.type) {
        case GESTURE_FIST:
            drv_gpio_value_set(gpio_led1_, GPIO_PV_HIGH);
            cout << "Gesture: FIST -> LED1 ON" << endl;
            break;
        case GESTURE_PALM:
            drv_gpio_value_set(gpio_led2_, GPIO_PV_HIGH);
            cout << "Gesture: PALM -> LED2 ON" << endl;
            break;
        case GESTURE_FIVE:
            drv_gpio_value_set(gpio_led3_, GPIO_PV_HIGH);
            cout << "Gesture: FIVE -> LED3 ON" << endl;
            break;
        case GESTURE_YEAH:
            drv_gpio_value_set(gpio_led1_, GPIO_PV_HIGH);
            drv_gpio_value_set(gpio_led2_, GPIO_PV_HIGH);
            cout << "Gesture: YEAH -> LED1+LED2 ON" << endl;
            break;
        case GESTURE_SWIPE_LEFT:
            drv_gpio_value_set(gpio_relay_, GPIO_PV_HIGH);
            cout << "Gesture: SWIPE_LEFT -> RELAY ON" << endl;
            break;
        case GESTURE_SWIPE_RIGHT:
            drv_gpio_value_set(gpio_relay_, GPIO_PV_LOW);
            cout << "Gesture: SWIPE_RIGHT -> RELAY OFF" << endl;
            break;
        case GESTURE_NONE:
        default:
            ResetIO();
            break;
    }
}

void GestureIOControl::ResetIO() {
    if (gpio_led1_) drv_gpio_value_set(gpio_led1_, GPIO_PV_LOW);
    if (gpio_led2_) drv_gpio_value_set(gpio_led2_, GPIO_PV_LOW);
    if (gpio_led3_) drv_gpio_value_set(gpio_led3_, GPIO_PV_LOW);
}

const char* GestureTypeToString(GestureType type) {
    switch (type) {
        case GESTURE_NONE: return "none";
        case GESTURE_FIST: return "fist";
        case GESTURE_PALM: return "palm";
        case GESTURE_FIVE: return "five";
        case GESTURE_YEAH: return "yeah";
        case GESTURE_SWIPE_LEFT: return "swipe_left";
        case GESTURE_SWIPE_RIGHT: return "swipe_right";
        default: return "unknown";
    }
}

GestureType StringToGestureType(const std::string& gesture_name) {
    if (gesture_name == "fist") return GESTURE_FIST;
    if (gesture_name == "palm") return GESTURE_PALM;
    if (gesture_name == "five") return GESTURE_FIVE;
    if (gesture_name == "yeah") return GESTURE_YEAH;
    return GESTURE_NONE;
}