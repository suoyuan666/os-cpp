#include <cstddef>

#include "fmt"
#include "vm.h"

struct mem_header {
  size_t size;
};

inline auto get_header(void* ptr) -> mem_header* {
  return reinterpret_cast<mem_header*>(reinterpret_cast<char*>(ptr) -
                                       sizeof(mem_header));
}

auto operator new(size_t size) -> void* {
  if (size == 0) {
    size = 1;
  }


  auto opt_mem = vm::kalloc();
  if (!opt_mem.has_value()) {
    fmt::panic("kernel new operator: no memory");
  }
  void* memory = opt_mem.value();
  auto* header = static_cast<mem_header*>(memory);
  header->size = size;

  return static_cast<char*>(memory) + sizeof(mem_header);
}

void operator delete(void* ptr) noexcept {
  if (!ptr) {
    return;
  }

  mem_header* header = get_header(ptr);
  void* original_ptr = header;

  vm::kfree(original_ptr);
}

auto operator new[](size_t size) -> void* {
  return operator new(size);
}

void operator delete[](void* ptr) noexcept {
  operator delete(ptr);
}
