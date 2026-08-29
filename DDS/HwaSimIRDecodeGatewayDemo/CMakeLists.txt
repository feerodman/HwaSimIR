cmake_minimum_required(VERSION 3.5)
project(HwaSimIRDecodeGatewayDemo CXX)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT ZRDDS_ROOT)
  if(DEFINED ENV{ZRDDS_HOME})
    set(ZRDDS_ROOT "$ENV{ZRDDS_HOME}")
  else()
    message(FATAL_ERROR "Set ZRDDS_ROOT or ZRDDS_HOME")
  endif()
endif()
set(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../..")
set(RKMPP_ROOT "$ENV{RKMPP_ROOT}" CACHE PATH "RK3588 MPP target sysroot")
find_path(ZRDDS_CPP_INCLUDE ZRDDSCppSimpleInterface.h PATHS "${ZRDDS_ROOT}/include/CPlusPlusInterface" NO_DEFAULT_PATH)
find_library(ZRDDS_CPP_LIBRARY NAMES ZRDDSCpp PATHS "${ZRDDS_ROOT}/lib" NO_DEFAULT_PATH)
if(NOT ZRDDS_CPP_INCLUDE OR NOT ZRDDS_CPP_LIBRARY)
  message(FATAL_ERROR "ZRDDS C++ SDK not found under ${ZRDDS_ROOT}")
endif()
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux" OR NOT CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64)$")
  message(FATAL_ERROR "DecodeGateway requires Linux AArch64 RKMPP")
endif()
set(RKMPP_INCLUDE_DIR "${RKMPP_ROOT}/usr/include/rockchip")
set(RKMPP_LIBRARY "${RKMPP_ROOT}/usr/lib/aarch64-linux-gnu/librockchip_mpp.so")
if(NOT EXISTS "${RKMPP_INCLUDE_DIR}/rk_mpi.h" OR NOT EXISTS "${RKMPP_LIBRARY}")
  message(FATAL_ERROR "RKMPP headers/library not found under ${RKMPP_ROOT}")
endif()
add_executable(HwaSimIRDecodeGatewayDemo main.cpp
  "${REPO_ROOT}/DDS/Common/MppH264GrayDecoder.cpp"
  "${REPO_ROOT}/DDS/Generated/HwaSimIRProtocolV1/HwaSimIRProtocolV1.cpp"
  "${REPO_ROOT}/DDS/Generated/HwaSimIRProtocolV1/HwaSimIRProtocolV1TypeSupport.cpp")
target_compile_definitions(HwaSimIRDecodeGatewayDemo PRIVATE
  _ZRDDSCPPINTERFACE HWASIMIR_HAS_RKMPP=1)
target_include_directories(HwaSimIRDecodeGatewayDemo PRIVATE
  "${REPO_ROOT}/DDS/Common"
  "${REPO_ROOT}/DDS/Generated/HwaSimIRProtocolV1"
  "${ZRDDS_ROOT}/include/CPlusPlusInterface" "${ZRDDS_ROOT}/include/ZRDDSCoreInterface"
  "${RKMPP_INCLUDE_DIR}")
target_link_libraries(HwaSimIRDecodeGatewayDemo PRIVATE
  "${ZRDDS_CPP_LIBRARY}" "${RKMPP_LIBRARY}" pthread dl)
set_target_properties(HwaSimIRDecodeGatewayDemo PROPERTIES
  BUILD_RPATH "${ZRDDS_ROOT}/lib;${RKMPP_ROOT}/usr/lib/aarch64-linux-gnu")
