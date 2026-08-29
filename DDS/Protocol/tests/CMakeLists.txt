cmake_minimum_required(VERSION 3.10)
project(dds_protocol_roundtrip_test LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(ZRDDS_ROOT "$ENV{ZRDDS_HOME}" CACHE PATH "Toolchain-specific ZRDDS root")

if(NOT ZRDDS_ROOT)
    message(FATAL_ERROR "Set ZRDDS_ROOT to the ZRDDS SDK for this compiler")
endif()

find_path(ZRDDS_CPP_INCLUDE ZRDDSCppSimpleInterface.h
    HINTS "${ZRDDS_ROOT}/include/CPlusPlusInterface" NO_DEFAULT_PATH)
find_path(ZRDDS_CORE_INCLUDE ZRDDSCommon.h
    HINTS "${ZRDDS_ROOT}/include/ZRDDSCoreInterface" NO_DEFAULT_PATH)
if(MSVC)
    find_library(ZRDDS_CPP_LIBRARY NAMES ZRDDSCpp_VS2015
        HINTS "${ZRDDS_ROOT}/lib" NO_DEFAULT_PATH)
else()
    find_library(ZRDDS_CPP_LIBRARY NAMES ZRDDSCpp
        HINTS "${ZRDDS_ROOT}/lib" NO_DEFAULT_PATH)
endif()

if(NOT ZRDDS_CPP_INCLUDE OR NOT ZRDDS_CORE_INCLUDE OR NOT ZRDDS_CPP_LIBRARY)
    message(FATAL_ERROR "ZRDDS C++ SDK does not match this compiler: ${ZRDDS_ROOT}")
endif()

set(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../..")
set(GENERATED_DIR "${REPO_ROOT}/DDS/Generated/HwaSimIRProtocolV1")

add_executable(dds_protocol_roundtrip_test
    dds_protocol_roundtrip_test.cpp
    ../CommonDataDdsAdapter.cpp
    "${GENERATED_DIR}/HwaSimIRProtocolV1.cpp"
    "${GENERATED_DIR}/HwaSimIRProtocolV1TypeSupport.cpp")
target_compile_definitions(dds_protocol_roundtrip_test PRIVATE _ZRDDSCPPINTERFACE)
if(WIN32)
    target_compile_definitions(dds_protocol_roundtrip_test PRIVATE _ZRDDSDLLIMPORT)
endif()
target_include_directories(dds_protocol_roundtrip_test PRIVATE
    ..
    "${REPO_ROOT}/HwaSim_IR/HwaSim_IR/Common"
    "${GENERATED_DIR}"
    "${ZRDDS_CPP_INCLUDE}"
    "${ZRDDS_CORE_INCLUDE}")
target_link_libraries(dds_protocol_roundtrip_test PRIVATE "${ZRDDS_CPP_LIBRARY}")
if(UNIX)
    target_link_libraries(dds_protocol_roundtrip_test PRIVATE pthread dl)
    set_target_properties(dds_protocol_roundtrip_test PROPERTIES
        BUILD_RPATH "${ZRDDS_ROOT}/lib")
endif()

enable_testing()
add_test(NAME dds_protocol_roundtrip COMMAND dds_protocol_roundtrip_test)
