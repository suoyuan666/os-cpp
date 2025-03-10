#include <stddef.h>

void *memmove(void *dest, const void *src, size_t n) {
  if (!dest || !src) {
    return NULL;
  }

  unsigned char *d = (unsigned char *)dest;
  const unsigned char *s = (const unsigned char *)src;

  if (d < s || d >= s + n) {
    while (n--) {
      *d++ = *s++;
    }
  } else if (d > s) {
    d += n;
    s += n;
    while (n--) {
      *--d = *--s;
    }
  }

  return dest;
}
