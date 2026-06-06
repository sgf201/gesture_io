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

/* 管线创建：初始化 VB → VICAP（无显示模式）
 * 数据流向：Sensor → VICAP
 *                         ├─ CHN0 (NV12, 全分辨率, 内部数据流)
 *                         └─ CHN1 (RGB Planar, AI分辨率, Dump给AI)
 */
int PipeLine::Create()
{
    ScopedTiming st("PipeLine::Create", debug_mode_);
    k_s32 ret = 0;
    k_vicap_sensor_info sensor_info;

    printf("[STEP 1] Configuring Video Buffer (VB)...\n");
    fflush(stdout);

    // =============================================================================================
    // 1. 配置 Video Buffer（VB）系统
    // =============================================================================================
    memset(&config, 0, sizeof(k_vb_config));
    config.max_pool_cnt = 64;

    ret = kd_mpi_vb_set_config(&config);
    if (ret) {
        printf("[WARN] vb_set_config failed ret:%d, may already be initialized\n", ret);
    }

    k_vb_supplement_config supplement_config;
    memset(&supplement_config, 0, sizeof(supplement_config));
    supplement_config.supplement_config |= VB_SUPPLEMENT_JPEG_MASK;
    ret = kd_mpi_vb_set_supplement_config(&supplement_config);
    if (ret) {
        printf("[WARN] vb_set_supplement_config failed ret:%d\n", ret);
    }

    ret = kd_mpi_vb_init();
    if (ret) {
        printf("[WARN] vb_init failed ret:%d\n", ret);
    }
    printf("[STEP 1] VB configured OK\n");
    fflush(stdout);

    // =============================================================================================
    // 2. 传感器探测
    // =============================================================================================
    printf("[STEP 2] Probing sensor on CSI=%d...\n", CONFIG_MPP_SENSOR_DEFAULT_CSI);
    fflush(stdout);

    k_vicap_probe_config probe_cfg;
    memset(&probe_cfg, 0, sizeof(probe_cfg));
    probe_cfg.csi_num = CONFIG_MPP_SENSOR_DEFAULT_CSI;
    probe_cfg.width   = ISP_WIDTH;
    probe_cfg.height  = ISP_HEIGHT;
    probe_cfg.fps     = 30;

    if (0x00 != kd_mpi_sensor_adapt_get(&probe_cfg, &sensor_info)) {
        printf("[ERROR] Cannot probe sensor on CSI=%d, output %dx%d@%d\n",
               probe_cfg.csi_num, probe_cfg.width, probe_cfg.height, probe_cfg.fps);
        return -1;
    }

    sensor_type = sensor_info.sensor_type;
    printf("[STEP 2] Sensor found: type=%d, name=%s, %ux%u@%u, csi=%u\n",
           sensor_type,
           sensor_info.sensor_name ? sensor_info.sensor_name : "unknown",
           sensor_info.width, sensor_info.height, sensor_info.fps, sensor_info.csi_num);
    fflush(stdout);

    // 重新获取完整的sensor_info
    memset(&sensor_info, 0, sizeof(k_vicap_sensor_info));
    ret = kd_mpi_vicap_get_sensor_info(sensor_type, &sensor_info);
    if (ret) {
        printf("[ERROR] vicap_get_sensor_info failed, ret=%d\n", ret);
        return ret;
    }

    // =============================================================================================
    // 3. VICAP 设备属性配置
    // =============================================================================================
    printf("[STEP 3] Configuring VICAP device attributes...\n");
    fflush(stdout);

    k_vicap_dev_attr dev_attr;
    memset(&dev_attr, 0, sizeof(k_vicap_dev_attr));
    dev_attr.acq_win.h_start = 0;
    dev_attr.acq_win.v_start = 0;
    dev_attr.acq_win.width   = sensor_info.width;   // 使用实际传感器分辨率
    dev_attr.acq_win.height  = sensor_info.height;
    dev_attr.mode            = VICAP_WORK_ONLINE_MODE;
    dev_attr.pipe_ctrl.data  = 0xFFFFFFFF;
    dev_attr.pipe_ctrl.bits.af_enable   = 0;
    dev_attr.pipe_ctrl.bits.ahdr_enable = 0;
    dev_attr.pipe_ctrl.bits.dnr3_enable = 0;
    dev_attr.cpature_frame   = 0;
    dev_attr.buffer_num      = 6;  // 重要：设置帧缓冲数量
    dev_attr.buffer_size     = VICAP_ALIGN_UP(sensor_info.width * sensor_info.height * 2, 1024);
    dev_attr.buffer_pool_id  = VB_INVALID_POOLID;
    memcpy(&dev_attr.sensor_info, &sensor_info, sizeof(k_vicap_sensor_info));

    printf("[STEP 3] Device: acq_win=%ux%u, buffer_num=%d, buffer_size=%d\n",
           dev_attr.acq_win.width, dev_attr.acq_win.height,
           dev_attr.buffer_num, dev_attr.buffer_size);
    fflush(stdout);

    ret = kd_mpi_vicap_set_dev_attr(vicap_dev, dev_attr);
    if (ret) {
        printf("[ERROR] kd_mpi_vicap_set_dev_attr failed, ret=%d\n", ret);
        return ret;
    }
    printf("[STEP 3] VICAP device attributes set OK\n");
    fflush(stdout);

    // =============================================================================================
    // 4. VICAP 通道 0：NV12 输出（内部数据流通道，即使不绑定VO也必须配置）
    // =============================================================================================
    printf("[STEP 4] Configuring VICAP CHN0 (NV12)...\n");
    fflush(stdout);

    k_vicap_chn_attr chn0_attr;
    memset(&chn0_attr, 0, sizeof(k_vicap_chn_attr));
    chn0_attr.out_win.width  = ISP_WIDTH;      // 1920
    chn0_attr.out_win.height = ISP_HEIGHT;     // 1080
    chn0_attr.crop_win       = dev_attr.acq_win;
    chn0_attr.scale_win      = chn0_attr.out_win;
    chn0_attr.crop_enable    = K_FALSE;
    chn0_attr.scale_enable   = K_FALSE;
    chn0_attr.chn_enable     = K_TRUE;
    chn0_attr.pix_format     = PIXEL_FORMAT_YUV_SEMIPLANAR_420;  // NV12
    chn0_attr.buffer_num     = 6;
    chn0_attr.buffer_size    = VICAP_ALIGN_UP(ISP_WIDTH * ISP_HEIGHT * 3 / 2, 4096);
    chn0_attr.alignment      = 12;
    chn0_attr.buffer_pool_id = VB_INVALID_POOLID;

    printf("[STEP 4] CHN0: %ux%u, format=NV12, buffer_size=%d, alignment=%d\n",
           chn0_attr.out_win.width, chn0_attr.out_win.height,
           chn0_attr.buffer_size, chn0_attr.alignment);
    fflush(stdout);

    ret = kd_mpi_vicap_set_chn_attr(vicap_dev, VICAP_CHN_ID_0, chn0_attr);
    if (ret) {
        printf("[ERROR] kd_mpi_vicap_set_chn_attr CHN0 failed, ret=%d\n", ret);
        return ret;
    }
    printf("[STEP 4] CHN0 configured OK\n");
    fflush(stdout);

    // =============================================================================================
    // 5. VICAP 通道 1：RGB Planar 输出（用于 AI 推理）
    // =============================================================================================
    printf("[STEP 5] Configuring VICAP CHN1 (RGB Planar for AI)...\n");
    fflush(stdout);

    k_vicap_chn_attr chn1_attr;
    memset(&chn1_attr, 0, sizeof(k_vicap_chn_attr));
    chn1_attr.out_win.width  = AI_FRAME_WIDTH;    // 640
    chn1_attr.out_win.height = AI_FRAME_HEIGHT;   // 360
    chn1_attr.crop_win       = dev_attr.acq_win;
    chn1_attr.scale_win      = chn1_attr.out_win;
    chn1_attr.crop_enable    = K_TRUE;            // 开启裁剪
    chn1_attr.scale_enable   = K_TRUE;            // 开启缩放
    chn1_attr.chn_enable     = K_TRUE;
    chn1_attr.pix_format     = PIXEL_FORMAT_RGB_888_PLANAR;
    chn1_attr.buffer_num     = 6;
    chn1_attr.buffer_size    = VICAP_ALIGN_UP((AI_FRAME_WIDTH * AI_FRAME_HEIGHT * 3), 4096);
    chn1_attr.alignment      = 12;
    chn1_attr.buffer_pool_id = VB_INVALID_POOLID;

    printf("[STEP 5] CHN1: %ux%u, format=RGB Planar, buffer_size=%d, alignment=%d\n",
           chn1_attr.out_win.width, chn1_attr.out_win.height,
           chn1_attr.buffer_size, chn1_attr.alignment);
    fflush(stdout);

    ret = kd_mpi_vicap_set_chn_attr(vicap_dev, vicap_chn_to_ai, chn1_attr);
    if (ret) {
        printf("[ERROR] kd_mpi_vicap_set_chn_attr CHN1 failed, ret=%d\n", ret);
        return ret;
    }
    printf("[STEP 5] CHN1 configured OK\n");
    fflush(stdout);

    // =============================================================================================
    // 6. 设置数据库解析模式，初始化 VICAP，启动数据流
    // =============================================================================================
    printf("[STEP 6] Setting database parse mode...\n");
    fflush(stdout);

    ret = kd_mpi_vicap_set_database_parse_mode(vicap_dev, VICAP_DATABASE_PARSE_XML_JSON);
    if (ret) {
        printf("[WARN] kd_mpi_vicap_set_database_parse_mode failed, ret=%d\n", ret);
    }

    printf("[STEP 7] Initializing VICAP...\n");
    fflush(stdout);

    ret = kd_mpi_vicap_init(vicap_dev);
    if (ret) {
        printf("[ERROR] kd_mpi_vicap_init failed, ret=%d\n", ret);
        return ret;
    }
    printf("[STEP 7] VICAP initialized OK\n");
    fflush(stdout);

    printf("[STEP 8] Starting VICAP stream...\n");
    fflush(stdout);

    ret = kd_mpi_vicap_start_stream(vicap_dev);
    if (ret) {
        printf("[ERROR] kd_mpi_vicap_start_stream failed, ret=%d\n", ret);
        return ret;
    }
    printf("[STEP 8] VICAP stream started OK\n");
    fflush(stdout);

    // 等待传感器稳定
    printf("[STEP 9] Waiting 200ms for sensor to stabilize...\n");
    fflush(stdout);
    usleep(200000);

    printf("[DONE] PipeLine created successfully (no display mode)\n");
    printf("[DONE] Ready to capture frames from CHN1 for AI inference\n");
    fflush(stdout);

    return 0;
}

/* 从 VICAP 通道 1 获取一帧，用于 AI 推理 */
void PipeLine::GetFrame(DumpRes &dump_res)
{
    int ret = 0;
    memset(&dump_info, 0, sizeof(k_video_frame_info));

    // 从 VICAP dump 一帧（阻塞最多 1000ms）
    ret = kd_mpi_vicap_dump_frame(vicap_dev, VICAP_CHN_ID_1, VICAP_DUMP_YUV, &dump_info, 1000);
    if (ret)
    {
        printf("[ERROR] kd_mpi_vicap_dump_frame failed, ret=%d\n", ret);
        dump_res.virt_addr = 0;
        dump_res.phy_addr = 0;
        return;
    }

    // 检查物理地址是否有效
    if (dump_info.v_frame.phys_addr[0] == 0) {
        printf("[ERROR] dump_frame returned invalid physical address (0)\n");
        dump_res.virt_addr = 0;
        dump_res.phy_addr = 0;
        return;
    }

    // 将物理地址映射为虚拟地址，供 CPU 访问
    void* mapped_addr = kd_mpi_sys_mmap(dump_info.v_frame.phys_addr[0],
                        AI_FRAME_CHANNEL * AI_FRAME_HEIGHT * AI_FRAME_WIDTH);

    if (mapped_addr == NULL) {
        printf("[ERROR] kd_mpi_sys_mmap failed (phys_addr=0x%lx)\n",
               (unsigned long)dump_info.v_frame.phys_addr[0]);
        dump_res.virt_addr = 0;
        dump_res.phy_addr = 0;
        return;
    }

    dump_res.virt_addr = reinterpret_cast<uintptr_t>(mapped_addr);
    dump_res.phy_addr = reinterpret_cast<uintptr_t>(dump_info.v_frame.phys_addr[0]);
}

/* 释放当前 dump 帧 */
int PipeLine::ReleaseFrame(DumpRes &dump_res)
{
    int ret = 0;

    // 解除虚拟地址映射
    kd_mpi_sys_munmap(reinterpret_cast<void*>(dump_res.virt_addr),
                      AI_FRAME_CHANNEL * AI_FRAME_HEIGHT * AI_FRAME_WIDTH);

    // 释放 VICAP dump 帧
    ret = kd_mpi_vicap_dump_release(vicap_dev, VICAP_CHN_ID_1, &dump_info);
    if (ret)
    {
        printf("[ERROR] kd_mpi_vicap_dump_release failed, ret=%d\n", ret);
    }
    return ret;
}

/* 向 OSD layer 插入一帧（无显示模式下不使用） */
int PipeLine::InsertFrame(void* osd_data)
{
    // 无显示模式，不执行任何操作
    return 0;
}

/* 销毁管线，释放所有资源 */
int PipeLine::Destroy()
{
    int ret = 0;

    printf("[Destroy] Stopping VICAP stream...\n");
    fflush(stdout);

    ret = kd_mpi_vicap_stop_stream(vicap_dev);
    if (ret) {
        printf("[ERROR] kd_mpi_vicap_stop_stream failed, ret=%d\n", ret);
        return ret;
    }

    ret = kd_mpi_vicap_deinit(vicap_dev);
    if (ret) {
        printf("[ERROR] kd_mpi_vicap_deinit failed, ret=%d\n", ret);
        return ret;
    }

    ret = kd_mpi_vb_exit();
    if (ret) {
        printf("[WARN] kd_mpi_vb_exit failed, ret=%d\n", ret);
    }

    printf("[Destroy] Pipeline destroyed successfully\n");
    fflush(stdout);
    return 0;
}
