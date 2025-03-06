#include <fnctl.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

int getcmd(char *buf, int nbuf) {
  write(2, "$ ", 2);
  memset(buf, 0, nbuf);
  fgets(buf, nbuf, stdin);
  if (buf[0] == EOF) {
    return -1;
  }
  return 0;
}

int main(void) {
  int fd = 0;
  static char buf[100];
  char *argv[] = {"cat", "README.md", 0};

  while ((fd = open("console", O_RDWR)) >= 0) {
    if (fd >= 3) {
      close(fd);
      break;
    }
  }

  while (getcmd(buf, sizeof(buf)) >= 0) {
    if (fork() == 0) {
      execve("cat", argv, NULL);
    }
    wait(NULL);
  }
}
