#include <stdint.h>

#include "syscall.h"

int mknod(const char *path, uint32_t mode, uint64_t dev) {
  return syscall(SYS_mknod, path, mode, dev);
}
