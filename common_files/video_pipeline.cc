#include "video_pipeline.h"

/* 16字节对齐宏，用于硬件DMA/图像缓冲区对齐 */
#define ALIGN_UP_16(x)  (((x) + 15) & ~15)

/* 构造函数：初始化管线各模块的默认配置 */
PipeLine::PipeLine(int debug_mode)
{
    // ------------------------ Sensor / VICAP 默认配置 ------------------------
    // 默认使用 GC2093，start() 中会根据探测结果自动适配
    sensor_type = GC2093_MIPI_CSI2_1920X1080_30FPS_10BIT_LINEAR;
    // VICAP 设备 ID
    vicap_dev = VICAP_DEV_ID_0;
    // VICAP → AI 通道（用于算法推理）
    vicap_chn_to_ai = VICAP_CHN_ID_1;

    // 调试模式开关
    debug_mode_ = debug_mode;
}

PipeLine::~PipeLine()
{
}

/* 管线创建：初始化 VB → VICAP（无显示） */
int PipeLine::Create()
{
    ScopedTiming st("PipeLine::Create", debug_mode_);
    k_s32 ret = 0;

    // =============================================================================================
    // 1. 配置 Video Buffer（VB）系统
    // =============================================================================================
    memset(&config, 0, sizeof(k_vb_config));
    config.max_pool_cnt = 64;  // 最多支持 64 个内存池

    // 设置 VB 全局配置
    ret = kd_mpi_vb_set_config(&config);
    if (ret) {
        printf("vb_set_config failed ret:%d, VB may already be initialized, continuing...\n", ret);
        // 不返回错误，继续执行
    }

    // 设置 VB 附加配置（JPEG、ISP 统计等）
    k_vb_supplement_config supplement_config;
    memset(&supplement_config, 0, sizeof(supplement_config));
    supplement_config.supplement_config |= VB_SUPPLEMENT_JPEG_MASK;
    ret = kd_mpi_vb_set_supplement_config(&supplement_config);
    if (ret) {
        printf("vb_set_supplement_config failed ret:%d, continuing...\n", ret);
        // 不返回错误，继续执行
    }

    // 初始化 VB 子系统
    ret = kd_mpi_vb_init();
    if (ret) {
        printf("vb_init failed ret:%d, continuing...\n", ret);
        // 不返回错误，继续执行
    }

    // =============================================================================================
    // 2. 传感器探测 & VICAP 设备配置
    // =============================================================================================
    // 自动探测 Sensor
    k_vicap_probe_config probe_cfg;
    k_vicap_sensor_info sensor_info;
    probe_cfg.csi_num = CONFIG_MPP_SENSOR_DEFAULT_CSI;
    probe_cfg.width   = ISP_WIDTH;
    probe_cfg.height  = ISP_HEIGHT;
    probe_cfg.fps     = 30;
    if(0x00 != kd_mpi_sensor_adapt_get(&probe_cfg, &sensor_info)) {
        printf("vicap, can't probe sensor on %d, output %dx%d@%d\n",
               probe_cfg.csi_num, probe_cfg.width, probe_cfg.height, probe_cfg.fps);
        return -1;
    }

    sensor_type =  sensor_info.sensor_type;
    printf("Detected sensor type: %d\n", sensor_type);
    
    memset(&sensor_info, 0, sizeof(k_vicap_sensor_info));
    ret = kd_mpi_vicap_get_sensor_info(sensor_type, &sensor_info);
    if (ret) {
        printf("vicap, the sensor type not supported!\n");
        return ret;
    }

    // 配置 VICAP 设备属性（采集窗口、工作模式、ISP 功能等）
    k_vicap_dev_attr dev_attr;
    memset(&dev_attr, 0, sizeof(k_vicap_dev_attr));
    dev_attr.acq_win.h_start = 0;
    dev_attr.acq_win.v_start = 0;
    dev_attr.acq_win.width   = ISP_WIDTH;
    dev_attr.acq_win.height  = ISP_HEIGHT;
    dev_attr.mode            = VICAP_WORK_ONLINE_MODE;  // 在线模式
    dev_attr.pipe_ctrl.data  = 0xFFFFFFFF;
    dev_attr.pipe_ctrl.bits.af_enable   = 0;
    dev_attr.pipe_ctrl.bits.ahdr_enable = 0;
    dev_attr.pipe_ctrl.bits.dnr3_enable = 0;
    dev_attr.cpature_frame   = 0;
    dev_attr.sensor_info     = sensor_info;

    ret = kd_mpi_vicap_set_dev_attr(vicap_dev, dev_attr);
    if (ret) {
        printf("vicap, kd_mpi_vicap_set_dev_attr failed.\n");
        return ret;
    }

    // =============================================================================================
    // 3. VICAP 通道 1：输出给 AI 使用（RGB Planar）
    // =============================================================================================
    k_vicap_chn_attr chn1_attr;
    memset(&chn1_attr, 0, sizeof(k_vicap_chn_attr));
    chn1_attr.out_win.width  = AI_FRAME_WIDTH;
    chn1_attr.out_win.height = AI_FRAME_HEIGHT;
    chn1_attr.crop_win       = dev_attr.acq_win;
    chn1_attr.scale_win      = chn1_attr.out_win;
    chn1_attr.crop_enable    = K_FALSE;
    chn1_attr.scale_enable   = K_TRUE;  // 启用缩放
    chn1_attr.chn_enable     = K_TRUE;
    chn1_attr.pix_format     = PIXEL_FORMAT_RGB_888_PLANAR; // AI 常用输入格式
    chn1_attr.buffer_num     = VICAP_MAX_FRAME_COUNT;
    chn1_attr.buffer_size    = VICAP_ALIGN_UP((AI_FRAME_WIDTH * AI_FRAME_HEIGHT * 3 ), VICAP_ALIGN_1K);
    chn1_attr.buffer_pool_id = VB_INVALID_POOLID;

    printf("kd_mpi_vicap_set_chn_attr, buffer_size[%d]\n", chn1_attr.buffer_size);
    ret = kd_mpi_vicap_set_chn_attr(vicap_dev, vicap_chn_to_ai, chn1_attr);
    if (ret) {
        printf("kd_mpi_vicap_set_chn_attr failed.\n");
        return ret;
    }

    // 设置数据库解析模式（XML/JSON）
    ret = kd_mpi_vicap_set_database_parse_mode(vicap_dev, VICAP_DATABASE_PARSE_XML_JSON);
    if (ret) {
        printf("kd_mpi_vicap_set_database_parse_mode failed.\n");
        return ret;
    }

    // 初始化 VICAP
    printf("kd_mpi_vicap_init\n");
    ret = kd_mpi_vicap_init(vicap_dev);
    if (ret) {
        printf("kd_mpi_vicap_init failed.\n");
    }

    // 启动数据流
    printf("kd_mpi_vicap_start_stream\n");
    ret = kd_mpi_vicap_start_stream(vicap_dev);
    if (ret) {
        printf("kd_mpi_vicap_start_stream failed.\n");
    }

    // 等待传感器稳定
    usleep(200000);  // 200ms
    
    printf("PipeLine created successfully (no display mode)\n");
    return ret;
}

/* 从 VICAP 通道 1 获取一帧，用于 AI 推理 */
void PipeLine::GetFrame(DumpRes &dump_res){
    ScopedTiming st("PipeLine::GetFrame", debug_mode_);
    int ret=0;
    memset(&dump_info, 0, sizeof(k_video_frame_info));

    // 从 VICAP dump 一帧（阻塞最多 1000ms）
    ret = kd_mpi_vicap_dump_frame(vicap_dev, VICAP_CHN_ID_1, VICAP_DUMP_YUV, &dump_info, 1000);
    if (ret)
    {
        printf("kd_mpi_vicap_dump_frame failed, ret=%d.\n", ret);
        dump_res.virt_addr = 0;
        dump_res.phy_addr = 0;
        return;
    }

    // 检查物理地址是否有效
    if (dump_info.v_frame.phys_addr[0] == 0) {
        printf("kd_mpi_vicap_dump_frame returned invalid physical address.\n");
        dump_res.virt_addr = 0;
        dump_res.phy_addr = 0;
        return;
    }

    // 将物理地址映射为虚拟地址，供 CPU 访问
    void* mapped_addr = kd_mpi_sys_mmap(dump_info.v_frame.phys_addr[0],
                        AI_FRAME_CHANNEL*AI_FRAME_HEIGHT*AI_FRAME_WIDTH);
    
    if (mapped_addr == NULL) {
        printf("kd_mpi_sys_mmap failed.\n");
        dump_res.virt_addr = 0;
        dump_res.phy_addr = 0;
        return;
    }

    dump_res.virt_addr = reinterpret_cast<uintptr_t>(mapped_addr);
    dump_res.phy_addr = reinterpret_cast<uintptr_t>(dump_info.v_frame.phys_addr[0]);
}

/* 释放当前 dump 帧 */
int PipeLine::ReleaseFrame(DumpRes &dump_res){
    ScopedTiming st("PipeLine::ReleaseFrame", debug_mode_);
    int ret=0;

    // 解除虚拟地址映射
    kd_mpi_sys_munmap(reinterpret_cast<void*>(dump_res.virt_addr),
                      AI_FRAME_CHANNEL*AI_FRAME_HEIGHT*AI_FRAME_WIDTH);

    // 释放 VICAP dump 帧
    ret = kd_mpi_vicap_dump_release(vicap_dev, VICAP_CHN_ID_1, &dump_info);
    if (ret)
    {
        printf("kd_mpi_vicap_dump_release failed.\n");
    }
    return ret;
}

/* 向 OSD layer 插入一帧（无显示模式下不使用） */
int PipeLine::InsertFrame(void* osd_data){
    // 无显示模式，不执行任何操作
    return 0;
}

/* 销毁管线，释放所有资源 */
int PipeLine::Destroy()
{
    ScopedTiming st("PipeLine::Destroy", debug_mode_);
    int ret=0;

    // ------------------ 停止 VICAP ------------------
    ret = kd_mpi_vicap_stop_stream(vicap_dev);
    if (ret) {
        printf("kd_mpi_vicap_stop_stream failed.\n");
        return ret;
    }

    // 反初始化 VICAP
    ret = kd_mpi_vicap_deinit(vicap_dev);
    if (ret) {
        printf("kd_mpi_vicap_deinit failed.\n");
        return ret;
    }

    // ------------------ 反初始化 VB ------------------
    ret = kd_mpi_vb_exit();
    if (ret) {
        printf("kd_mpi_vb_exit failed.\n");
        return ret;
    }

    printf("PipeLine destroyed successfully\n");
    return 0;
}