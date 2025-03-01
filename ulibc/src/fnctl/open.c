#include <fnctl.h>
#include <stdarg.h>

#include "syscall.h"

int open(const char *filename, int flags) {
  int fd = syscall(SYS_open, filename, flags);
  return fd;
}
