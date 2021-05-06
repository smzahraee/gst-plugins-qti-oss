project(venc-omx-sample)
cmake_minimum_required(VERSION 2.6)

set(ROOT_PATH ${CMAKE_CURRENT_SOURCE_DIR})
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror -Wno-int-conversion")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

add_definitions(
    -D__AGL__
    -D_GNU_SOURCE
    -D_LINUX_
    -DSUPPORT_CONFIG_INTRA_REFRESH
)

set(SOURCE_FILES
    ${ROOT_PATH}/video_sample.cpp
    ${ROOT_PATH}/memory_manager.cpp
    ${ROOT_PATH}/omx_utils.cpp
    ${ROOT_PATH}/video_queue.cpp
    ${ROOT_PATH}/config_utils.cpp
    ${ROOT_PATH}/cpu_utils.cpp
    ${ROOT_PATH}/video_types.cpp
)

add_executable (venc-omx-sample ${SOURCE_FILES})

include_directories (${SYSROOTINC_PATH})
include_directories (${SYSROOT_INCLUDEDIR})

link_directories(${SYSROOT_LIBDIR})

target_link_libraries (venc-omx-sample glib-2.0)
target_link_libraries (venc-omx-sample pthread)
target_link_libraries (venc-omx-sample OmxCore)
target_link_libraries (venc-omx-sample OmxVenc)
target_link_libraries (venc-omx-sample ion)
target_link_libraries (venc-omx-sample gbm)


install (TARGETS venc-omx-sample DESTINATION ${CMAKE_INSTALL_BINDIR})
