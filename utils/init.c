#include <fnctl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

int main() {
  char *argv[] = {"sh", 0};

  if (open("console", O_RDWR) < 0) {
    mknod("console", 1, 0);
    open("console", O_RDWR);
  }
  dup(0);  // stdout
  dup(0);  // stderr
  printf("\ninit process start\n\n");
  if (fork() == 0) {
    execve("sh", argv, NULL);
  }
  while (1) {
  };
  return 0;
}
