#include <stddef.h>

#include "stdio_impl.h"
#include "syscall.h"
#include "type.h"

size_t __stdio_write(FILE *f, const unsigned char *buf, size_t len) {
  struct iovec iovs[2] = {{.iov_base = f->wbase, .iov_len = f->wpos - f->wbase},
                          {.iov_base = (void *)buf, .iov_len = len}};
  struct iovec *iov = iovs;
  size_t rem = iov[0].iov_len + iov[1].iov_len;
  int iovcnt = 2;
  size_t cnt = 0;
  for (;;) {
    cnt = syscall(SYS_write, f->fd, iov, iovcnt);
    if (cnt == rem) {
      f->wend = f->buf + f->buf_size;
      f->wpos = f->wbase = f->buf;
      return len;
    }
    if (cnt < 0) {
      f->wpos = f->wbase = f->wend = 0;
      f->flags |= F_ERR;
      return iovcnt == 2 ? 0 : len - iov[0].iov_len;
    }
    rem -= cnt;
    if (cnt > iov[0].iov_len) {
      cnt -= iov[0].iov_len;
      iov++;
      iovcnt--;
    }
    iov[0].iov_base = (char *)iov[0].iov_base + cnt;
    iov[0].iov_len -= cnt;
  }
}
