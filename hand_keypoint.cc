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
#include "hand_keypoint.h"
#include <string.h>
#include <vector>

HandKeypoint::HandKeypoint(char *kmodel_file, FrameCHWSize image_size, int debug_mode)
: AIBase(kmodel_file,"HandKeypoint", debug_mode)
{
    model_name_ = "HandKeypoint";
    image_size_ = image_size;
    input_size_ = {input_shapes_[0][1], input_shapes_[0][2],input_shapes_[0][3]};
    ai2d_out_tensor_ = get_input_tensor(0);
}

HandKeypoint::~HandKeypoint()
{
}

void HandKeypoint::pre_process(runtime_tensor& input_tensor, Bbox &bbox)
{
    printf("[HKP] pre_process: crop=(%d,%d,%d,%d) -> resize=%dx%d\n",
           bbox.x, bbox.y, bbox.w, bbox.h, input_size_.width, input_size_.height);
    fflush(stdout);

    // 获取输入帧数据
    auto src_buf = input_tensor.impl()->to_host().unwrap()
        ->buffer().as_host().unwrap()
        .map(map_access_::map_read).unwrap().buffer();
    uint8_t* src_data = reinterpret_cast<uint8_t*>(src_buf.data());

    int src_w = image_size_.width;
    int src_h = image_size_.height;
    int dst_w = input_size_.width;
    int dst_h = input_size_.height;

    // 计算裁剪区域（边界检查）
    int cx = std::max(0, (int)std::min((float)src_w - 1, bbox.x));
    int cy = std::max(0, (int)std::min((float)src_h - 1, bbox.y));
    int cw = std::min((int)bbox.w, src_w - cx);
    int ch = std::min((int)bbox.h, src_h - cy);

    int dst_size = dst_w * dst_h * input_size_.channel;
    std::vector<uint8_t> dst_buf(dst_size);

    // 先用114填充
    memset(dst_buf.data(), 114, dst_size);

    if (cw > 0 && ch > 0) {
        // 软件 crop + resize（双线性插值）
        float ratio_x = (float)cw / dst_w;
        float ratio_y = (float)ch / dst_h;

        for (int c = 0; c < 3; c++) {
            const uint8_t* src_plane = src_data + c * src_w * src_h;
            uint8_t* dst_plane = dst_buf.data() + c * dst_w * dst_h;

            for (int dy = 0; dy < dst_h; dy++) {
                float src_y = cy + (dy + 0.5f) * ratio_y - 0.5f;
                if (src_y < cy) src_y = cy;
                if (src_y > cy + ch - 1) src_y = cy + ch - 1;
                int y0 = (int)src_y;
                int y1 = y0 + 1;
                if (y1 > cy + ch - 1) y1 = cy + ch - 1;
                float y_frac = src_y - y0;

                for (int dx = 0; dx < dst_w; dx++) {
                    float src_x = cx + (dx + 0.5f) * ratio_x - 0.5f;
                    if (src_x < cx) src_x = cx;
                    if (src_x > cx + cw - 1) src_x = cx + cw - 1;
                    int x0 = (int)src_x;
                    int x1 = x0 + 1;
                    if (x1 > cx + cw - 1) x1 = cx + cw - 1;
                    float x_frac = src_x - x0;

                    float v00 = src_plane[y0 * src_w + x0];
                    float v01 = src_plane[y0 * src_w + x1];
                    float v10 = src_plane[y1 * src_w + x0];
                    float v11 = src_plane[y1 * src_w + x1];

                    float v0 = v00 * (1 - x_frac) + v01 * x_frac;
                    float v1 = v10 * (1 - x_frac) + v11 * x_frac;
                    float v = v0 * (1 - y_frac) + v1 * y_frac;

                    if (v < 0) v = 0;
                    if (v > 255) v = 255;

                    dst_plane[dy * dst_w + dx] = (uint8_t)v;
                }
            }
        }
    }

    printf("[HKP] crop_resize done, writing to model input tensor...\n");
    fflush(stdout);

    // 将结果写入模型输入 tensor
    auto out_buf = ai2d_out_tensor_.impl()->to_host().unwrap()
        ->buffer().as_host().unwrap()
        .map(map_access_::map_write).unwrap().buffer();
    memcpy(out_buf.data(), dst_buf.data(), dst_size);
}

void HandKeypoint::inference()
{
    this->run();
    this->get_output();
}

void HandKeypoint::post_process(Bbox &bbox)
{
    ScopedTiming st(model_name_ + " post_process", debug_mode_);
    float *pred = p_outputs_[0];
    // 绘制关键点像素坐标
    int64_t output_tensor_size = output_shapes_[0][1];// 关键点输出 （x,y）*21= 42
    results.clear();

    for (unsigned i = 0; i < output_tensor_size / 2; i++)
    {
        float x_kp;
        float y_kp;
        x_kp = pred[i * 2] * bbox.w + bbox.x;
        y_kp = pred[i * 2 + 1] * bbox.h + bbox.y;

        results.push_back(static_cast<int>(x_kp));
        results.push_back(static_cast<int>(y_kp));

    }
}

void HandKeypoint::draw_result(cv::Mat &img, std::string text, Bbox &bbox)
{
    ScopedTiming st(model_name_ + " draw_keypoints", debug_mode_);
    int img_w = img.cols;
    int img_h = img.rows;
    int64_t output_tensor_size = output_shapes_[0][1];// 关键点输出 （x,y）*21= 42
    std::vector<int>results_vd(output_tensor_size);

    int x =  int(bbox.x / image_size_.width * img_w);
    int y =  int(bbox.y / image_size_.height  * img_h);
    int w = int((bbox.w) / image_size_.width * img_w);
    int h = int((bbox.h) / image_size_.height  * img_h);
    if(img.channels()==3){
        cv::rectangle(img, cv::Rect( x,y,w,h ), cv::Scalar(0,0, 255), 4, 2, 0); 
        cv::putText(img, text, cv::Point(x, y-20), cv::FONT_HERSHEY_SIMPLEX, 2, cv::Scalar(0, 255, 0), 1);
    }
    else{
        cv::rectangle(img, cv::Rect( x,y,w,h ), cv::Scalar(0,0,255, 255), 4, 2, 0); 
        cv::putText(img, text, cv::Point(x, y-20), cv::FONT_HERSHEY_SIMPLEX, 2, cv::Scalar(0, 255, 0,255), 1);
    }

    for (unsigned i = 0; i < output_tensor_size / 2; i++)
    {
        results_vd[i * 2] = static_cast<float>(results[i*2]) / image_size_.width * img_w;
        results_vd[i * 2 + 1] = static_cast<float>(results[i*2+1]) / image_size_.height * img_h;
        if(img.channels()==3){
            cv::circle(img, cv::Point(results_vd[i * 2], results_vd[i * 2 + 1]), 4, cv::Scalar(155, 255, 255), 3);
        }else{
            cv::circle(img, cv::Point(results_vd[i * 2], results_vd[i * 2 + 1]), 4, cv::Scalar(155, 255, 255, 255), 4);
        }
        
    }

    for (unsigned k = 0; k < 5; k++)
    {
        int i = k*8;
        unsigned char R = 255, G = 0, B = 0;

        switch(k)
        {
            case 0:R = 255; G = 0; B = 0;break;
            case 1:R = 255; G = 0; B = 255;break;
            case 2:R = 255; G = 255; B = 0;break;
            case 3:R = 0; G = 255; B = 0;break;
            case 4:R = 0; G = 0; B = 255;break;
            default: std::cout << "error" << std::endl;
        }

        if(img.channels()==3){
            cv::line(img, cv::Point(results[0], results[1]), cv::Point(results[i + 2], results[i + 3]), cv::Scalar(B,G,R), 2, cv::LINE_AA);
            cv::line(img, cv::Point(results[i + 2], results[i + 3]), cv::Point(results[i + 4], results[i + 5]), cv::Scalar(B, G, R), 2, cv::LINE_AA);
            cv::line(img, cv::Point(results[i + 4], results[i + 5]), cv::Point(results[i + 6], results[i + 7]), cv::Scalar(B, G, R), 2, cv::LINE_AA);
            cv::line(img, cv::Point(results[i + 6], results[i + 7]), cv::Point(results[i + 8], results[i + 9]), cv::Scalar(B, G, R), 2, cv::LINE_AA);
        }
        else{
            cv::line(img, cv::Point(results_vd[0], results_vd[1]), cv::Point(results_vd[i + 2], results_vd[i + 3]), cv::Scalar(B,G,R,255), 2, cv::LINE_AA);
            cv::line(img, cv::Point(results_vd[i + 2], results_vd[i + 3]), cv::Point(results_vd[i + 4], results_vd[i + 5]), cv::Scalar(B, G, R,255), 2, cv::LINE_AA);
            cv::line(img, cv::Point(results_vd[i + 4], results_vd[i + 5]), cv::Point(results_vd[i + 6], results_vd[i + 7]), cv::Scalar(B, G, R,255), 2, cv::LINE_AA);
            cv::line(img, cv::Point(results_vd[i + 6], results_vd[i + 7]), cv::Point(results_vd[i + 8], results_vd[i + 9]), cv::Scalar(B, G, R,255), 2, cv::LINE_AA);
        }
    }
}

double HandKeypoint::vector_2d_angle(std::vector<double> v1, std::vector<double> v2)
{
    double v1_x = v1[0];
    double v1_y = v1[1];
    double v2_x = v2[0];
    double v2_y = v2[1];
    double v1_norm = std::sqrt(v1_x * v1_x + v1_y * v1_y);
    double v2_norm = std::sqrt(v2_x * v2_x + v2_y * v2_y);
    double dot_product = v1_x * v2_x + v1_y * v2_y;
    double cos_angle = dot_product / (v1_norm * v2_norm);
    double angle = std::acos(cos_angle) * 180 / M_PI;
    if (angle > 180.0)
    {
        return 65535.0;
    }
    return angle;
}

std::vector<double> HandKeypoint::hand_angle()
{
    double angle_;
    std::vector<double> angle_list;
    //---------------------------- thumb 大拇指角度
    angle_ = vector_2d_angle(
        {(results[0] - results[4]), (results[1] - results[5])},
        {(results[6] - results[8]), (results[7] - results[9])}
    );
    angle_list.push_back(angle_);
    //---------------------------- index 食指角度
    angle_ = vector_2d_angle(
        {(results[0] - results[12]), (results[1] - results[13])},
        {(results[14] - results[16]), (results[15] - results[17])}
    );
    angle_list.push_back(angle_);
    //---------------------------- middle 中指角度
    angle_ = vector_2d_angle(
        {(results[0] - results[20]), (results[1] - results[21])},
        {(results[22] - results[24]), (results[23] - results[25])}
    );
    angle_list.push_back(angle_);
    //---------------------------- ring 无名指角度
    angle_ = vector_2d_angle(
        {(results[0] - results[28]), (results[1] - results[29])},
        {(results[30] - results[32]), (results[31] - results[33])}
    );
    angle_list.push_back(angle_);
    //---------------------------- pink 小拇指角度
    angle_ = vector_2d_angle(
        {(results[0] - results[36]), (results[1] - results[37])},
        {(results[38] - results[40]), (results[39] - results[41])}
    );
    angle_list.push_back(angle_);
    return angle_list;
}

std::string HandKeypoint::h_gesture(std::vector<double> angle_list)
{
    int thr_angle = 65;
    int thr_angle_thumb = 53;
    int thr_angle_s = 49;
    std::string gesture_str="other";

    bool present = std::find(angle_list.begin(),angle_list.end(),65535) != angle_list.end();
    if (present)
    {
        std::cout<<"gesture_str:"<<gesture_str<<std::endl;
    }else{
        if (angle_list[0]>thr_angle_thumb && angle_list[1]>thr_angle && angle_list[2]>thr_angle && (angle_list[3]>thr_angle) && (angle_list[4]>thr_angle))
            {gesture_str = "fist";}
        else if ((angle_list[1]<thr_angle_s) && (angle_list[2]<thr_angle_s) && (angle_list[3]<thr_angle_s) && (angle_list[4]<thr_angle_s))
            {gesture_str = "five";}
        else if ((angle_list[0]<thr_angle_s)  && (angle_list[1]<thr_angle_s) && (angle_list[2]>thr_angle) && (angle_list[3]>thr_angle) && (angle_list[4]>thr_angle))
            {gesture_str = "gun";}
        else if ((angle_list[0]<thr_angle_s)  && (angle_list[1]<thr_angle_s) && (angle_list[2]>thr_angle) && (angle_list[3]>thr_angle) && (angle_list[4]<thr_angle_s))
            {gesture_str = "love";}
        else if ((angle_list[0]>5)  && (angle_list[1]<thr_angle_s) && (angle_list[2]>thr_angle) && (angle_list[3]>thr_angle) && (angle_list[4]>thr_angle))
            {gesture_str = "one";}
        else if ((angle_list[0]<thr_angle_s)  && (angle_list[1]>thr_angle) && (angle_list[2]>thr_angle) && (angle_list[3]>thr_angle) && (angle_list[4]<thr_angle_s))
            {gesture_str = "six";}
        else if ((angle_list[0]>thr_angle_thumb)  && (angle_list[1]<thr_angle_s) && (angle_list[2]<thr_angle_s) && (angle_list[3]<thr_angle_s) && (angle_list[4]>thr_angle))
            {gesture_str = "three";}
        else if ((angle_list[0]<thr_angle_s)  && (angle_list[1]>thr_angle) && (angle_list[2]>thr_angle) && (angle_list[3]>thr_angle) && (angle_list[4]>thr_angle))
            {gesture_str = "thumbUp";}
        else if ((angle_list[0]>thr_angle_thumb)  && (angle_list[1]<thr_angle_s) && (angle_list[2]<thr_angle_s) && (angle_list[3]>thr_angle) && (angle_list[4]>thr_angle))
            {gesture_str = "yeah";}
    }
    return gesture_str;
}
