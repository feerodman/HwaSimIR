rm -rf SimpleInterfaceBytesTypePub
rm -rf SimpleInterfaceBytesTypeSub
rm -rf SimpleInterfaceZeroCopyPub
rm -rf SimpleInterfaceZeroCopySub
g++ SimpleInterfaceBytesTypePub.cpp -D_ZRDDSCPPINTERFACE -I../../../../include/ZRDDSCoreInterface -I../../../../include/CPlusPlusInterface -L../../../../lib -lZRDDSCppzd -lpthread -ldl -o SimpleInterfaceBytesTypePub
g++ SimpleInterfaceBytesTypeSub.cpp -D_ZRDDSCPPINTERFACE -I../../../../include/ZRDDSCoreInterface -I../../../../include/CPlusPlusInterface -L../../../../lib -lZRDDSCppzd -lpthread -ldl -o SimpleInterfaceBytesTypeSub
g++ SimpleInterfaceZeroCopyPub.cpp -D_ZRDDSCPPINTERFACE -I../../../../include/ZRDDSCoreInterface -I../../../../include/CPlusPlusInterface -L../../../../lib -lZRDDSCppzd -lpthread -ldl -o SimpleInterfaceZeroCopyPub
g++ SimpleInterfaceZeroCopySub.cpp -D_ZRDDSCPPINTERFACE -I../../../../include/ZRDDSCoreInterface -I../../../../include/CPlusPlusInterface -L../../../../lib -lZRDDSCppzd -lpthread -ldl -o SimpleInterfaceZeroCopySub