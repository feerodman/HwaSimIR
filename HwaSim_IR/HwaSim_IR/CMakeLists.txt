cmake_minimum_required(VERSION 3.18)

# This entry point targets Debian/aarch64. Explicit command-line or toolchain
# values remain authoritative; these are the documented VM defaults.
if (NOT DEFINED CMAKE_SYSTEM_NAME)
    set(CMAKE_SYSTEM_NAME Linux)
endif()
if (NOT DEFINED CMAKE_SYSTEM_PROCESSOR)
    set(CMAKE_SYSTEM_PROCESSOR aarch64)
endif()
if (NOT DEFINED CMAKE_C_COMPILER)
    set(CMAKE_C_COMPILER /usr/bin/aarch64-linux-gnu-gcc)
endif()
if (NOT DEFINED CMAKE_CXX_COMPILER)
    set(CMAKE_CXX_COMPILER /usr/bin/aarch64-linux-gnu-g++)
endif()

project(HwaSim_IR)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Panda3D aarch64 根目录，可被 CLion 的 -DPANDA3D_ROOT=xxx 覆盖
set(PANDA3D_ROOT "/opt/panda3d-aarch64" CACHE PATH "Panda3D aarch64 root")

find_package(Threads REQUIRED)

set(OpenCV_DIR "/usr/lib/aarch64-linux-gnu/cmake/opencv4" CACHE PATH "OpenCV aarch64 cmake dir")
find_package(OpenCV REQUIRED CONFIG)

option(HWASIMIR_ENABLE_RKMPP "Enable Rockchip MPP H.264 encoder (Linux aarch64 only)" OFF)
set(RKMPP_ROOT "$ENV{RKMPP_ROOT}" CACHE PATH "RK3588 MPP sysroot root")
option(HWASIMIR_ENABLE_FFMPEG "Enable optional FFmpeg/libavcodec encoder" ON)
set(FFMPEG_ROOT "$ENV{FFMPEG_ROOT}" CACHE PATH "Optional FFmpeg/libavcodec root")

if (HWASIMIR_ENABLE_FFMPEG)
    find_path(FFMPEG_INCLUDE_DIR libavcodec/avcodec.h
            HINTS ${FFMPEG_ROOT}/include)
    find_library(FFMPEG_AVCODEC_LIBRARY avcodec
            HINTS ${FFMPEG_ROOT}/lib ${FFMPEG_ROOT}/lib/aarch64-linux-gnu)
    find_library(FFMPEG_AVUTIL_LIBRARY avutil
            HINTS ${FFMPEG_ROOT}/lib ${FFMPEG_ROOT}/lib/aarch64-linux-gnu)
    find_library(FFMPEG_SWSCALE_LIBRARY swscale
            HINTS ${FFMPEG_ROOT}/lib ${FFMPEG_ROOT}/lib/aarch64-linux-gnu)
endif()

if (HWASIMIR_ENABLE_RKMPP)
    if (NOT CMAKE_SYSTEM_NAME STREQUAL "Linux" OR NOT CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64)$")
        message(FATAL_ERROR "HWASIMIR_ENABLE_RKMPP requires Linux aarch64; got ${CMAKE_SYSTEM_NAME}/${CMAKE_SYSTEM_PROCESSOR}")
    endif()
    if (NOT RKMPP_ROOT)
        message(FATAL_ERROR "HWASIMIR_ENABLE_RKMPP=ON requires -DRKMPP_ROOT=<rk3588-mpp-sysroot>")
    endif()
    set(RKMPP_INCLUDE_DIR "${RKMPP_ROOT}/usr/include/rockchip")
    set(RKMPP_HEADER "${RKMPP_INCLUDE_DIR}/rk_mpi.h")
    set(RKMPP_LIBRARY "${RKMPP_ROOT}/usr/lib/aarch64-linux-gnu/librockchip_mpp.so")
    if (NOT EXISTS "${RKMPP_HEADER}")
        message(FATAL_ERROR "RK MPP header not found: ${RKMPP_HEADER}")
    endif()
    if (NOT EXISTS "${RKMPP_LIBRARY}")
        message(FATAL_ERROR "RK MPP library not found: ${RKMPP_LIBRARY}")
    endif()
endif()

include_directories(
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/Common
        ${CMAKE_CURRENT_SOURCE_DIR}/Annotation
        ${CMAKE_CURRENT_SOURCE_DIR}/IR
        ${CMAKE_CURRENT_SOURCE_DIR}/Eigen
        ${PANDA3D_ROOT}/include
        /usr/include/eigen3
        ${OpenCV_INCLUDE_DIRS}
)

message(STATUS "OpenCV_VERSION = ${OpenCV_VERSION}")
message(STATUS "OpenCV_INCLUDE_DIRS = ${OpenCV_INCLUDE_DIRS}")
message(STATUS "OpenCV_LIBS = ${OpenCV_LIBS}")

message(STATUS "PANDA3D_ROOT = ${PANDA3D_ROOT}")
message(STATUS "PANDA3D_INCLUDE = ${PANDA3D_ROOT}/include")
message(STATUS "PANDA3D_LIB = ${PANDA3D_ROOT}/lib")

link_directories(${PANDA3D_ROOT}/lib)

add_executable(HwaSim_IR
        main.cpp
        HwaSimIR.cpp
        GeoTransform.cpp
        AttitudeTransform.cpp

        Annotation/AnnotationConfig.cpp
        Annotation/AnnotationManager.cpp
        Annotation/AnnotationOverlay.cpp
        Annotation/AnnotationProjector.cpp

        IR/IRConfig.cpp
        IR/IREnginePlumeModel.cpp
        IR/IRModtranRadianceLut.cpp
        IR/IRModtranTauLut.cpp
        IR/IRPerfStats.cpp
        IR/IRRadianceModelV2.cpp
        IR/IRAeroThermalModel.cpp
        IR/IRRuntimeConfig.cpp
        IR/IRSceneMaterialMapper.cpp
        IR/IRSensorModel.cpp
        IR/IRSensorPostProcess.cpp
        IR/IRTemperatureModel.cpp
        IR/IRTypes.cpp
        IR/IRWeatherEffects.cpp

        IRSimulation.cpp
        TcpCommThread_Linux.cpp
        UdpCommThread_Linux.cpp
        Video/H264FfmpegEncoder.cpp
        Video/JpegFrameEncoder.cpp
)

if (HWASIMIR_ENABLE_RKMPP)
    target_sources(HwaSim_IR PRIVATE Video/H264MppEncoder.cpp)
    target_compile_definitions(HwaSim_IR PRIVATE HWASIMIR_HAS_RKMPP=1)
    target_include_directories(HwaSim_IR PRIVATE ${RKMPP_INCLUDE_DIR})
    target_link_libraries(HwaSim_IR PRIVATE ${RKMPP_LIBRARY})
    message(STATUS "RK MPP encoder enabled")
    message(STATUS "RKMPP_ROOT = ${RKMPP_ROOT}")
    message(STATUS "RKMPP_HEADER = ${RKMPP_HEADER}")
    message(STATUS "RKMPP_LIBRARY = ${RKMPP_LIBRARY}")
else()
    message(STATUS "RK MPP encoder disabled (HWASIMIR_ENABLE_RKMPP=OFF)")
endif()

if (HWASIMIR_ENABLE_FFMPEG AND FFMPEG_INCLUDE_DIR AND FFMPEG_AVCODEC_LIBRARY AND FFMPEG_AVUTIL_LIBRARY AND FFMPEG_SWSCALE_LIBRARY)
    target_compile_definitions(HwaSim_IR PRIVATE HWASIM_HAS_FFMPEG=1)
    target_include_directories(HwaSim_IR PRIVATE ${FFMPEG_INCLUDE_DIR})
    target_link_libraries(HwaSim_IR PRIVATE
            ${FFMPEG_AVCODEC_LIBRARY}
            ${FFMPEG_AVUTIL_LIBRARY}
            ${FFMPEG_SWSCALE_LIBRARY})
    message(STATUS "FFmpeg/libavcodec encoder enabled")
    message(STATUS "FFMPEG_INCLUDE_DIR = ${FFMPEG_INCLUDE_DIR}")
    message(STATUS "FFMPEG_AVCODEC_LIBRARY = ${FFMPEG_AVCODEC_LIBRARY}")
    message(STATUS "FFMPEG_AVUTIL_LIBRARY = ${FFMPEG_AVUTIL_LIBRARY}")
    message(STATUS "FFMPEG_SWSCALE_LIBRARY = ${FFMPEG_SWSCALE_LIBRARY}")
elseif (HWASIMIR_ENABLE_FFMPEG)
    message(STATUS "FFmpeg/libavcodec requested but not found; H.264 reports unavailable and JPEG fallback remains active")
else()
    message(STATUS "FFmpeg/libavcodec encoder disabled (HWASIMIR_ENABLE_FFMPEG=OFF)")
endif()

target_link_libraries(HwaSim_IR PRIVATE
        p3framework
        panda
        pandaexpress
        p3dtool
        p3dtoolconfig
        p3direct
        Threads::Threads
        ${OpenCV_LIBS}
)

set_target_properties(HwaSim_IR PROPERTIES
        INSTALL_RPATH "${PANDA3D_ROOT}/lib"
        BUILD_WITH_INSTALL_RPATH ON
)
