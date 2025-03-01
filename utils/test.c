#include <fnctl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

int main() {
  if (open("console", O_RDWR) < 0) {
    mknod("console", 1, 0);
    open("console", O_RDWR);
  }
  dup(0);  // stdout
  dup(0);  // stderr
  write(1, "hello, world\n", 14);
  while (1) {
  };
  return 0;
}
