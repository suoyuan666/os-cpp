#include "syscall.h"
#include <unistd.h>

int pipe(int fd[2])
{
	return syscall(SYS_pipe, fd);
}
