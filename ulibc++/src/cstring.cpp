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

auto memmove(void* dest, const void* src, uint32_t count) -> void* {
  char* dst = static_cast<char*>(dest);
  const auto* vsrc = (char*)src;
  if (vsrc > dst) {
    while (count-- > 0) *dst++ = *vsrc++;
  } else {
    dst += count;
    vsrc += count;
    while (count-- > 0) *--dst = *--vsrc;
  }
  return dest;
}

auto strncpy(char* __restrict__ dest, const char* __restrict__ src,
             uint32_t count) -> char* {
  auto* s = dest;
  while (count-- > 0 && (*dest++ = *src++) != 0) {
  };
  while (count-- > 0) *dest++ = 0;
  return s;
}
}  // namespace std
