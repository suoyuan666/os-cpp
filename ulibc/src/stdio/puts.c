#include <stdio.h>

#include "stdio_impl.h"

int puts(const char *s) {
  int r = 0;
  // FLOCK(stdout);
  r = -(fputs(s, stdout) < 0 || putc_unlocked('\n', stdout) < 0);
  // FUNLOCK(stdout);
  return r;
}
