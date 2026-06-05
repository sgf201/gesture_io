cmake_minimum_required(VERSION 3.10)

project(gesture_io_control)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(COMMON_FILES_PATH "${CMAKE_SOURCE_DIR}/../k230_pangofly/src/rtsmart/examples/ai/ai_demo/common_files")

include_directories(${COMMON_FILES_PATH})
include_directories(${PROJECT_SOURCE_DIR}/src)

set(COMMON_SOURCES
    ${COMMON_FILES_PATH}/ai_base.cc
    ${COMMON_FILES_PATH}/ai_utils.cc
    ${COMMON_FILES_PATH}/video_pipeline.cc
)

set(SOURCES
    src/main.cc
    src/gesture_io_control.cc
)

set(ALL_SOURCES ${SOURCES} ${COMMON_SOURCES})

add_executable(gesture_io_control.elf ${ALL_SOURCES})

target_link_libraries(gesture_io_control.elf 
    -Wl,--start-group 
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
    drv_gpio
    drv_fpioa
    -Wl,--end-group
)

target_link_libraries(gesture_io_control.elf
    -Wl,--start-group
    opencv_imgcodecs
    opencv_imgproc
    opencv_core
    zlib
    libjpeg-turbo
    libopenjp2
    libpng
    libtiff
    libwebp
    csi_cv
    -Wl,--end-group
)

install(TARGETS gesture_io_control.elf DESTINATION bin)
