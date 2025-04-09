#include "stdio_impl.h"
#include "syscall.h"

size_t __stdio_read(FILE *f, unsigned char *buf, size_t len) {
  ssize_t cnt = 0;

  cnt = syscall(SYS_read, f->fd, buf, len);
  if (cnt <= 0) {
    f->flags |= cnt ? F_ERR : F_EOF;
    return 0;
  }
  f->rpos = f->buf;
  f->rend = f->buf + cnt;
  if (f->buf_size) {
    buf[len - 1] = *f->rpos++;
  }
  return len;
}
