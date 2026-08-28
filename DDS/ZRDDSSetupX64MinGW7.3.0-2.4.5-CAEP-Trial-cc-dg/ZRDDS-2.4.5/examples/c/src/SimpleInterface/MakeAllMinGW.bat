rm -rf SimpleInterfaceBytesTypePub_c
rm -rf SimpleInterfaceBytesTypeSub_c
rm -rf SimpleInterfaceZeroCopyPub_c
rm -rf SimpleInterfaceZeroCopySub_c
gcc SimpleInterfaceBytesTypePub.c -I../../../../include/ZRDDSCoreInterface -I../../../../include/CInterface -L../../../../lib -lZRDDSCzd -lpthread -lstdc++ -lws2_32 -lwsock32 -liphlpapi -o SimpleInterfaceBytesTypePub_c
gcc SimpleInterfaceBytesTypeSub.c -I../../../../include/ZRDDSCoreInterface -I../../../../include/CInterface -L../../../../lib -lZRDDSCzd -lpthread -lstdc++ -lws2_32 -lwsock32 -liphlpapi -o SimpleInterfaceBytesTypeSub_c
gcc SimpleInterfaceZeroCopyPub.c -I../../../../include/ZRDDSCoreInterface -I../../../../include/CInterface -L../../../../lib -lZRDDSCzd -lpthread -lstdc++ -lws2_32 -lwsock32 -liphlpapi -o SimpleInterfaceZeroCopyPub_c
gcc SimpleInterfaceZeroCopySub.c -I../../../../include/ZRDDSCoreInterface -I../../../../include/CInterface -L../../../../lib -lZRDDSCzd -lpthread -lstdc++ -lws2_32 -lwsock32 -liphlpapi -o SimpleInterfaceZeroCopySub_c