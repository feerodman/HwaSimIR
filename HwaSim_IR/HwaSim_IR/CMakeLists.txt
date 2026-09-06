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
option(HWASIMIR_ENABLE_AVFORMAT "Enable local MP4 AVFormat mux backend" ON)
option(HWASIMIR_ENABLE_ZRDDS "Enable ZRDDS C++ video publisher integration" OFF)
set(ZRDDS_ROOT "$ENV{ZRDDS_HOME}" CACHE PATH "ZRDDS target SDK root")

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

if (HWASIMIR_ENABLE_AVFORMAT)
    find_path(AVFORMAT_INCLUDE_DIR libavformat/avformat.h
            HINTS ${FFMPEG_ROOT}/include /usr/include/aarch64-linux-gnu)
    find_library(AVFORMAT_LIBRARY avformat
            HINTS ${FFMPEG_ROOT}/lib /usr/lib/aarch64-linux-gnu)
    find_library(AVFORMAT_AVCODEC_LIBRARY avcodec
            HINTS ${FFMPEG_ROOT}/lib /usr/lib/aarch64-linux-gnu)
    find_library(AVFORMAT_AVUTIL_LIBRARY avutil
            HINTS ${FFMPEG_ROOT}/lib /usr/lib/aarch64-linux-gnu)
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

    include(CheckCXXSourceCompiles)
    set(_HWASIMIR_SAVED_REQUIRED_INCLUDES "${CMAKE_REQUIRED_INCLUDES}")
    set(_HWASIMIR_SAVED_REQUIRED_LIBRARIES "${CMAKE_REQUIRED_LIBRARIES}")
    set(CMAKE_REQUIRED_INCLUDES "${RKMPP_INCLUDE_DIR}")
    set(CMAKE_REQUIRED_LIBRARIES "${RKMPP_LIBRARY}")
    unset(HWASIMIR_MPP_BUFFER_SYNC_AVAILABLE CACHE)
    check_cxx_source_compiles([=[
extern "C" {
#include <mpp_buffer.h>
}
int main()
{
    MppBuffer buffer = nullptr;
    MppBufferType type = static_cast<MppBufferType>(
        MPP_BUFFER_TYPE_DRM | MPP_BUFFER_FLAGS_CACHABLE);
    (void)type;
    mpp_buffer_sync_begin(buffer);
    mpp_buffer_sync_end(buffer);
    return 0;
}
]=] HWASIMIR_MPP_BUFFER_SYNC_AVAILABLE)
    set(CMAKE_REQUIRED_INCLUDES "${_HWASIMIR_SAVED_REQUIRED_INCLUDES}")
    set(CMAKE_REQUIRED_LIBRARIES "${_HWASIMIR_SAVED_REQUIRED_LIBRARIES}")
    unset(_HWASIMIR_SAVED_REQUIRED_INCLUDES)
    unset(_HWASIMIR_SAVED_REQUIRED_LIBRARIES)
endif()

if (HWASIMIR_ENABLE_ZRDDS)
    if (NOT ZRDDS_ROOT)
        message(FATAL_ERROR "HWASIMIR_ENABLE_ZRDDS=ON requires -DZRDDS_ROOT=<target-zrdds-root>")
    endif()
    set(ZRDDS_CPP_INCLUDE_DIR "${ZRDDS_ROOT}/include/CPlusPlusInterface")
    set(ZRDDS_CORE_INCLUDE_DIR "${ZRDDS_ROOT}/include/ZRDDSCoreInterface")
    set(ZRDDS_CPP_HEADER "${ZRDDS_CPP_INCLUDE_DIR}/ZRDDSCppSimpleInterface.h")
    set(ZRDDS_CPP_LIBRARY "${ZRDDS_ROOT}/lib/libZRDDSCpp.so")
    if (NOT EXISTS "${ZRDDS_CPP_HEADER}")
        message(FATAL_ERROR "ZRDDS C++ header not found: ${ZRDDS_CPP_HEADER}")
    endif()
    if (NOT EXISTS "${ZRDDS_CPP_LIBRARY}")
        message(FATAL_ERROR "ZRDDS C++ release library not found: ${ZRDDS_CPP_LIBRARY}")
    endif()
    if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../DDS/Runtime/DdsRuntimeManager.cpp")
        set(HWASIMIR_DDS_SOURCE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../DDS")
    elseif (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/DDS/Runtime/DdsRuntimeManager.cpp")
        set(HWASIMIR_DDS_SOURCE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/DDS")
    else()
        message(FATAL_ERROR "F1 DDS shared runtime/protocol sources not found")
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
        IR/IRSolarPosition.cpp
        IR/IRSensorModel.cpp
        IR/IRSensorPostProcess.cpp
        IR/IRTemperatureModel.cpp
        IR/IRTypes.cpp
        IR/IRWeatherEffects.cpp
        IR/IRWorldCloudStreaming.cpp

        IRSimulation.cpp
        TcpCommThread_Linux.cpp
        UdpCommThread_Linux.cpp
        Video/H264FfmpegEncoder.cpp
        Video/JpegFrameEncoder.cpp
        Video/DdsVideoPublisher.cpp
        Video/LocalMp4Recorder.cpp
)

if (HWASIMIR_ENABLE_RKMPP)
    target_sources(HwaSim_IR PRIVATE Video/H264MppEncoder.cpp)
    target_compile_definitions(HwaSim_IR PRIVATE HWASIMIR_HAS_RKMPP=1)
    if (HWASIMIR_MPP_BUFFER_SYNC_AVAILABLE)
        target_compile_definitions(HwaSim_IR PRIVATE HWASIMIR_MPP_HAS_BUFFER_SYNC=1)
        message(STATUS "RK MPP buffer sync API available: cached DRM buffers enabled")
    else()
        message(STATUS "RK MPP buffer sync API unavailable: MPP 1.3.8-compatible non-cached DRM buffers enabled")
    endif()
    target_include_directories(HwaSim_IR PRIVATE ${RKMPP_INCLUDE_DIR})
    target_link_libraries(HwaSim_IR PRIVATE ${RKMPP_LIBRARY})
    message(STATUS "RK MPP encoder enabled")
    message(STATUS "RKMPP_ROOT = ${RKMPP_ROOT}")
    message(STATUS "RKMPP_HEADER = ${RKMPP_HEADER}")
    message(STATUS "RKMPP_LIBRARY = ${RKMPP_LIBRARY}")
else()
    message(STATUS "RK MPP encoder disabled (HWASIMIR_ENABLE_RKMPP=OFF)")
endif()

if (HWASIMIR_ENABLE_AVFORMAT AND AVFORMAT_INCLUDE_DIR AND AVFORMAT_LIBRARY AND
        AVFORMAT_AVCODEC_LIBRARY AND AVFORMAT_AVUTIL_LIBRARY)
    target_compile_definitions(HwaSim_IR PRIVATE HWASIMIR_HAS_AVFORMAT=1)
    target_include_directories(HwaSim_IR PRIVATE ${AVFORMAT_INCLUDE_DIR})
    target_link_libraries(HwaSim_IR PRIVATE ${AVFORMAT_LIBRARY}
            ${AVFORMAT_AVCODEC_LIBRARY} ${AVFORMAT_AVUTIL_LIBRARY})
    message(STATUS "Local MP4 backend: shared_h264_remux (AVFormat)")
elseif (HWASIMIR_ENABLE_AVFORMAT)
    message(STATUS "Local MP4 backend: opencv_reencode (AVFormat unavailable)")
endif()

if (HWASIMIR_ENABLE_ZRDDS)
    target_sources(HwaSim_IR PRIVATE
            ${HWASIMIR_DDS_SOURCE_ROOT}/Runtime/DdsRuntimeManager.cpp
            ${HWASIMIR_DDS_SOURCE_ROOT}/Protocol/CommonDataDdsAdapter.cpp
            ${HWASIMIR_DDS_SOURCE_ROOT}/Protocol/HwaSimIRProtocolEndpoint.cpp
            ${HWASIMIR_DDS_SOURCE_ROOT}/Generated/HwaSimIRProtocolV1/HwaSimIRProtocolV1.cpp
            ${HWASIMIR_DDS_SOURCE_ROOT}/Generated/HwaSimIRProtocolV1/HwaSimIRProtocolV1TypeSupport.cpp)
    target_compile_definitions(HwaSim_IR PRIVATE _ZRDDSCPPINTERFACE HWASIMIR_HAS_ZRDDS=1)
    target_include_directories(HwaSim_IR PRIVATE
            ${ZRDDS_CPP_INCLUDE_DIR} ${ZRDDS_CORE_INCLUDE_DIR}
            ${HWASIMIR_DDS_SOURCE_ROOT}/Runtime
            ${HWASIMIR_DDS_SOURCE_ROOT}/Protocol
            ${HWASIMIR_DDS_SOURCE_ROOT}/Generated/HwaSimIRProtocolV1)
    target_link_libraries(HwaSim_IR PRIVATE ${ZRDDS_CPP_LIBRARY})
    message(STATUS "ZRDDS C++ video publisher integration enabled")
    message(STATUS "ZRDDS_ROOT = ${ZRDDS_ROOT}")
    message(STATUS "ZRDDS_CPP_HEADER = ${ZRDDS_CPP_HEADER}")
    message(STATUS "ZRDDS_CPP_LIBRARY = ${ZRDDS_CPP_LIBRARY}")
else()
    message(STATUS "ZRDDS C++ build integration disabled (HWASIMIR_ENABLE_ZRDDS=OFF)")
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
		INSTALL_RPATH "${PANDA3D_ROOT}/lib;${ZRDDS_ROOT}/lib;${RKMPP_ROOT}/usr/lib/aarch64-linux-gnu"
        BUILD_WITH_INSTALL_RPATH ON
)
