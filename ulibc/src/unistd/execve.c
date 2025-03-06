#include "syscall.h"
int execve(const char *path, char *const argv[], char *const envp[])
{
  (void)envp;
	return syscall(SYS_exec, path, argv);
}
