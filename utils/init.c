#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

int main() {
  char *argv[] = {"sh", nullptr};

  if (open("console", O_RDWR) < 0) {
    mknod("console", 1, 0);
    open("console", O_RDWR);
  }
  dup(0);  // stdout
  dup(0);  // stderr
  printf("\ninit process start\n\n");
  setuid(1000);
  setgid(1000);
  if (fork() == 0) {
    execve("/bin/sh", argv, nullptr);
  }
  while (1) {
  };
  return 0;
}
