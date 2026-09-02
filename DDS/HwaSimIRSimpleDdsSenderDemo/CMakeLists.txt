cmake_minimum_required(VERSION 3.5)
project(HwaSimIRSimpleDdsSenderDemo CXX)

if(NOT MINGW)
  message(FATAL_ERROR "This customer demo must be built with MinGW 7.3.0 x64")
endif()
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(FATAL_ERROR "This customer demo requires a 64-bit MinGW compiler")
endif()
if(NOT CMAKE_CXX_COMPILER_VERSION VERSION_EQUAL "7.3.0")
  message(FATAL_ERROR "MinGW g++ 7.3.0 is required; detected ${CMAKE_CXX_COMPILER_VERSION}")
endif()

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT ZRDDS_ROOT)
  if(DEFINED ENV{ZRDDS_HOME})
    set(ZRDDS_ROOT "$ENV{ZRDDS_HOME}")
  else()
    message(FATAL_ERROR "Set ZRDDS_ROOT to the MinGW 7.3.0 x64 ZRDDS SDK")
  endif()
endif()
file(TO_CMAKE_PATH "${ZRDDS_ROOT}" ZRDDS_ROOT)
if(ZRDDS_ROOT MATCHES "VS2015")
  message(FATAL_ERROR "Refusing the VS2015 ZRDDS SDK in a MinGW build: ${ZRDDS_ROOT}")
endif()

find_path(ZRDDS_CPP_INCLUDE ZRDDSCppSimpleInterface.h
  PATHS "${ZRDDS_ROOT}/include/CPlusPlusInterface" NO_DEFAULT_PATH)
find_file(ZRDDS_CPP_LIBRARY
  NAMES libZRDDSCpp.dll.a libZRDDSCpp.a ZRDDSCpp.lib
  PATHS "${ZRDDS_ROOT}/lib" NO_DEFAULT_PATH)
if(NOT ZRDDS_CPP_INCLUDE OR NOT ZRDDS_CPP_LIBRARY)
  message(FATAL_ERROR "MinGW ZRDDS C++ headers/library not found under ${ZRDDS_ROOT}")
endif()
if(ZRDDS_CPP_LIBRARY MATCHES "VS2015")
  message(FATAL_ERROR "Refusing VS2015 ZRDDSCpp library: ${ZRDDS_CPP_LIBRARY}")
endif()

set(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../..")
set(GENERATED_DIR "${REPO_ROOT}/DDS/Generated/HwaSimIRProtocolV1")

add_executable(HwaSimIRSimpleDdsSenderDemo
  main.cpp
  HwaSimIRSimpleDdsClient.cpp
  "${GENERATED_DIR}/HwaSimIRProtocolV1.cpp"
  "${GENERATED_DIR}/HwaSimIRProtocolV1TypeSupport.cpp")
target_include_directories(HwaSimIRSimpleDdsSenderDemo PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${GENERATED_DIR}"
  "${ZRDDS_ROOT}/include/CPlusPlusInterface"
  "${ZRDDS_ROOT}/include/ZRDDSCoreInterface")
target_compile_definitions(HwaSimIRSimpleDdsSenderDemo PRIVATE
  _ZRDDSCPPINTERFACE _ZRDDSDLLIMPORT)
target_link_libraries(HwaSimIRSimpleDdsSenderDemo PRIVATE "${ZRDDS_CPP_LIBRARY}")

add_custom_command(TARGET HwaSimIRSimpleDdsSenderDemo POST_BUILD
  COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_FILE_DIR:HwaSimIRSimpleDdsSenderDemo>/Config"
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different
    "${CMAKE_CURRENT_SOURCE_DIR}/Config/ZRDDS_PROTOCOL_QOS.xml"
    "$<TARGET_FILE_DIR:HwaSimIRSimpleDdsSenderDemo>/Config/ZRDDS_PROTOCOL_QOS.xml"
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different
    "${ZRDDS_ROOT}/lib/ZRDDSCpp.dll" "$<TARGET_FILE_DIR:HwaSimIRSimpleDdsSenderDemo>"
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different
    "${ZRDDS_ROOT}/lib/ZRDDSC.dll" "$<TARGET_FILE_DIR:HwaSimIRSimpleDdsSenderDemo>")

message(STATUS "MinGW compiler: ${CMAKE_CXX_COMPILER} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "MinGW ZRDDS root: ${ZRDDS_ROOT}")
message(STATUS "MinGW ZRDDS C++ library: ${ZRDDS_CPP_LIBRARY}")
