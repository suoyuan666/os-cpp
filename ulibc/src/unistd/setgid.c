#include <unistd.h>

#include "syscall.h"

int setgid(gid_t gid) { return syscall(SYS_setgid, gid); }
