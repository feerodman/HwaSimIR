#include "../Shared/Sha256File.h"
#include <iostream>

int main(int argc, char** argv) {
    if (argc != 3) return 2;
    const std::string actual = HwaHash::Sha256File(argv[1]);
    std::cout << actual << '\n';
    return actual == argv[2] ? 0 : 1;
}
