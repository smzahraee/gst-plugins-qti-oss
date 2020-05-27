project(mm-venc-omx-test-lite)
cmake_minimum_required(VERSION 2.6)

set(ROOT_PATH ${CMAKE_CURRENT_SOURCE_DIR})
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror -Wno-int-conversion")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

add_definitions(
    -D__AGL__
    -D_GNU_SOURCE
    -DUSE_ION
    -D_LINUX_
    -D_MSM8974_
    -DUSE_GBM
)

set(SOURCE_FILES
    ${ROOT_PATH}/venc_test.cpp
    ${ROOT_PATH}/venc_util.c
    ${ROOT_PATH}/venc_util.h
    ${ROOT_PATH}/extra_data_handler.h
    ${ROOT_PATH}/vidc_debug.h
    ${ROOT_PATH}/camera_test.cpp
    ${ROOT_PATH}/camera_test.h
)

add_executable (mm-venc-omx-test-lite ${SOURCE_FILES})

include_directories (${SYSROOTINC_PATH})
include_directories (${SYSROOT_INCLUDEDIR})

link_directories(${SYSROOT_LIBDIR})

target_link_libraries (mm-venc-omx-test-lite glib-2.0)
target_link_libraries (mm-venc-omx-test-lite pthread)
target_link_libraries (mm-venc-omx-test-lite OmxCore)
target_link_libraries (mm-venc-omx-test-lite OmxVenc)
target_link_libraries (mm-venc-omx-test-lite ion)
target_link_libraries (mm-venc-omx-test-lite gbm)


install (TARGETS mm-venc-omx-test-lite DESTINATION ${CMAKE_INSTALL_BINDIR})
