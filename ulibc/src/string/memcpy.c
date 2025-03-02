#include <stdint.h>
#include <string.h>

void *memcpy(void *restrict dest, const void *restrict src, size_t n) {
  if (dest == NULL || src == NULL) {
    return NULL;
  }

  unsigned char *d = (unsigned char *)dest;
  const unsigned char *s = (const unsigned char *)src;

  for (size_t i = 0; i < n; i++) {
    d[i] = s[i];
  }

  return dest;
}
