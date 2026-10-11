cmake_minimum_required(VERSION 3.10)
project(gesture_io C CXX)
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

    set(src main_linux.cc
        ${CMAKE_SOURCE_DIR}/common/utils.cc
        ${CMAKE_SOURCE_DIR}/common/ai_base.cc
        ${CMAKE_SOURCE_DIR}/common/sensor_buf_manager.cc
        hand_detection.cc hand_keypoint.cc
        udp_command.cc debug_http_server.cc)
    set(bin sq_handkp_class.elf)

    include_directories(${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/common
        ${CMAKE_SYSROOT}/usr/include/libdrm
        ${CMAKE_SYSROOT}/usr/include/opencv4)

    add_executable(${bin} ${src})

    target_link_libraries(${bin} -Wl,--start-group Nncase.Runtime.Native nncase.rt_modules.k230 functional_k230 opencv_imgcodecs opencv_imgproc opencv_core sharpyuv libjpeg.so.9 webp png z v4l2-drm drm display mmz pthread -Wl,--end-group)

    install(TARGETS ${bin} DESTINATION bin)
endif()
