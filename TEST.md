# 测试步骤

## 1. 编译项目

```bash
cd /home/sgf/ws/k230_gesture_io
mkdir -p build
cd build
cmake ..
make
```

## 2. 准备模型文件

需要准备以下模型文件：
- `hand_det.kmodel` - 手部检测模型
- `handkp_det.kmodel` - 手部关键点检测模型

这些模型可以从RT-Smart SDK的模型目录获取：
```bash
cp /path/to/sdk/hand_det.kmodel /opt/models/
cp /path/to/sdk/handkp_det.kmodel /opt/models/
```

## 3. 运行测试

### 使用摄像头测试：
```bash
./gesture_demo.elf /opt/models/hand_det.kmodel None 0.4 0.4 /opt/models/handkp_det.kmodel 1
```

### 使用本地图片测试：
```bash
./gesture_demo.elf /opt/models/hand_det.kmodel test.jpg 0.4 0.4 /opt/models/handkp_det.kmodel 1
```

## 4. 预期结果

- 摄像头模式：实时显示手势识别结果
- 图片模式：生成 `hand_kp_class_result.jpg` 文件

## 5. 支持的手势

| 手势 | 名称 | 识别条件 |
|------|------|----------|
| 握拳 | fist | 所有手指角度大于阈值 |
| 五指张开 | five | 所有手指角度小于阈值 |
| 剪刀手 | yeah | 拇指、食指、中指张开，无名指和小指弯曲 |
| 比心 | love | 拇指和食指、小指弯曲，中指和无名指弯曲 |
| 点赞 | thumbUp | 拇指弯曲，其他手指伸直 |
| ... | ... | ... |

## 6. 验证标准

- [ ] 程序能够正常启动
- [ ] 能够检测到手部
- [ ] 能够识别至少3种手势（fist, five, yeah）
- [ ] 屏幕能够正常显示识别结果
- [ ] 没有卡住或崩溃

## 7. 下一步

确认基本功能正常后，添加GPIO控制功能，实现手势控制LED和继电器。
