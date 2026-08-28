
cmake_minimum_required(VERSION 3.16)

PROJECT(dds_dec VERSION 1.0.0 LANGUAGES C CXX)

SET(CMAKE_SYSTEM_NAME Linux)
SET(CMAKE_C_COMPILER "gcc")
SET(CMAKE_CXX_COMPILER "g++")
SET(CMAKE_SYSTEM_PROCESSOR "arm")

set(CMAKE_CXX_STANDARD 14)
#set(CMAKE_BUILD_TYPE release)

SET(CMAKE_BUILD_TYPE "Debug")
SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS} -O0 -Wall -g -ggdb -std=c++14 -fpermissive -fPIC -D_ZRDDSCPPINTERFACE")
SET(CMAKE_C_FLAGS_DEBUG   "${CMAKE_C_FLAGS}   -O0 -Wall -g -ggdb -std=gnu99 -Wall")

# ============ MPP ============
include_directories(/usr/include)
link_libraries(rockchip_mpp)

# ============ system ============
link_libraries(rt m pthread)

# ============ dds ============
include_directories(/usr/ZRDDS/ZRDDS-2.4.5/include/ZRDDSCoreInterface)
include_directories(/usr/ZRDDS/ZRDDS-2.4.5/include/CPlusPlusInterface)
link_directories(/usr/ZRDDS/ZRDDS-2.4.5/lib)
link_libraries(ZRDDSCpp)

include_directories(./)
include_directories(./common)
include_directories(./idl)

aux_source_directory(./common common_src)
aux_source_directory(./idl idl_src)

# ============ dds_dec: 订阅H264 -> MPP解码 -> 灰度 -> DDS发布 ============
add_executable(dds_dec
	main.cpp
	${common_src}
	${idl_src}
	)

# ============ h264_pub: H264 文件发送工具 ============
add_executable(h264_pub
	h264_pub.cpp
	${idl_src}
	)

# ============ gray_sub: 灰度图接收存文件工具 ============
add_executable(gray_sub
	gray_sub.cpp
	${idl_src}
	)
