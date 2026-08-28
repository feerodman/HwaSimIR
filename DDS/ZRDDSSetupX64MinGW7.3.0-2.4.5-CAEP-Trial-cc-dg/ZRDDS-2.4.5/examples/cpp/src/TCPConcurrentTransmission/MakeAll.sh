rm -rf SimpleInterfaceBytesTypePub
rm -rf SimpleInterfaceBytesTypeSub
rm -rf SimpleInterfaceZeroCopyPub
rm -rf SimpleInterfaceZeroCopySub
g++ SimpleInterfaceBytesTypePub.cpp -D_ZRDDSCPPINTERFACE -I../../../../include/ZRDDSCoreInterface -I../../../../include/CPlusPlusInterface -L../../../../lib -lZRDDSCppzd -lpthread -o SimpleInterfaceBytesTypePub
g++ SimpleInterfaceBytesTypeSub.cpp -D_ZRDDSCPPINTERFACE -I../../../../include/ZRDDSCoreInterface -I../../../../include/CPlusPlusInterface -L../../../../lib -lZRDDSCppzd -lpthread -o SimpleInterfaceBytesTypeSub
g++ SimpleInterfaceZeroCopyPub.cpp -D_ZRDDSCPPINTERFACE -I../../../../include/ZRDDSCoreInterface -I../../../../include/CPlusPlusInterface -L../../../../lib -lZRDDSCppzd -lpthread -o SimpleInterfaceZeroCopyPub
g++ SimpleInterfaceZeroCopySub.cpp -D_ZRDDSCPPINTERFACE -I../../../../include/ZRDDSCoreInterface -I../../../../include/CPlusPlusInterface -L../../../../lib -lZRDDSCppzd -lpthread -o SimpleInterfaceZeroCopySub