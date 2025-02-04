#include <cstdint>
#include <cstring>

namespace std {
auto memset(void* dest, int ch, uint32_t count) -> void* {
  auto* cdst = static_cast<char*>(dest);
  for (uint32_t i = 0; i < count; ++i) {
    cdst[i] = ch;
  }
  return cdst;
}
}  // namespace std
