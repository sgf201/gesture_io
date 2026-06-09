cmake_minimum_required(VERSION 3.10)
project(gesture_demo LANGUAGES CXX C)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/common)

if(DEFINED ENV{SDK_PATH})
    include_directories($ENV{SDK_PATH}/src/rtsmart/libs/nncase/riscv64/nncase/include)
    include_directories($ENV{SDK_PATH}/src/rtsmart/libs/nncase/riscv64)
    include_directories($ENV{SDK_PATH}/src/rtsmart/examples/ai/yolo/src)
    link_directories($ENV{SDK_PATH}/src/rtsmart/libs/nncase/riscv64/nncase/lib)
endif()

if(GSL_INCLUDE_DIR)
    include_directories(${GSL_INCLUDE_DIR})
endif()

if(K230_LINUX_INCLUDE_DIR)
    include_directories(${K230_LINUX_INCLUDE_DIR}/libmmz)
    include_directories(${K230_LINUX_INCLUDE_DIR}/display/include)
    include_directories(${K230_LINUX_INCLUDE_DIR}/vvcam/v4l2-drm/include)
endif()

if(CMAKE_SYSROOT)
    include_directories(${CMAKE_SYSROOT}/usr/include)
    include_directories(${CMAKE_SYSROOT}/usr/include/libdrm)
    include_directories(${CMAKE_SYSROOT}/usr/include/opencv4)
    link_directories(${CMAKE_SYSROOT}/usr/lib)
    link_directories(${CMAKE_SYSROOT}/lib)
endif()

if(PANGOFLY_INCLUDE_DIR)
    include_directories(${PANGOFLY_INCLUDE_DIR})
    include_directories(${PANGOFLY_INCLUDE_DIR}/idl)
    include_directories(${PANGOFLY_INCLUDE_DIR}/pangofly)
    include_directories(${PANGOFLY_INCLUDE_DIR}/pangofly/common)
    include_directories(${PANGOFLY_INCLUDE_DIR}/pangofly/message)
    include_directories(${PANGOFLY_INCLUDE_DIR}/pangofly/node)
    include_directories(${PANGOFLY_INCLUDE_DIR}/pangofly/transport)
    include_directories(${PANGOFLY_INCLUDE_DIR}/gesture/include)
endif()

if(CMAKE_TOOLCHAIN_FILE MATCHES "buildroot/toolchainfile.cmake")
    get_filename_component(CURRENT_DIR_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)
    project(${CURRENT_DIR_NAME})
    set(src main.cc  hand_detection.cc hand_keypoint.cc)
    include(../common/ai_demo_cml_common)
    install(FILES ${AIDEMO_KMODEL_DIR}/hand_det.kmodel DESTINATION  /root/app/${PROJECT_NAME}/)
    install(FILES ${AIDEMO_KMODEL_DIR}/handkp_det.kmodel DESTINATION  /root/app/${PROJECT_NAME}/)
    install(FILES ${AIDEMO_IMAGES_DIR}/input_hd.jpg DESTINATION  /root/app/${PROJECT_NAME}/)
    install(PROGRAMS ${AIDEMO_SHELL_DIR}/handkpclass_cpp_image.sh DESTINATION  /root/app/${PROJECT_NAME}/)
    install(PROGRAMS ${AIDEMO_SHELL_DIR}/handkpclass_cpp_isp.sh DESTINATION  /root/app/${PROJECT_NAME}/)

else()

    set(src main.cc
        ${CMAKE_SOURCE_DIR}/common/utils.cc
        ${CMAKE_SOURCE_DIR}/common/ai_base.cc
        ${CMAKE_SOURCE_DIR}/common/sensor_buf_manager.cc
        hand_detection.cc hand_keypoint.cc)
    set(bin sq_handkp_class.elf)

    add_executable(${bin} ${src})
    target_link_libraries(${bin} -Wl,--start-group Nncase.Runtime.Native nncase.rt_modules.k230 functional_k230 opencv_imgcodecs opencv_imgproc opencv_core sharpyuv libjpeg.so.9 webp png z v4l2-drm drm display mmz pthread -Wl,--end-group)
    install(TARGETS ${bin} DESTINATION bin)

    set(publisher_src gesture_publisher.cc
        ${CMAKE_SOURCE_DIR}/common/utils.cc
        ${CMAKE_SOURCE_DIR}/common/ai_base.cc
        ${CMAKE_SOURCE_DIR}/common/sensor_buf_manager.cc)
    set(publisher_bin gesture_publisher)

    add_executable(${publisher_bin} ${publisher_src})
    if(PANGOFLY_LIBRARY)
        target_link_libraries(${publisher_bin} -Wl,--start-group Nncase.Runtime.Native nncase.rt_modules.k230 functional_k230 opencv_imgcodecs opencv_imgproc opencv_core ${PANGOFLY_LIBRARY} sharpyuv libjpeg.so.9 webp png z v4l2-drm drm display mmz pthread -Wl,--end-group)
    else()
        target_link_libraries(${publisher_bin} -Wl,--start-group Nncase.Runtime.Native nncase.rt_modules.k230 functional_k230 opencv_imgcodecs opencv_imgproc opencv_core pangofly sharpyuv libjpeg.so.9 webp png z v4l2-drm drm display mmz pthread -Wl,--end-group)
    endif()
    install(TARGETS ${publisher_bin} DESTINATION bin)

    set(subscriber_src gesture_subscriber.cc
        ${CMAKE_SOURCE_DIR}/common/utils.cc
        ${CMAKE_SOURCE_DIR}/common/ai_base.cc
        hand_detection.cc hand_keypoint.cc)
    set(subscriber_bin gesture_subscriber)

    add_executable(${subscriber_bin} ${subscriber_src})
    if(PANGOFLY_LIBRARY)
        target_link_libraries(${subscriber_bin} -Wl,--start-group Nncase.Runtime.Native nncase.rt_modules.k230 functional_k230 opencv_imgcodecs opencv_imgproc opencv_core ${PANGOFLY_LIBRARY} sharpyuv libjpeg.so.9 webp png z mmz pthread -Wl,--end-group)
    else()
        target_link_libraries(${subscriber_bin} -Wl,--start-group Nncase.Runtime.Native nncase.rt_modules.k230 functional_k230 opencv_imgcodecs opencv_imgproc opencv_core pangofly sharpyuv libjpeg.so.9 webp png z mmz pthread -Wl,--end-group)
    endif()
    install(TARGETS ${subscriber_bin} DESTINATION bin)

endif()
