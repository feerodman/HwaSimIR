cmake_minimum_required(VERSION 3.5.0)

project(MutableTypeExample)

include_directories(${ZRDDS_HOME}/include/ZRDDSCoreInterface)
include_directories(${ZRDDS_HOME}/include/CPlusPlusInterface)
add_definitions(-D_ZRDDSCPPINTERFACE)
link_directories(${ZRDDS_HOME}/lib)
link_libraries(ZRDDSCppzd_VS2013.lib)

add_executable(app1 MutableTypeExample.cpp MutableTypeExampleTypeSupport.cpp app1.cpp)
add_executable(app2 MutableTypeExample.cpp MutableTypeExampleTypeSupport.cpp app2.cpp)
add_executable(app3 MutableTypeExample.cpp MutableTypeExampleTypeSupport.cpp app3.cpp)
add_executable(app4 MutableTypeExample.cpp MutableTypeExampleTypeSupport.cpp app4.cpp)
