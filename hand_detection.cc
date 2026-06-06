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
#include "hand_detection.h"
#include <vector>
#include <string.h>

HandDetection::HandDetection(char *kmodel_file, float obj_thresh, float nms_thresh, FrameCHWSize image_size, int debug_mode)
: obj_thresh_(obj_thresh), nms_thresh_(nms_thresh), AIBase(kmodel_file,"HandDetection", debug_mode)
{
    model_name_ = "HandDetection";
    classes_num_ = 1;
    image_size_ = image_size;
    input_size_ = {input_shapes_[0][1], input_shapes_[0][2], input_shapes_[0][3]};
    ai2d_out_tensor_ = get_input_tensor(0);

    printf("[HD_INIT] image_size: %dx%dx%d, model_input: %dx%dx%d\n",
           image_size_.channel, image_size_.height, image_size_.width,
           input_size_.channel, input_size_.height, input_size_.width);
    fflush(stdout);
}


HandDetection::~HandDetection()
{
}

// 软件方式：将 RGB planar 图像 resize + padding 到模型输入尺寸
// 输入: src_data - RGB planar (CHW), src_w x src_h
// 输出: dst_data - RGB planar (CHW), dst_w x dst_h, padding用114填充
static void software_resize_pad(const uint8_t* src_data, int src_w, int src_h,
                                 uint8_t* dst_data, int dst_w, int dst_h)
{
    // 计算缩放比例（保持宽高比）
    float ratiow = (float)dst_w / src_w;
    float ratioh = (float)dst_h / src_h;
    float ratio = (ratiow < ratioh) ? ratiow : ratioh;

    int new_w = (int)(src_w * ratio);
    int new_h = (int)(src_h * ratio);
    int pad_left = (dst_w - new_w) / 2;
    int pad_top = (dst_h - new_h) / 2;

    // 先用114填充整个目标区域
    memset(dst_data, 114, dst_w * dst_h * 3);

    // 双线性插值 resize
    for (int c = 0; c < 3; c++) {
        const uint8_t* src_plane = src_data + c * src_w * src_h;
        uint8_t* dst_plane = dst_data + c * dst_w * dst_h;

        for (int dy = 0; dy < new_h; dy++) {
            float src_y = (dy + 0.5f) / ratio - 0.5f;
            if (src_y < 0) src_y = 0;
            if (src_y > src_h - 1) src_y = src_h - 1;
            int y0 = (int)src_y;
            int y1 = y0 + 1;
            if (y1 > src_h - 1) y1 = src_h - 1;
            float y_frac = src_y - y0;

            for (int dx = 0; dx < new_w; dx++) {
                float src_x = (dx + 0.5f) / ratio - 0.5f;
                if (src_x < 0) src_x = 0;
                if (src_x > src_w - 1) src_x = src_w - 1;
                int x0 = (int)src_x;
                int x1 = x0 + 1;
                if (x1 > src_w - 1) x1 = src_w - 1;
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

                dst_plane[(pad_top + dy) * dst_w + (pad_left + dx)] = (uint8_t)v;
            }
        }
    }
}

void HandDetection::pre_process(runtime_tensor& input_tensor)
{
    printf("[HD] pre_process start: %dx%d -> %dx%d\n",
           image_size_.width, image_size_.height,
           input_size_.width, input_size_.height);
    fflush(stdout);

    // 获取输入帧数据（来自 VICAP）
    auto src_buf = input_tensor.impl()->to_host().unwrap()
        ->buffer().as_host().unwrap()
        .map(map_access_::map_read).unwrap().buffer();
    uint8_t* src_data = reinterpret_cast<uint8_t*>(src_buf.data());

    printf("[HD] src_data mapped, size=%zu bytes\n", src_buf.size_bytes());
    fflush(stdout);

    // 计算目标大小
    dims_t in_shape { 1, input_size_.channel, input_size_.height, input_size_.width };
    int dst_size = input_size_.width * input_size_.height * input_size_.channel;

    printf("[HD] model input requires %d bytes (1x%dx%dx%d)\n",
           dst_size, input_size_.channel, input_size_.height, input_size_.width);
    fflush(stdout);

    // 先创建模型输入 tensor（由 NNCase 分配 KPU 可访问的内存）
    printf("[HD] Creating model input tensor with pool_shared...\n");
    fflush(stdout);

    runtime_tensor model_input = host_runtime_tensor::create(
        typecode_t::dt_uint8, in_shape, hrt::pool_shared)
        .expect("cannot create model input tensor");

    // 获取 tensor 的 CPU 可访问指针并写入 resize 后的数据
    auto model_buf = model_input.impl()->to_host().unwrap()
        ->buffer().as_host().unwrap()
        .map(map_access_::map_write).unwrap().buffer();
    uint8_t* model_data = reinterpret_cast<uint8_t*>(model_buf.data());

    printf("[HD] model tensor buffer: %p, size=%zu\n",
           (void*)model_data, model_buf.size_bytes());
    fflush(stdout);

    printf("[HD] Running software resize...\n");
    fflush(stdout);

    // 软件 resize + padding 直接写入 tensor 内存
    software_resize_pad(src_data, image_size_.width, image_size_.height,
                        model_data, input_size_.width, input_size_.height);

    printf("[HD] Syncing tensor to physical memory for KPU...\n");
    fflush(stdout);

    // 同步缓存到物理内存
    hrt::sync(model_input, sync_op_t::sync_write_back, true)
        .expect("sync write_back failed");

    printf("[HD] Setting interpreter input tensor (index 0)...\n");
    fflush(stdout);

    // 显式设置为模型解释器的输入
    set_input_tensor(0, model_input);

    printf("[HD] pre_process done\n");
    fflush(stdout);
}

void HandDetection::inference()
{
    printf("[HD] Running model inference...\n");
    fflush(stdout);

    printf("[HD] Calling kmodel_interp_.run()...\n");
    fflush(stdout);

    this->run();

    printf("[HD] Inference completed, getting output...\n");
    fflush(stdout);

    this->get_output();

    printf("[HD] Output retrieved\n");
    fflush(stdout);
}

void HandDetection::post_process(std::vector<BoxInfo> &result)
{
    result.clear();
    auto boxes0 = decode_infer(p_outputs_[0], 8, image_size_, anchors_0);
    result.insert(result.begin(), boxes0.begin(), boxes0.end());
    auto boxes1 = decode_infer(p_outputs_[1], 16, image_size_, anchors_1);
    result.insert(result.begin(), boxes1.begin(), boxes1.end());
    auto boxes2 = decode_infer(p_outputs_[2], 32, image_size_, anchors_2);
    result.insert(result.begin(), boxes2.begin(), boxes2.end());
    nms(result);
}

void HandDetection::nms(std::vector<BoxInfo> &input_boxes)
{
    std::sort(input_boxes.begin(), input_boxes.end(), [](BoxInfo a, BoxInfo b) { return a.score > b.score; });
    std::vector<float> vArea(input_boxes.size());
    for (int i = 0; i < int(input_boxes.size()); ++i)
    {
        vArea[i] = (input_boxes.at(i).x2 - input_boxes.at(i).x1 + 1)
            * (input_boxes.at(i).y2 - input_boxes.at(i).y1 + 1);
    }
    for (int i = 0; i < int(input_boxes.size()); ++i)
    {
        for (int j = i + 1; j < int(input_boxes.size());)
        {
            float xx1 = std::max(input_boxes[i].x1, input_boxes[j].x1);
            float yy1 = std::max(input_boxes[i].y1, input_boxes[j].y1);
            float xx2 = std::min(input_boxes[i].x2, input_boxes[j].x2);
            float yy2 = std::min(input_boxes[i].y2, input_boxes[j].y2);
            float w = std::max(float(0), xx2 - xx1 + 1);
            float h = std::max(float(0), yy2 - yy1 + 1);
            float inter = w * h;
            float ovr = inter / (vArea[i] + vArea[j] - inter);
            if (ovr >= nms_thresh_)
            {
                input_boxes.erase(input_boxes.begin() + j);
                vArea.erase(vArea.begin() + j);
            }
            else
            {
                j++;
            }
        }
    }
}

std::vector<BoxInfo> HandDetection::decode_infer(float *data, int stride, FrameCHWSize frame_size, float anchors[][2])
{
    float ratiow = (float)input_shapes_[0][3] / frame_size.width;
    float ratioh = (float)input_shapes_[0][2] / frame_size.height;
    float gain = ratiow < ratioh ? ratiow : ratioh;
    std::vector<BoxInfo> result;
    int grid_size = input_shapes_[0][2] / stride;
    int one_rsize = classes_num_ + 5;
    float cx, cy, w, h;

    for (int shift_y = 0; shift_y < grid_size; shift_y++)
    {
        for (int shift_x = 0; shift_x < grid_size; shift_x++)
        {

            
            int loc = shift_x + shift_y * grid_size;
            for (int i = 0; i < 3; i++)
            {
                float *record = data + (loc * 3 + i) * one_rsize;
                float *cls_ptr = record + 5;
                for (int cls = 0; cls < classes_num_; cls++)
                {
                    float score = cls_ptr[cls] * record[4];
                    if (score > obj_thresh_)
                    {
                        cx = (record[0] * 2.f - 0.5f + (float)shift_x) * (float)stride;
                        cy = (record[1] * 2.f - 0.5f + (float)shift_y) * (float)stride;
                        w = pow(record[2] * 2.f, 2) * anchors[i][0];
                        h = pow(record[3] * 2.f, 2) * anchors[i][1];

                        cx -= ((input_shapes_[0][3] - frame_size.width * gain) / 2);
                        cy -= ((input_shapes_[0][2] - frame_size.height * gain) / 2);
                        cx /= gain;
                        cy /= gain;
                        w /= gain;
                        h /= gain;
                        BoxInfo box;
                        box.x1 = std::max(0, std::min(int(frame_size.width), int(cx - w / 2.f)));
                        box.y1 = std::max(0, std::min(int(frame_size.height), int(cy - h / 2.f)));
                        box.x2 = std::max(0, std::min(int(frame_size.width), int(cx + w / 2.f)));
                        box.y2 = std::max(0, std::min(int(frame_size.height), int(cy + h / 2.f)));
                        if ((abs(box.x1-box.x2)< 0.1*frame_size.width) || (abs(box.y1-box.y2)< 0.1*frame_size.height))
                            continue;
                        box.score = score;
                        box.label = cls;
                        result.push_back(box);
                    }
                }
            }
        }
    }
    return result;
}
