#include <unistd.h>

#include "syscall.h"

void *sbrk(int len) { return (void *)syscall(SYS_sbrk, len); }
