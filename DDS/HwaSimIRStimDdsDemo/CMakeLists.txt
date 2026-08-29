cmake_minimum_required(VERSION 3.5)
project(HwaSimIRStimDdsDemo CXX)

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
set(COMMONDATA_DIR "${REPO_ROOT}/HwaSim_IR/HwaSim_IR/Common")
if(NOT EXISTS "${COMMONDATA_DIR}/CommonData.h" AND EXISTS "${REPO_ROOT}/CommonData.h")
  set(COMMONDATA_DIR "${REPO_ROOT}")
endif()
if(NOT EXISTS "${COMMONDATA_DIR}/CommonData.h" AND EXISTS "${REPO_ROOT}/Common/CommonData.h")
  set(COMMONDATA_DIR "${REPO_ROOT}/Common")
endif()
find_path(ZRDDS_CPP_INCLUDE ZRDDSCppSimpleInterface.h PATHS
  "${ZRDDS_ROOT}/include/CPlusPlusInterface" NO_DEFAULT_PATH)
find_library(ZRDDS_CPP_LIBRARY NAMES ZRDDSCpp_VS2015 ZRDDSCpp PATHS
  "${ZRDDS_ROOT}/lib" NO_DEFAULT_PATH)
if(NOT ZRDDS_CPP_INCLUDE OR NOT ZRDDS_CPP_LIBRARY)
  message(FATAL_ERROR "ZRDDS C++ SDK not found under ${ZRDDS_ROOT}")
endif()

add_executable(HwaSimIRStimDdsDemo
  main.cpp
  "${REPO_ROOT}/DDS/Protocol/DdsStimClient.cpp"
  "${REPO_ROOT}/DDS/Protocol/CommonDataDdsAdapter.cpp"
  "${REPO_ROOT}/DDS/Runtime/DdsRuntimeManager.cpp"
  "${REPO_ROOT}/DDS/Generated/HwaSimIRProtocolV1/HwaSimIRProtocolV1.cpp"
  "${REPO_ROOT}/DDS/Generated/HwaSimIRProtocolV1/HwaSimIRProtocolV1TypeSupport.cpp")
target_include_directories(HwaSimIRStimDdsDemo PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${REPO_ROOT}/DDS/Protocol"
  "${REPO_ROOT}/DDS/Runtime"
  "${REPO_ROOT}/DDS/Generated/HwaSimIRProtocolV1"
  "${COMMONDATA_DIR}"
  "${ZRDDS_ROOT}/include/CPlusPlusInterface"
  "${ZRDDS_ROOT}/include/ZRDDSCoreInterface")
target_compile_definitions(HwaSimIRStimDdsDemo PRIVATE
  HWASIMIR_HAS_ZRDDS=1 _ZRDDSCPPINTERFACE _ZRDDSDLLIMPORT)
target_link_libraries(HwaSimIRStimDdsDemo PRIVATE "${ZRDDS_CPP_LIBRARY}")
if(UNIX)
  target_link_libraries(HwaSimIRStimDdsDemo PRIVATE pthread dl)
  set_target_properties(HwaSimIRStimDdsDemo PROPERTIES
    BUILD_RPATH "${ZRDDS_ROOT}/lib" INSTALL_RPATH "/usr/ZRDDS/ZRDDS-2.4.5/lib")
endif()
