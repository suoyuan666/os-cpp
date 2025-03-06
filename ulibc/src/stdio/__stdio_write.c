#include <stddef.h>

#include "stdio_impl.h"
#include "syscall.h"

size_t __stdio_write(FILE *f, const unsigned char *buf, size_t len) {
  size_t cnt = 0;
  cnt = syscall(SYS_write, f->fd, buf, len);
  if (cnt == len) {
    f->wend = f->buf + f->buf_size;
    f->wpos = f->wbase = f->buf;
    return len;
  }
  if (cnt < 0) {
    f->wpos = f->wbase = f->wend = 0;
    f->flags |= F_ERR;
    return 0;
  }
  return len;
}
