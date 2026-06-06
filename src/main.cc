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
    cout << "Usage: " << name << " <kmodel_path> [debug_mode]" << endl
         << "Options:" << endl
         << "  kmodel_path      Path to gesture recognition kmodel" << endl
         << "  debug_mode       0: no debug, 1: time only, 2: verbose (default: 1)" << endl
         << endl;
}

int main(int argc, char *argv[]) {
    cout << "Gesture IO Control Application" << endl;
    cout << "Built at: " << __DATE__ << " " << __TIME__ << endl;
    
    if (argc < 2) {
        print_usage(argv[0]);
        return -1;
    }
    
    const char* kmodel_path = argv[1];
    int debug_mode = (argc > 2) ? atoi(argv[2]) : 1;
    
    cout << "Kmodel path: " << kmodel_path << endl;
    cout << "Debug mode: " << debug_mode << endl;
    
    try {
        GestureIOControl gesture_io(kmodel_path, debug_mode);
        
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
        
        FrameCHWSize image_size = {AI_FRAME_CHANNEL, AI_FRAME_HEIGHT, AI_FRAME_WIDTH};
        runtime_tensor input_tensor;
        dims_t in_shape = {1, AI_FRAME_CHANNEL, AI_FRAME_HEIGHT, AI_FRAME_WIDTH};
        DumpRes dump_res;
        
        cout << "Gesture IO Control started. Running..." << endl;
        
        while (true) {
            ScopedTiming st("Total frame time", 1);
            
            pl.GetFrame(dump_res);
            
            input_tensor = host_runtime_tensor::create(typecode_t::dt_uint8, in_shape,
                {(gsl::byte *)dump_res.virt_addr, compute_size(in_shape)},
                false, hrt::pool_shared, dump_res.phy_addr).expect("cannot create input tensor");
            
            hrt::sync(input_tensor, sync_op_t::sync_write_back, true).expect("sync write_back failed");
            
            gesture_io.pre_process(input_tensor);
            gesture_io.inference();
            GestureResult result = gesture_io.post_process();
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