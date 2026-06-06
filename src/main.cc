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
#include <iostream>
#include "ai_utils.h"
#include "video_pipeline.h"
#include "gesture_io_control.h"
#include "setting.h"

using namespace std;

void print_usage(const char *name) {
    cout << "Usage: " << name << " <kmodel_det> <obj_thresh> <nms_thresh> <kmodel_kp> <debug_mode>" << endl
         << "Options:" << endl
         << "  kmodel_det       手部检测kmodel路径 (hand_det.kmodel)" << endl
         << "  obj_thresh       手部检测阈值 (默认0.4)" << endl
         << "  nms_thresh       手部检测NMS阈值 (默认0.4)" << endl
         << "  kmodel_kp        手部关键点检测kmodel路径 (handkp_det.kmodel)" << endl
         << "  debug_mode       0: 不调试, 1: 时间统计, 2: 详细调试 (默认1)" << endl
         << endl
         << "示例:" << endl
         << "  " << name << " hand_det.kmodel 0.4 0.4 handkp_det.kmodel 1" << endl
         << endl;
}

int main(int argc, char *argv[]) {
    cout << "Gesture IO Control Application" << endl;
    cout << "Built at: " << __DATE__ << " " << __TIME__ << endl;
    
    if (argc < 5) {
        print_usage(argv[0]);
        return -1;
    }
    
    const char* kmodel_det = argv[1];
    float obj_thresh = atof(argv[2]);
    float nms_thresh = atof(argv[3]);
    const char* kmodel_kp = argv[4];
    int debug_mode = (argc > 5) ? atoi(argv[5]) : 1;
    
    cout << "手部检测模型: " << kmodel_det << endl;
    cout << "检测阈值: obj=" << obj_thresh << ", nms=" << nms_thresh << endl;
    cout << "关键点检测模型: " << kmodel_kp << endl;
    cout << "调试模式: " << debug_mode << endl;
    
    try {
        FrameCHWSize image_size = {AI_FRAME_CHANNEL, AI_FRAME_HEIGHT, AI_FRAME_WIDTH};
        GestureIOControl gesture_io(kmodel_det, obj_thresh, nms_thresh, kmodel_kp, debug_mode);
        
        if (!gesture_io.InitGPIO()) {
            cerr << "Failed to initialize GPIO" << endl;
            return -1;
        }
        
        PipeLine pl(debug_mode);
        if (pl.Create() != 0) {
            cerr << "Failed to create video pipeline" << endl;
            gesture_io.DeinitGPIO();
            return -1;
        }
        
        runtime_tensor input_tensor;
        dims_t in_shape = {1, AI_FRAME_CHANNEL, AI_FRAME_HEIGHT, AI_FRAME_WIDTH};
        DumpRes dump_res;
        
        cout << "Gesture IO Control started. Running..." << endl;
        cout << "手势映射:" << endl
             << "  FIST (握拳) -> LED1 ON" << endl
             << "  PALM (手掌) -> LED2 ON" << endl
             << "  FIVE (五指张开) -> LED3 ON" << endl
             << "  YEAH (剪刀手) -> LED1+LED2 ON" << endl
             << endl;
        
        while (true) {
            ScopedTiming st("Total frame time", 1);
            
            pl.GetFrame(dump_res);
            
            input_tensor = host_runtime_tensor::create(typecode_t::dt_uint8, in_shape,
                {(gsl::byte *)dump_res.virt_addr, compute_size(in_shape)},
                false, hrt::pool_shared, dump_res.phy_addr).expect("cannot create input tensor");
            
            hrt::sync(input_tensor, sync_op_t::sync_write_back, true).expect("sync write_back failed");
            
            gesture_io.pre_process(input_tensor);
            gesture_io.inference();
            GestureResult result = gesture_io.post_process(image_size);
            gesture_io.UpdateIO(result);
            
            pl.ReleaseFrame(dump_res);
        }
        
        pl.Destroy();
        gesture_io.DeinitGPIO();
        
        cout << "Gesture IO Control stopped." << endl;
        
    } catch (const exception& e) {
        cerr << "Error: " << e.what() << endl;
        return -1;
    }
    
    return 0;
}