project(vdec-omx-sample)
cmake_minimum_required(VERSION 2.6)

set(ROOT_PATH ${CMAKE_CURRENT_SOURCE_DIR})
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror -Wno-int-conversion")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -fprofile-arcs -ftest-coverage")
#set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_EXE_LINKER_FLAGS} -fprofile-arcs -ftest-coverage")

add_definitions(
  -D__AGL__
  -D_GNU_SOURCE
  -DUSE_ION
  -D_LINUX_
  -D_MSM8974_
)
#  -DUSE_GBM

set(SOURCE_FILES
  ${ROOT_PATH}/cpu_utils.cpp
  ${ROOT_PATH}/video_queue.cpp
  ${ROOT_PATH}/video_decode_sample.cpp
  ${ROOT_PATH}/omx_utils.cpp
  ${ROOT_PATH}/video_types.cpp
)

add_executable (vdec-omx-sample ${SOURCE_FILES})

include_directories (${SYSROOTINC_PATH})
include_directories (${SYSROOT_INCLUDEDIR})

link_directories(${SYSROOT_LIBDIR})

target_link_libraries (vdec-omx-sample glib-2.0)
target_link_libraries (vdec-omx-sample pthread)
target_link_libraries (vdec-omx-sample OmxCore)
target_link_libraries (vdec-omx-sample OmxVdec)
target_link_libraries (vdec-omx-sample ion)
target_link_libraries (vdec-omx-sample gbm)

install (TARGETS vdec-omx-sample DESTINATION ${CMAKE_INSTALL_BINDIR})

