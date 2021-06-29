project(mm-vdec-omx-test-lite)
cmake_minimum_required(VERSION 2.6)

set(ROOT_PATH ${CMAKE_CURRENT_SOURCE_DIR})
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror -Wno-int-conversion")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_definitions(
    -D__AGL__
    -D_GNU_SOURCE
    -D_LINUX_
    -D_MULTI_PAYLOAD_
)

set(SOURCE_FILES
    ${ROOT_PATH}/secure_copy.cpp
    ${ROOT_PATH}/queue.c
    ${ROOT_PATH}/omx_vdec_test.cpp
)

add_executable (mm-vdec-omx-test-lite ${SOURCE_FILES})

include_directories (${SYSROOTINC_PATH})
include_directories (${SYSROOT_INCLUDEDIR})

link_directories(${SYSROOT_LIBDIR})

target_link_libraries (mm-vdec-omx-test-lite glib-2.0)
target_link_libraries (mm-vdec-omx-test-lite pthread)
target_link_libraries (mm-vdec-omx-test-lite OmxCore)
target_link_libraries (mm-vdec-omx-test-lite OmxVdec)
target_link_libraries (mm-vdec-omx-test-lite dl)


install (TARGETS mm-vdec-omx-test-lite DESTINATION ${CMAKE_INSTALL_BINDIR})
