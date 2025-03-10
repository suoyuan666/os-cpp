#include <sys/stat.h>

#include "syscall.h"

int fstat(int fd, struct stat* stat) { return syscall(SYS_fstat, fd, stat); }
