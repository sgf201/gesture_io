# CMake 最低版本要求
cmake_minimum_required(VERSION 3.10)

# 设置工程名称
project(sq_handkp_class)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置SDK路径
set(SDK_PATH /home/sgf/ws/k230_pangofly/src/rtsmart)

# 设置NNCASE路径
set(NNCASE_PATH ${SDK_PATH}/libs/nncase/riscv64/nncase)
set(GSL_PATH ${SDK_PATH}/libs/nncase/riscv64)

# 设置公共文件路径
set(common_files ${CMAKE_SOURCE_DIR}/common_files)

# 添加include目录
include_directories(${PROJECT_SOURCE_DIR})
include_directories(.)
include_directories(${common_files})
include_directories(${GSL_PATH})
include_directories(${NNCASE_PATH}/include)
include_directories(${NNCASE_PATH}/include/nncase/rt/module/k230)
include_directories(${SDK_PATH}/libs/opencv/include/opencv4)
include_directories(${SDK_PATH}/mpp/include)
include_directories(${SDK_PATH}/mpp/include/comm)
include_directories(${SDK_PATH}/rtsmart/kernel/bsp/maix3)
include_directories(${SDK_PATH}/mpp/userapps/api)
include_directories(${SDK_PATH}/mpp/userapps/include)

# 添加库目录
link_directories(${NNCASE_PATH}/lib)
link_directories(${GSL_PATH}/rvvlib)
link_directories(${SDK_PATH}/mpp/userapps/lib)
link_directories(${SDK_PATH}/libs/opencv/lib)
link_directories(${SDK_PATH}/libs/opencv/lib/opencv4/3rdparty)

# 设置源文件
set(src main.cc hand_detection.cc hand_keypoint.cc)
set(common_files_src
    ${common_files}/ai_base.cc
    ${common_files}/ai_utils.cc
    ${common_files}/video_pipeline.cc
)
set(all_src ${src} ${common_files_src})
set(bin sq_handkp_class.elf)

# 生成可执行文件
add_executable(${bin} ${all_src})

# 链接库
target_link_libraries(${bin} -Wl,--start-group
    rvv
    Nncase.Runtime.Native
    nncase.rt_modules.k230
    functional_k230
    sys
    vicap
    vb
    isp
    vo
    connector
    sensor
    atomic
    gsdma
    -Wl,--end-group)

target_link_libraries(${bin} -Wl,--start-group
    opencv_imgcodecs
    opencv_imgproc
    opencv_core
    ade
    libjpeg-turbo
    libpng
    libtiff
    libwebp
    libopenjp2
    quirc
    csi_cv
    zlib
    -Wl,--end-group)

# 设置链接选项
# set_target_properties(${bin} PROPERTIES
#     LINK_FLAGS "-static"
# )

# 安装目标
install(TARGETS ${bin} DESTINATION bin)
