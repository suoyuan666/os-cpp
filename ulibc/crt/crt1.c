#include "syscall.h"

int main();

void _start()
{
  main();
  syscall(SYS_exit, 0);
}
