cmake_minimum_required(VERSION 3.5.0)

project(ExtensionTypeExample)

include_directories(${ZRDDS_HOME}/include/ZRDDSCoreInterface)
include_directories(${ZRDDS_HOME}/include/CPlusPlusInterface)
add_definitions(-D_ZRDDSCPPINTERFACE)
link_directories(${ZRDDS_HOME}/lib)
link_libraries(ZRDDSCppzd_VS2013.lib)

add_executable(app1 ExtensionTypeExample.cpp ExtensionTypeExampleTypeSupport.cpp app1.cpp)
add_executable(app2 ExtensionTypeExample.cpp ExtensionTypeExampleTypeSupport.cpp app2.cpp)
add_executable(app3 ExtensionTypeExample.cpp ExtensionTypeExampleTypeSupport.cpp app3.cpp)
add_executable(app4 ExtensionTypeExample.cpp ExtensionTypeExampleTypeSupport.cpp app4.cpp)
