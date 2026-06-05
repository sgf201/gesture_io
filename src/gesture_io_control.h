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
#ifndef _GESTURE_IO_CONTROL_H
#define _GESTURE_IO_CONTROL_H

#include <vector>
#include <string>
#include <memory>
#include "ai_base.h"
#include "ai_utils.h"

#define GPIO_PIN_LED1    11
#define GPIO_PIN_LED2    12
#define GPIO_PIN_LED3    13
#define GPIO_PIN_RELAY   14

typedef enum {
    GESTURE_NONE = 0,
    GESTURE_FIST,
    GESTURE_PALM,
    GESTURE_FIVE,
    GESTURE_SWIPE_LEFT,
    GESTURE_SWIPE_RIGHT,
    GESTURE_MAX
} GestureType;

typedef struct {
    GestureType type;
    float confidence;
    int64_t timestamp;
} GestureResult;

class GestureIOControl : public AIBase {
public:
    GestureIOControl(const char *kmodel_file, int debug_mode = 1);
    ~GestureIOControl();

    bool InitGPIO();
    void DeinitGPIO();
    
    void pre_process(runtime_tensor &input_tensor);
    void inference();
    GestureResult post_process();
    
    void UpdateIO(GestureResult result);
    void ResetIO();
    
private:
    std::unique_ptr<ai2d_builder> ai2d_builder_;
    runtime_tensor ai2d_out_tensor_;
    FrameCHWSize input_size_;
    
    drv_gpio_inst_t* gpio_led1_;
    drv_gpio_inst_t* gpio_led2_;
    drv_gpio_inst_t* gpio_led3_;
    drv_gpio_inst_t* gpio_relay_;
    
    GestureType last_gesture_;
};

const char* GestureTypeToString(GestureType type);

#endif
