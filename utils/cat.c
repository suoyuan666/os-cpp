#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#include "fnctl.h"

void cat(int fd);

int main(int argc, char *argv[]) {
  if (argc < 2) {
    cat(0);
    return 0;
  }

  for (int i = 1; i < argc; ++i) {
    auto fd = open(argv[i], O_RDONLY);
    if (fd < 0) {
      printf("cat: open file error\n");
    }
    cat(fd);
    close(fd);
  }
}

void cat(int fd) {
  ssize_t sz = 0;
  char buf[512];

  while ((sz = read(fd, buf, sizeof(buf))) > 0) {
    if (write(1, buf, sz) != sz) {
      printf("cat: write error\n");
      exit(1);
    }
  }
  if (sz < 0) {
    printf("cat: read error\n");
    exit(1);
  }
}
