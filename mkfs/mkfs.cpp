#include <cassert>
#include <fstream>
#include <ios>
#include <iostream>

auto main(int argc, char* argv[]) -> int {
  if (argc < 2) {
    std::cerr << "usage: mkfs fs.img ...\n";
  }

  std::ifstream fs {"fs.img", std::ios_base::binary};
  
  return 0;
}