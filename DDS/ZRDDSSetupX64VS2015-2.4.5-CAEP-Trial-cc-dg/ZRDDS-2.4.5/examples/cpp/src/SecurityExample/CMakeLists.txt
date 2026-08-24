cmake_minimum_required(VERSION 3.5.0)

# 源文件目录
set(core_interface_dir "${ZRDDS_ROOT_DIR}/src/ZRDDSCoreInterface")
set(c_interface_dir "${ZRDDS_ROOT_DIR}/src/CInterface")
set(cpp_interface_dir "${ZRDDS_ROOT_DIR}/src/CPlusPlusInterface")
set(security_include_dir "${ZRDDS_ROOT_DIR}/src/ZRDDSSecurityInterface")

add_executable(auth_example_pub auth_example_pub.cpp)
# 设置动态库目标DLL宏
target_compile_definitions(auth_example_pub PRIVATE _ZRDDSCPPINTERFACE)
# 设置静态库目标头文件目录
target_include_directories(auth_example_pub PRIVATE ${core_interface_dir} ${cpp_interface_dir} ${security_include_dir})
target_link_libraries(auth_example_pub ZRDDSCppz)

add_executable(auth_example_sub auth_example_sub.cpp)
# 设置动态库目标DLL宏
target_compile_definitions(auth_example_sub PRIVATE _ZRDDSCPPINTERFACE)
# 设置静态库目标头文件目录
target_include_directories(auth_example_sub PRIVATE ${core_interface_dir} ${cpp_interface_dir} ${security_include_dir})
target_link_libraries(auth_example_sub ZRDDSCppz)

add_executable(ac_example_pub ac_example_pub.cpp)
# 设置动态库目标DLL宏
target_compile_definitions(ac_example_pub PRIVATE _ZRDDSCPPINTERFACE)
# 设置静态库目标头文件目录
target_include_directories(ac_example_pub PRIVATE ${core_interface_dir} ${cpp_interface_dir} ${security_include_dir})
target_link_libraries(ac_example_pub ZRDDSCppz)

add_executable(ac_example_sub ac_example_sub.cpp)
# 设置动态库目标DLL宏
target_compile_definitions(ac_example_sub PRIVATE _ZRDDSCPPINTERFACE)
# 设置静态库目标头文件目录
target_include_directories(ac_example_sub PRIVATE ${core_interface_dir} ${cpp_interface_dir} ${security_include_dir})
target_link_libraries(ac_example_sub ZRDDSCppz)

add_executable(encry_example_pub encry_example_pub.cpp)
# 设置动态库目标DLL宏
target_compile_definitions(encry_example_pub PRIVATE _ZRDDSCPPINTERFACE)
# 设置静态库目标头文件目录
target_include_directories(encry_example_pub PRIVATE ${core_interface_dir} ${cpp_interface_dir} ${security_include_dir})
target_link_libraries(encry_example_pub ZRDDSCppz)

add_executable(encry_example_sub encry_example_sub.cpp)
# 设置动态库目标DLL宏
target_compile_definitions(encry_example_sub PRIVATE _ZRDDSCPPINTERFACE)
# 设置静态库目标头文件目录
target_include_directories(encry_example_sub PRIVATE ${core_interface_dir} ${cpp_interface_dir} ${security_include_dir})
target_link_libraries(encry_example_sub ZRDDSCppz)