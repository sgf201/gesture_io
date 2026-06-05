# K230 手势识别控制IO输出项目

## 项目概述

本项目基于K230的RT-Thread SDK开发，实现了手势识别决定IO输出的功能。通过摄像头采集图像，使用AI模型进行手势识别，并根据识别结果控制GPIO输出（LED和继电器）。

## 功能特性

- 支持6种手势识别：none、fist（握拳）、palm（手掌）、five（五指张开）、swipe_left（向左滑动）、swipe_right（向右滑动）
- 根据手势识别结果控制GPIO输出：
  - **握拳(FIST)** → LED1点亮
  - **手掌(PALM)** → LED2点亮  
  - **五指张开(FIVE)** → LED3点亮
  - **向左滑动(SWIPE_LEFT)** → 继电器开启
  - **向右滑动(SWIPE_RIGHT)** → 继电器关闭

## 硬件连接

| GPIO引脚 | 功能 | 连接设备 |
|---------|------|----------|
| GPIO11 | LED1 | 红色LED |
| GPIO12 | LED2 | 绿色LED |
| GPIO13 | LED3 | 蓝色LED |
| GPIO14 | RELAY | 继电器模块 |

## 项目结构

```
k230_gesture_io/
├── CMakeLists.txt          # CMake配置文件
├── README.md               # 项目说明文档
└── src/
    ├── main.cc             # 主程序入口
    ├── gesture_io_control.h # 手势识别与IO控制类头文件
    └── gesture_io_control.cc # 手势识别与IO控制类实现
```

## 依赖

本项目依赖以下库和模块：

- **AI基础模块**: `ai_base.h`, `ai_utils.h`, `video_pipeline.h` (位于k230_pangofly项目中)
- **GPIO驱动**: `drv_gpio.h`, `drv_fpioa.h`
- **NNCase运行时**: Nncase.Runtime.Native, nncase.rt_modules.k230
- **OpenCV**: opencv_core, opencv_imgproc, opencv_imgcodecs

## 编译

```bash
mkdir build && cd build
cmake ..
make
```

## 运行

```bash
./gesture_io_control.elf /path/to/gesture.kmodel [debug_mode]
```

### 参数说明

| 参数 | 说明 | 默认值 |
|------|------|--------|
| kmodel_path | 手势识别模型文件路径 | 必需 |
| debug_mode | 调试模式：0=关闭, 1=仅显示时间, 2=详细输出 | 1 |

### 示例

```bash
# 使用默认调试模式运行
./gesture_io_control.elf /opt/models/gesture.kmodel

# 使用详细调试模式运行
./gesture_io_control.elf /opt/models/gesture.kmodel 2
```

## 手势控制映射

| 手势 | 识别结果 | IO输出 |
|------|----------|--------|
| 握拳 | GESTURE_FIST | LED1 ON |
| 手掌 | GESTURE_PALM | LED2 ON |
| 五指张开 | GESTURE_FIVE | LED3 ON |
| 向左滑动 | GESTURE_SWIPE_LEFT | RELAY ON |
| 向右滑动 | GESTURE_SWIPE_RIGHT | RELAY OFF |
| 无手势 | GESTURE_NONE | 所有LED关闭 |

## 退出程序

按 `q` 键退出程序。

## 注意事项

1. 确保手势识别模型文件路径正确
2. 确保GPIO引脚已正确配置并连接硬件
3. 继电器模块可能需要外部电源，注意电源供应
4. 置信度阈值设为0.7，低于此值的识别结果将被忽略

## License

BSD License
