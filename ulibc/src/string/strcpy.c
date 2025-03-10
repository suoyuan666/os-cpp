#include "type.h"

char* strcpy(char* __restrict dest, const char* __restrict src) {
  if (!dest || !src) {
    return NULL;
  }

  char* original = dest;
  while ((*dest++ = *src++)) {
  }

  return original;
}
