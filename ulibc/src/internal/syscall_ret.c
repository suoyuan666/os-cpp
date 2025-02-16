#include "syscall.h"

long __syscall_ret(unsigned long r)
{
	if (r > -4096UL) {
		return -1;
	}
	return r;
}
