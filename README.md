# K230 手势识别演示程序

## 项目概述

本项目基于RT-Smart SDK开发，实现了基础的手势识别功能。通过摄像头采集图像，使用AI模型进行手势识别（支持握拳、手掌、五指张开、剪刀手等手势）。

## 功能特性

- 手部检测：使用 `hand_det.kmodel` 模型检测手部位置
- 关键点检测：使用 `handkp_det.kmodel` 模型检测手部21个关键点
- 手势识别：基于关键点角度判断手势类型
- 实时显示：在屏幕上显示识别结果

## 硬件要求

- K230开发板
- 摄像头（ST7701或LT9611）
- 显示屏

## 编译

```bash
mkdir build && cd build
cmake ..
make
```

## 运行

```bash
./gesture_demo.elf <kmodel_det> <input_mode> <obj_thresh> <nms_thresh> <kmodel_kp> <debug_mode>
```

### 参数说明

| 参数 | 说明 |
|------|------|
| kmodel_det | 手部检测模型路径 |
| input_mode | 输入模式：图片路径 或 "None"（使用摄像头）|
| obj_thresh | 检测阈值（默认0.4）|
| nms_thresh | NMS阈值（默认0.4）|
| kmodel_kp | 关键点检测模型路径 |
| debug_mode | 调试模式：0=不调试，1=简单调试，2=详细调试 |

### 示例

```bash
# 使用摄像头
./gesture_demo.elf /opt/models/hand_det.kmodel None 0.4 0.4 /opt/models/handkp_det.kmodel 1

# 使用本地图片
./gesture_demo.elf /opt/models/hand_det.kmodel test.jpg 0.4 0.4 /opt/models/handkp_det.kmodel 1
```

## 下一步计划

- [ ] 确认基本手势识别功能可用
- [ ] 添加GPIO控制功能
- [ ] 实现手势控制LED和继电器

## License

BSD License
