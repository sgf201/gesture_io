# K230 手势识别演示程序

## 项目概述

本项目基于RT-Smart SDK开发，实现了基础的手势识别功能。通过摄像头采集图像，使用AI模型进行手势识别（支持握拳、手掌、五指张开、剪刀手等手势）。

## 功能特性

- 手部检测：使用 `hand_det.kmodel` 模型检测手部位置
- 关键点检测：使用 `handkp_det.kmodel` 模型检测手部21个关键点
- 手势识别：基于关键点角度判断手势类型
- 实时显示：在屏幕上显示识别结果

## 分支说明

| 分支 | 描述 |
|------|------|
| main | 主分支，RT-Smart 平台完整实现 |
| linux | Linux 平台实现 |
| linux_pangofly | Linux 平台使用 pangofly 进行节点间通信 |

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

### 标准模式（main/linux 分支）

```bash
./sq_handkp_class.elf <kmodel_det> <input_mode> <obj_thresh> <nms_thresh> <kmodel_kp> <debug_mode>
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
./sq_handkp_class.elf /opt/models/hand_det.kmodel None 0.4 0.4 /opt/models/handkp_det.kmodel 1

# 使用本地图片
./sq_handkp_class.elf /opt/models/hand_det.kmodel test.jpg 0.4 0.4 /opt/models/handkp_det.kmodel 1
```

### Pangofly 模式（linux_pangofly 分支）

linux_pangofly 分支使用 pangofly 框架实现图像获取节点与手势识别节点的分离：

**1. 启动图像发布者（Publisher）**

```bash
./gesture_publisher
```

**2. 启动手势识别订阅者（Subscriber）**

```bash
./gesture_subscriber [<kmodel_det> <kmodel_kp> <obj_thresh> <nms_thresh> <debug_mode>]
```

**参数说明：**

| 参数 | 默认值 | 说明 |
|------|--------|------|
| kmodel_det | /opt/models/hand_det.kmodel | 手部检测模型路径 |
| kmodel_kp | /opt/models/handkp_det.kmodel | 关键点检测模型路径 |
| obj_thresh | 0.4 | 检测阈值 |
| nms_thresh | 0.4 | NMS阈值 |
| debug_mode | 1 | 调试模式 |

**节点通信架构：**

```
[摄像头] --v4l2_drm--> [gesture_publisher] --pangofly--> [gesture_subscriber] --pangofly--> [结果输出]
                              |                                       |
                              v                                       v
                       gesture_image_channel                   gesture_result_channel
```

## 支持的手势类型

| 手势ID | 手势名称 | 描述 |
|--------|----------|------|
| 0 | palm | 手掌 |
| 1 | fist | 握拳 |
| 2 | five | 五指张开 |
| 3 | yeah | 剪刀手 |
| 4 | ok | OK手势 |

## 下一步计划

- [ ] 确认基本手势识别功能可用
- [ ] 添加GPIO控制功能
- [ ] 实现手势控制LED和继电器

## License

BSD License