#include <dirent.h>
#include <fnctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstdint>

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
  char *p = nullptr;

  for (p = path + strlen(path); p >= path && *p != '/'; p--);
  p++;

  if (strlen(p) >= DIRSIZ) {
    return p;
  }
  memmove(buf, p, strlen(p));
  memset(buf + strlen(p), ' ', DIRSIZ - strlen(p));
  return buf;
}

void print_permissions(const struct dirent *de) {
  if (de->mask_user & (1U << 2U)) {
    printf("r");
  } else {
    printf(" ");
  }
  if (de->mask_user & (1U << 1U)) {
    printf("w");
  } else {
    printf(" ");
  }
  if (de->mask_user & 1U) {
    printf("x");
  } else {
    printf(" ");
  }
  printf(" ");

  if (de->mask_group & (1U << 2U)) {
    printf("r");
  } else {
    printf(" ");
  }
  if (de->mask_group & (1U << 1U)) {
    printf("w");
  } else {
    printf(" ");
  }
  if (de->mask_group & 1U) {
    printf("x");
  } else {
    printf(" ");
  }
  printf(" ");

  if (de->mas_other & (1U << 2U)) {
    printf("r");
  } else {
    printf(" ");
  }
  if (de->mas_other & (1U << 1U)) {
    printf("w");
  } else {
    printf(" ");
  }
  if (de->mas_other & 1U) {
    printf("x");
  } else {
    printf(" ");
  }
}

void print_id(const uint32_t id) {
  if (id < 10) {
    printf("   %d", id);
  } else if (id < 100) {
    printf("  %d", id);
  } else if (id < 1000) {
    printf(" %d", id);
  } else {
    printf("%d", id);
  }
}

void ls(char *path) {
  auto fd = open(path, O_RDONLY);
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
  char *p = nullptr;
  struct dirent de;

  switch (st.type) {
    case T_DEVICE:
    case T_FILE:
      printf(" ");
      print_id(st.uid);
      printf(" ");
      print_id(st.gid);
      printf(" %d %d %d %s\n", st.type, st.ino, (int)st.size, fmtname(path));
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
        if (de.inum == 0) {
          continue;
        }
        memmove(p, de.name, DIRSIZ);
        p[DIRSIZ] = 0;
        if (stat(buf, &st) < 0) {
          printf("ls: cannot stat %s\n", buf);
          continue;
        }
        const char *name = fmtname(buf);

        print_permissions(&de);
        printf(" ");
        print_id(st.uid);
        printf(" ");
        print_id(st.gid);
        printf(" %d %d %d %s\n", st.type, st.ino, (int)st.size, name);
      }
      break;
  }
  close(fd);
}
