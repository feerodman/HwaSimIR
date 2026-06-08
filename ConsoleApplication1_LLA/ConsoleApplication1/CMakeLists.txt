cmake_minimum_required(VERSION 4.1)
project(HwaSim_RK3588)

set(CMAKE_CXX_STANDARD 14)

set(PANDA3D_ROOT "/opt/panda3d/build_aarch")

find_package(Threads REQUIRED)

include_directories(
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/Common
        ${PANDA3D_ROOT}/include
       # ${PANDA3D_ROOT}/include/parser-inc
        /usr/include/eigen3
)
message(${PANDA3D_ROOT}/include)
link_directories(${PANDA3D_ROOT}/lib)

add_executable(HwaSim_RK3588 main.cpp
        HwaSimIR.cpp
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
        UdpCommThread_Linux.cpp)

target_link_libraries(HwaSim_RK3588
        p3framework
        panda
        pandaexpress
        p3dtool
        p3dtoolconfig
        p3direct
        HwaSim_RK3588 PRIVATE Threads::Threads
)
set_target_properties(${HwaSim_RK3588} PROPERTIES
        INSTALL_RPATH "${PANDA3D_ROOT}/lib"
        BUILD_WITH_INSTALL_RPATH ON
)
