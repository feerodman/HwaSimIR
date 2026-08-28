rm -rf SimpleInterfaceBytesTypePub_c
rm -rf SimpleInterfaceBytesTypeSub_c
gcc SimpleInterfaceBytesTypePub.c -I../../../../include/ZRDDSCoreInterface -I../../../../include/CInterface -L../../../../lib -lZRDDSCzd -lpthread -lstdc++ -lws2_32 -lwsock32 -liphlpapi -o SimpleInterfaceBytesTypePub_c
gcc SimpleInterfaceBytesTypeSub.c -I../../../../include/ZRDDSCoreInterface -I../../../../include/CInterface -L../../../../lib -lZRDDSCzd -lpthread -lstdc++ -lws2_32 -lwsock32 -liphlpapi -o SimpleInterfaceBytesTypeSub_c