#include <iostream>
#include "block/Block.h"

int main() {
    Block b1(1, "2025, 03-11", {}, "0");

    std::cout << "Mining block..." << std::endl;
    b1.mineBlock(5); // difficulty 3

    std::cout << "Block Mined!" << std::endl;
    std::cout << "Hash: " << b1.hash << std::endl;

    return 0;
}
