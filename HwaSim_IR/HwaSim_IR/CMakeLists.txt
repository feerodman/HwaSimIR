cmake_minimum_required(VERSION 3.18)
project(HwaSim_IR)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 交叉编译目标
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

# 交叉编译器
set(CMAKE_C_COMPILER /usr/bin/aarch64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER /usr/bin/aarch64-linux-gnu-g++)

# Panda3D aarch64 根目录，可被 CLion 的 -DPANDA3D_ROOT=xxx 覆盖
set(PANDA3D_ROOT "/opt/panda3d-aarch64" CACHE PATH "Panda3D aarch64 root")

find_package(Threads REQUIRED)

set(OpenCV_DIR "/usr/lib/aarch64-linux-gnu/cmake/opencv4" CACHE PATH "OpenCV aarch64 cmake dir")
find_package(OpenCV REQUIRED CONFIG)

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
        IR/IRModtranTauLut.cpp
        IR/IRRadianceModelV2.cpp
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
)

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
