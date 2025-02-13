#include <cstdint>
#include <cstring>

namespace std {
auto memset(void* dest, int ch, uint32_t count) noexcept -> void* {
  auto* cdst = static_cast<char*>(dest);
  for (uint32_t i = 0; i < count; ++i) {
    cdst[i] = ch;
  }
  return cdst;
}

auto memmove(void* dest, const void* src, uint32_t count) noexcept -> void* {
  auto* d = static_cast<unsigned char*>(dest);
  const auto* s = static_cast<const unsigned char*>(src);

  if (d == s || count == 0) {
    return dest;
  }

  const auto d_addr = reinterpret_cast<uintptr_t>(d);
  const auto s_addr = reinterpret_cast<uintptr_t>(s);

  if (d_addr < s_addr) {
    for (uint32_t i = 0; i < count; ++i) {
      d[i] = s[i];
    }
  } else {
    for (uint32_t i = count; i > 0; --i) {
      d[i - 1] = s[i - 1];
    }
  }

  return dest;
}

extern "C" auto memcpy(void* dest, const void* src, uint32_t count) -> void* {
  unsigned char* d = static_cast<unsigned char*>(dest);
  const unsigned char* s = static_cast<const unsigned char*>(src);
  while (count--) {
    *d++ = *s++;
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

auto strncmp(const char* lhs, const char* rhs, uint32_t count) -> int {
  while (count > 0 && *lhs != '\0' && *lhs != *rhs) {
    --count;
    ++lhs;
    ++rhs;
  }
  if (count == 0) {
    return 0;
  }
  return static_cast<unsigned char>(*lhs) - static_cast<unsigned char>(*rhs);
}

auto strlen( const char* str ) -> uint32_t {
  if (str == nullptr) {
        return 0;
    }

    const char* s = str;
    while (*s != '\0') {
        ++s;
    }
    return s - str;
}
}  // namespace std
