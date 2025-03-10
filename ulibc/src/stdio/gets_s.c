#include <stdint.h>
#include <stdio.h>

#include "syscall.h"

char *gets_s(char *buf, uint32_t n) {
  uint32_t i = 0;
  int cc = 0;
  char c = 0;

  for (i = 0; i + 1 < n;) {
    cc = syscall(SYS_read, 0, &c, 1);
    if (cc < 1) {
      break;
    }
    buf[i++] = c;
    if (c == '\n' || c == '\r') {
      break;
    }
  }
  buf[i] = '\0';
  return buf;
}
