rm -rf SimpleInterfaceBytesTypePub
rm -rf SimpleInterfaceBytesTypeSub
rm -rf SimpleInterfaceZeroCopyPub
rm -rf SimpleInterfaceZeroCopySub
g++ SimpleInterfaceBytesTypePub.cpp -D_ZRDDSCPPINTERFACE -I../../../../include/ZRDDSCoreInterface -I../../../../include/CPlusPlusInterface -L../../../../lib -lZRDDSCppzd -lpthread -lws2_32 -lwsock32 -liphlpapi -o SimpleInterfaceBytesTypePub
g++ SimpleInterfaceBytesTypeSub.cpp -D_ZRDDSCPPINTERFACE -I../../../../include/ZRDDSCoreInterface -I../../../../include/CPlusPlusInterface -L../../../../lib -lZRDDSCppzd -lpthread -lws2_32 -lwsock32 -liphlpapi -o SimpleInterfaceBytesTypeSub
g++ SimpleInterfaceZeroCopyPub.cpp -D_ZRDDSCPPINTERFACE -I../../../../include/ZRDDSCoreInterface -I../../../../include/CPlusPlusInterface -L../../../../lib -lZRDDSCppzd -lpthread -lws2_32 -lwsock32 -liphlpapi -o SimpleInterfaceZeroCopyPub
g++ SimpleInterfaceZeroCopySub.cpp -D_ZRDDSCPPINTERFACE -I../../../../include/ZRDDSCoreInterface -I../../../../include/CPlusPlusInterface -L../../../../lib -lZRDDSCppzd -lpthread -lws2_32 -lwsock32 -liphlpapi -o SimpleInterfaceZeroCopySub