#include "syscall.h"

void _start()
{
  extern int main();
  main();
  syscall(SYS_exit, 0);
}
