#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

int stat(const char *__restrict path, struct stat *__restrict buf) {
  int fd = open(path, O_RDONLY);
  if (fd < 0) {
    return -1;
  }
  int rs = fstat(fd, buf);
  close(fd);
  return rs;
}
