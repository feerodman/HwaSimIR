rm -rf SimpleInterfaceBytesTypePub_c
rm -rf SimpleInterfaceBytesTypeSub_c
rm -rf SimpleInterfaceZeroCopyPub_c
rm -rf SimpleInterfaceZeroCopySub_c
gcc SimpleInterfaceBytesTypePub.c -I../../../../include/ZRDDSCoreInterface -I../../../../include/CInterface -L../../../../lib -lZRDDSCzd -lpthread -lstdc++ -o SimpleInterfaceBytesTypePub_c
gcc SimpleInterfaceBytesTypeSub.c -I../../../../include/ZRDDSCoreInterface -I../../../../include/CInterface -L../../../../lib -lZRDDSCzd -lpthread -lstdc++ -o SimpleInterfaceBytesTypeSub_c
gcc SimpleInterfaceZeroCopyPub.c -I../../../../include/ZRDDSCoreInterface -I../../../../include/CInterface -L../../../../lib -lZRDDSCzd -lpthread -lstdc++ -o SimpleInterfaceZeroCopyPub_c
gcc SimpleInterfaceZeroCopySub.c -I../../../../include/ZRDDSCoreInterface -I../../../../include/CInterface -L../../../../lib -lZRDDSCzd -lpthread -lstdc++ -o SimpleInterfaceZeroCopySub_c