cmake_minimum_required(VERSION 3.10)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_SOURCE_DIR}/cmake/toolchain_riscv64.cmake")
endif()

project(gesture_io_control)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mcmodel=medany -march=rv64imafdcv -mabi=lp64d")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mcmodel=medany -march=rv64imafdcv -mabi=lp64d")
set(CMAKE_EXE_LINKER_FLAGS "-T ${CMAKE_SOURCE_DIR}/../k230_pangofly/src/rtsmart/examples/ai/ai_demo/cmake/link.lds --static -mcmodel=medany")

set(SDK_ROOT "${CMAKE_SOURCE_DIR}/../k230_pangofly/src/rtsmart")
set(K230_ROOT "${CMAKE_SOURCE_DIR}/../k230_pangofly")

set(nncase_sdk_root "${SDK_ROOT}/libs/nncase/riscv64")
include_directories(${nncase_sdk_root})
include_directories(${nncase_sdk_root}/nncase/include)
include_directories(${nncase_sdk_root}/nncase/include/nncase/runtime)
include_directories(${nncase_sdk_root}/rvvlib/include)
link_directories(${nncase_sdk_root}/nncase/lib/)
link_directories(${nncase_sdk_root}/rvvlib/)

set(k230_opencv "${SDK_ROOT}/libs/opencv")
include_directories(${k230_opencv}/include/opencv4/)
link_directories(${k230_opencv}/lib ${k230_opencv}/lib/opencv4/3rdparty)

include_directories(${SDK_ROOT}/libs/3rd-party/include)
link_directories(${SDK_ROOT}/libs/3rd-party/lib)

set(k230_mpp_root "${SDK_ROOT}/mpp")
include_directories(${k230_mpp_root}/userapps/api/)
include_directories(${k230_mpp_root}/include)
include_directories(${k230_mpp_root}/include/comm)
link_directories(${k230_mpp_root}/userapps/lib)

set(rtsmart_hal_root "${SDK_ROOT}/libs/rtsmart_hal")
include_directories(${rtsmart_hal_root}/include)
include_directories(${rtsmart_hal_root}/drivers/gpio)
include_directories(${rtsmart_hal_root}/drivers/fpioa)
include_directories(${rtsmart_hal_root}/drivers/timer)
include_directories(${rtsmart_hal_root}/utils)
include_directories(${K230_ROOT}/include)

set(COMMON_FILES_PATH "${SDK_ROOT}/examples/ai/ai_demo/common_files")

include_directories(${COMMON_FILES_PATH})
include_directories(${PROJECT_SOURCE_DIR}/src)

set(COMMON_SOURCES
    ${COMMON_FILES_PATH}/ai_base.cc
    ${COMMON_FILES_PATH}/ai_utils.cc
    ${COMMON_FILES_PATH}/video_pipeline.cc
)

set(GPIO_SOURCES
    ${rtsmart_hal_root}/drivers/gpio/drv_gpio.c
    ${rtsmart_hal_root}/drivers/fpioa/drv_fpioa.c
    ${rtsmart_hal_root}/utils/hal_utils.c
)

set(SOURCES
    src/main.cc
    src/gesture_io_control.cc
    src/hand_detection.cc
    src/hand_keypoint.cc
)

set(ALL_SOURCES ${SOURCES} ${COMMON_SOURCES} ${GPIO_SOURCES})

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
    gcc
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

target_link_libraries(gesture_io_control.elf -latomic)

install(TARGETS gesture_io_control.elf DESTINATION bin)