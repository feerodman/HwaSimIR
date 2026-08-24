rm -rf SimpleInterfaceBytesTypePub
rm -rf SimpleInterfaceBytesTypeSub
g++ SimpleInterfaceBytesTypePub.cpp -D_ZRDDSCPPINTERFACE -I../../../../include/ZRDDSCoreInterface -I../../../../include/CPlusPlusInterface -L../../../../lib -lZRDDSCppzd -lpthread -o SimpleInterfaceBytesTypePub
g++ SimpleInterfaceBytesTypeSub.cpp -D_ZRDDSCPPINTERFACE -I../../../../include/ZRDDSCoreInterface -I../../../../include/CPlusPlusInterface -L../../../../lib -lZRDDSCppzd -lpthread -o SimpleInterfaceBytesTypeSub