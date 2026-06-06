/* Copyright (c) 2023, Canaan Bright Sight Co., Ltd
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
#include <chrono>
#include <fstream>
#include <thread>

#include "ai_utils.h"
#include "video_pipeline.h"
#include "hand_detection.h"
#include "hand_keypoint.h"


std::atomic<bool> isp_stop(false);

void print_usage(const char *name)
{
	cout << "Usage: " << name << "<kmodel_det> <input_mode> <obj_thresh> <nms_thresh> <kmodel_kp> <debug_mode>" << endl
		 << "Options:" << endl
		 << "  kmodel_det      手掌检测kmodel路径\n"
		 << "  input_mode      本地图片(图片路径)/ 摄像头(None) \n"
         << "  obj_thresh      手掌检测阈值\n"
         << "  nms_thresh      手掌检测非极大值抑制阈值\n"
		 << "  kmodel_kp       手势关键点检测kmodel路径\n"
		 << "  debug_mode      是否需要调试，0、1、2分别表示不调试、简单调试、详细调试\n"
		 << "\n"
		 << endl;
}

void video_proc(char *argv[])
{
    printf("[VIDEO_PROC] Starting video processing thread...\n");
    fflush(stdout);

    int debug_mode = atoi(argv[6]);
    FrameCHWSize image_size={AI_FRAME_CHANNEL,AI_FRAME_HEIGHT, AI_FRAME_WIDTH};

    printf("[VIDEO_PROC] AI frame size: %dx%dx%d\n",
           AI_FRAME_CHANNEL, AI_FRAME_HEIGHT, AI_FRAME_WIDTH);
    fflush(stdout);

    runtime_tensor input_tensor;
    dims_t in_shape { 1, AI_FRAME_CHANNEL, AI_FRAME_HEIGHT, AI_FRAME_WIDTH };

    printf("[VIDEO_PROC] Creating PipeLine...\n");
    fflush(stdout);

    PipeLine pl(debug_mode);
    pl.Create();

    printf("[VIDEO_PROC] PipeLine created. Initializing AI models...\n");
    printf("[VIDEO_PROC] Loading hand detection model: %s\n", argv[1]);
    fflush(stdout);

    DumpRes dump_res;
    HandDetection hd(argv[1], atof(argv[3]), atof(argv[4]), image_size, debug_mode);

    printf("[VIDEO_PROC] Hand detection model loaded. Loading keypoint model: %s\n", argv[5]);
    fflush(stdout);

    HandKeypoint hk(argv[5], image_size, debug_mode);

    printf("[VIDEO_PROC] AI models loaded. Starting main loop...\n");
    fflush(stdout);

    std::vector<BoxInfo> results;
    int frame_count = 0;

    while(!isp_stop){
        printf("[LOOP] Frame %d: Calling GetFrame...\n", frame_count + 1);
        fflush(stdout);

        pl.GetFrame(dump_res);

        printf("[LOOP] Frame %d: GetFrame returned virt_addr=0x%lx, phy_addr=0x%lx\n",
               frame_count + 1, (unsigned long)dump_res.virt_addr, (unsigned long)dump_res.phy_addr);
        fflush(stdout);

        if (dump_res.virt_addr == 0) {
            printf("[ERROR] Frame %d: GetFrame returned invalid address, skipping\n", frame_count + 1);
            fflush(stdout);
            usleep(30000);
            continue;
        }

        printf("[LOOP] Frame %d: Creating input tensor...\n", frame_count + 1);
        fflush(stdout);

        input_tensor = host_runtime_tensor::create(typecode_t::dt_uint8, in_shape, { (gsl::byte *)dump_res.virt_addr, compute_size(in_shape) },false, hrt::pool_shared, dump_res.phy_addr).expect("cannot create input tensor");

        printf("[LOOP] Frame %d: Syncing tensor...\n", frame_count + 1);
        fflush(stdout);

        hrt::sync(input_tensor, sync_op_t::sync_write_back, true).expect("sync write_back failed");

        printf("[LOOP] Frame %d: Running hand detection pre_process...\n", frame_count + 1);
        fflush(stdout);

        results.clear();
        hd.pre_process(input_tensor);

        printf("[LOOP] Frame %d: Running hand detection inference...\n", frame_count + 1);
        fflush(stdout);

        hd.inference();

        printf("[LOOP] Frame %d: Running hand detection post_process...\n", frame_count + 1);
        fflush(stdout);

        hd.post_process(results);

        printf("[LOOP] Frame %d: Hand detection done, found %zu hands\n", frame_count + 1, results.size());
        fflush(stdout);

        frame_count++;

        if (frame_count % 30 == 0) {
            printf("[FRAME] #%d: hands=%zu, virt_addr=0x%lx, phy_addr=0x%lx\n",
                   frame_count, results.size(),
                   (unsigned long)dump_res.virt_addr, (unsigned long)dump_res.phy_addr);
            fflush(stdout);
        } else if (results.size() > 0) {
            printf("[FRAME] #%d: hands=%zu\n", frame_count, results.size());
            fflush(stdout);
        }

        for (auto r: results)
        {
            int w = r.x2 - r.x1 + 1;
            int h = r.y2 - r.y1 + 1;
            int length = std::max(w,h)/2;
            int cx = (r.x1+r.x2)/2;
            int cy = (r.y1+r.y2)/2;
            int ratio_num = 1.26*length;
            int x1_1 = std::max(0,cx-ratio_num);
            int y1_1 = std::max(0,cy-ratio_num);
            int x2_1 = std::min(image_size.width-1, cx+ratio_num);
            int y2_1 = std::min(image_size.height-1, cy+ratio_num);
            int w_1 = x2_1 - x1_1 + 1;
            int h_1 = y2_1 - y1_1 + 1;
            Bbox bbox = {x:x1_1,y:y1_1,w:w_1,h:h_1};
            hk.pre_process(input_tensor,bbox);
            hk.inference();
            hk.post_process(bbox);
            std::vector<double> angle_list = hk.hand_angle();
            std::string gesture = hk.h_gesture(angle_list);

            printf("[GESTURE] Frame #%d: %s (bbox: %d,%d %dx%d)\n",
                   frame_count, gesture.c_str(), r.x1, r.y1, w, h);
            fflush(stdout);
        }

        pl.ReleaseFrame(dump_res);
    }

    printf("[VIDEO_PROC] Stop signal received. Cleaning up...\n");
    fflush(stdout);
    pl.Destroy();
    printf("[VIDEO_PROC] Cleanup done.\n");
    fflush(stdout);
}


int main(int argc, char *argv[])
{
    std::cout << "case " << argv[0] << " built at " << __DATE__ << " " << __TIME__ << std::endl;
    if (argc != 7)
    {
        print_usage(argv[0]);
        return -1;
    }

    if (strcmp(argv[2], "None") == 0)
    {
        std::thread thread_isp(video_proc, argv);
        while (getchar() != 'q')
        {
            usleep(10000);
        }

        isp_stop = true;
        thread_isp.join();
    }
    else
    {   
        int debug_mode = atoi(argv[6]);
        // 读取图片
        cv::Mat ori_img = cv::imread(argv[2]);
        FrameCHWSize image_size={ori_img.channels(),ori_img.rows,ori_img.cols};
         // 创建一个空的向量，用于存储chw图像数据,将读入的hwc数据转换成chw数据
        std::vector<uint8_t> chw_vec;
        std::vector<cv::Mat> bgrChannels(3);
        cv::split(ori_img, bgrChannels);
        for (auto i = 2; i > -1; i--)
        {
            std::vector<uint8_t> data = std::vector<uint8_t>(bgrChannels[i].reshape(1, 1));
            chw_vec.insert(chw_vec.end(), data.begin(), data.end());
        }
        // 创建tensor
        dims_t in_shape { 1, 3, ori_img.rows, ori_img.cols };
        runtime_tensor input_tensor = host_runtime_tensor::create(typecode_t::dt_uint8, in_shape, hrt::pool_shared).expect("cannot create input tensor");
        auto input_buf = input_tensor.impl()->to_host().unwrap()->buffer().as_host().unwrap().map(map_access_::map_write).unwrap().buffer();
        memcpy(reinterpret_cast<char *>(input_buf.data()), chw_vec.data(), chw_vec.size());
        hrt::sync(input_tensor, sync_op_t::sync_write_back, true).expect("write back input failed");

        HandDetection hd(argv[1], atof(argv[3]), atof(argv[4]), image_size, debug_mode);
        HandKeypoint hk(argv[5], image_size,debug_mode);
        std::vector<BoxInfo> results;
        results.clear();
        hd.pre_process(input_tensor);
        hd.inference();
        hd.post_process(results);
        for (auto r: results)
        {
            int w = r.x2 - r.x1 + 1;
            int h = r.y2 - r.y1 + 1;
            int length = std::max(w,h)/2;
            int cx = (r.x1+r.x2)/2;
            int cy = (r.y1+r.y2)/2;
            int ratio_num = 1.26*length;
            int x1_1 = std::max(0,cx-ratio_num);
            int y1_1 = std::max(0,cy-ratio_num);
            int x2_1 = std::min(image_size.width-1, cx+ratio_num);
            int y2_1 = std::min(image_size.height-1, cy+ratio_num);
            int w_1 = x2_1 - x1_1 + 1;
            int h_1 = y2_1 - y1_1 + 1;
            Bbox bbox = {x:x1_1,y:y1_1,w:w_1,h:h_1};
            Bbox draw_box={r.x1,r.y1,(r.x2-r.x1),(r.y2-r.y1)};
            hk.pre_process(input_tensor,bbox);
            hk.inference();
            hk.post_process(bbox);
            std::vector<double> angle_list = hk.hand_angle();
            std::string gesture = hk.h_gesture(angle_list);
            hk.draw_result(ori_img,gesture,draw_box);
        }
        cv::imwrite("hand_kp_class_result.jpg", ori_img);
    }
    return 0;
}
