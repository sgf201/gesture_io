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

GestureIOControl::GestureIOControl(const char *kmodel_file, int debug_mode)
    : AIBase(kmodel_file, "GestureRecognition", debug_mode),
      gpio_led1_(nullptr), gpio_led2_(nullptr), gpio_led3_(nullptr), gpio_relay_(nullptr),
      last_gesture_(GESTURE_NONE) {
    input_size_ = {input_shapes_[0][1], input_shapes_[0][2], input_shapes_[0][3]};
    ai2d_out_tensor_ = get_input_tensor(0);
    FrameCHWSize image_size = {AI_FRAME_CHANNEL, AI_FRAME_HEIGHT, AI_FRAME_WIDTH};
    Utils::padding_resize_one_side_set(image_size, input_size_, ai2d_builder_, cv::Scalar(114, 114, 114));
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
    ScopedTiming st("Gesture pre_process", debug_mode_);
    ai2d_builder_->invoke(input_tensor, ai2d_out_tensor_).expect("ai2d invoke failed");
}

void GestureIOControl::inference() {
    run();
    get_output();
}

GestureResult GestureIOControl::post_process() {
    ScopedTiming st("Gesture post_process", debug_mode_);
    GestureResult result;
    result.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    if (p_outputs_.empty() || !p_outputs_[0]) {
        result.type = GESTURE_NONE;
        result.confidence = 0.0f;
        return result;
    }
    
    float *output = p_outputs_[0];
    int max_idx = 0;
    float max_val = output[0];
    
    for (int i = 1; i < GESTURE_MAX; i++) {
        if (output[i] > max_val) {
            max_val = output[i];
            max_idx = i;
        }
    }
    
    result.type = static_cast<GestureType>(max_idx);
    result.confidence = max_val;
    
    if (debug_mode_ > 0) {
        cout << "Gesture detected: " << GestureTypeToString(result.type) 
             << " (confidence: " << result.confidence << ")" << endl;
    }
    
    return result;
}

void GestureIOControl::UpdateIO(GestureResult result) {
    if (result.confidence < 0.7f) {
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
        case GESTURE_SWIPE_LEFT: return "swipe_left";
        case GESTURE_SWIPE_RIGHT: return "swipe_right";
        default: return "unknown";
    }
}