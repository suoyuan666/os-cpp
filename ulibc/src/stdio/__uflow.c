#include <stdio.h>

#include "stdio_impl.h"

int __uflow(FILE *f) {
  unsigned char c = 0;
  if (!__toread(f) && f->read(f, &c, 1) == 1 ) return c;
  return EOF;
}
