#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>

int printf(const char *restrict fmt, ...) {
  int ret = 0;
  va_list ap = NULL;
  va_start(ap, fmt);
  ret = vfprintf(stdout, fmt, ap);
  va_end(ap);
  return ret;
}
