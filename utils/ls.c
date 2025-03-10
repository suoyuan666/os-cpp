#include <dirent.h>
#include <fnctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

void ls(char *path);

int main(int argc, char *argv[]) {
  if (argc < 2) {
    ls(".");
    return 0;
  }
  for (int i = 1; i < argc; i++) {
    ls(argv[i]);
  }
  return 0;
}

char *fmtname(char *path) {
  static char buf[DIRSIZ + 1];
  char *p = NULL;

  for (p = path + strlen(path); p >= path && *p != '/'; p--);
  p++;

  if (strlen(p) >= DIRSIZ) {
    return p;
  }
  memmove(buf, p, strlen(p));
  memset(buf + strlen(p), ' ', DIRSIZ - strlen(p));
  return buf;
}

void ls(char *path) {
  int fd = open(path, O_RDONLY);
  if (fd < 0) {
    printf("ls: cannot open %s\n", path);
    exit(1);
  }

  struct stat st;
  if (fstat(fd, &st) < 0) {
    printf("ls: cannot stat %s\n", path);
    close(fd);
    exit(1);
  }

  char buf[512];
  char *p = NULL;
  struct dirent de;

  switch (st.type) {
    case T_DEVICE:
    case T_FILE:
      printf("%s %d %d %d\n", fmtname(path), st.type, st.ino, (int)st.size);
      break;

    case T_DIR:
      if (strlen(path) + 1 + DIRSIZ + 1 > sizeof buf) {
        printf("ls: path too long\n");
        break;
      }
      strcpy(buf, path);
      p = buf + strlen(buf);
      *p++ = '/';
      while (read(fd, &de, sizeof(de)) == sizeof(de)) {
        if (de.inum == 0) continue;
        memmove(p, de.name, DIRSIZ);
        p[DIRSIZ] = 0;
        if (stat(buf, &st) < 0) {
          printf("ls: cannot stat %s\n", buf);
          continue;
        }
        const char *name = fmtname(buf);
        printf("%s %d %d %d\n", name, st.type, st.ino, (int)st.size);
      }
      break;
  }
  close(fd);
}
