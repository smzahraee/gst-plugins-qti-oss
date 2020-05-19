project(mm-vdec-omx-test)
cmake_minimum_required(VERSION 2.6)

set(ROOT_PATH ${CMAKE_CURRENT_SOURCE_DIR})
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror -Wno-int-conversion")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror ")

add_definitions(
    -D__AGL__
    -D_GNU_SOURCE
    -DUSE_ION
    -D_LINUX_
    -D_MULTI_PAYLOAD_
    -D_MSM8974_
)

set(SOURCE_FILES
    ${ROOT_PATH}/queue.c
    ${ROOT_PATH}/queue.h
    ${ROOT_PATH}/xdg-shell-protocol.c
    ${ROOT_PATH}/xdg-shell-client-protocol.h
    ${ROOT_PATH}/ivi-application-protocol.c
    ${ROOT_PATH}/ivi-application-client-protocol.h
    ${ROOT_PATH}/scaler-protocol.c
    ${ROOT_PATH}/scaler-client-protocol.h
    ${ROOT_PATH}/omx_vdec_test.cpp
)

add_executable (mm-vdec-omx-test ${SOURCE_FILES})

include_directories (${SYSROOTINC_PATH})
include_directories (${SYSROOT_INCLUDEDIR})

link_directories(${SYSROOT_LIBDIR})

target_link_libraries (mm-vdec-omx-test glib-2.0)
target_link_libraries (mm-vdec-omx-test pthread)
target_link_libraries (mm-vdec-omx-test wayland-client)
target_link_libraries (mm-vdec-omx-test gbm)
target_link_libraries (mm-vdec-omx-test drm)
target_link_libraries (mm-vdec-omx-test OmxCore)
target_link_libraries (mm-vdec-omx-test OmxVdec)
target_link_libraries (mm-vdec-omx-test EGL_adreno)


install (TARGETS mm-vdec-omx-test DESTINATION ${CMAKE_INSTALL_BINDIR})
