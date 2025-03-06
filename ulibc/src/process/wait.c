#include <sys/wait.h>
#include "syscall.h"

pid_t wait(int *status)
{
	return syscall(SYS_wait, status);
}
