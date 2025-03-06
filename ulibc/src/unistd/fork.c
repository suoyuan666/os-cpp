#include <unistd.h>

#include "syscall.h"

pid_t fork(void) { return syscall(SYS_fork); }
