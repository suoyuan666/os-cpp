#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "stdio_impl.h"
#include "syscall.h"

static const char xdigits[16] = {"0123456789ABCDEF"};

static void out(FILE *f, const char *s, size_t l) {
  if (f == stdout) {
    syscall(SYS_write, 1, s, l);
  }
  // if (!ferror(f)) {
  //   __fwritex((void *)s, l, f);
  // }
}

static void printint(FILE *restrict f, int xx, int base, int sgn) {
  char buf[16];
  int i = 0;
  int neg = 0;
  uint32_t x = 0;

  neg = 0;
  if (sgn && xx < 0) {
    neg = 1;
    x = -xx;
  } else {
    x = xx;
  }

  i = 0;
  do {
    buf[i++] = xdigits[x % base];
  } while ((x /= base) != 0);
  if (neg) buf[i++] = '-';

  while (--i >= 0) out(f, &buf[i], 1);
}

static void printptr(FILE *restrict f, uint64_t x) {
  out(f, "0", 1);
  out(f, "x", 1);
  for (uint64_t i = 0; i < (sizeof(uint64_t) * 2); i++, x <<= 4) {
    out(f, &xdigits[x >> (sizeof(uint64_t) * 8 - 4)], 1);
  }
}

int vfprintf(FILE *restrict f, const char *restrict fmt, va_list ap) {
  int state = 0;
  for (uint32_t i = 0; fmt[i]; ++i) {
    unsigned int c = fmt[i] & 0xFFU;
    if (state == 0) {
      if (c == '%') {
        state = '%';
      } else {
        out(f, (char *)&c, 1);
      }
    } else if (state == '%') {
      unsigned int c1 = 0;
      unsigned int c2 = 0;
      if (c) {
        c1 = fmt[i + 1] & 0xFFU;
      }
      if (c1) {
        c2 = fmt[i + 2] & 0xFFU;
      }
      if (c == 'd') {
        printint(f, va_arg(ap, int), 10, 1);
      } else if (c == 'l' && c1 == 'd') {
        printint(f, va_arg(ap, uint64_t), 10, 1);
        i += 1;
      } else if (c == 'l' && c1 == 'l' && c2 == 'd') {
        printint(f, va_arg(ap, uint64_t), 10, 1);
        i += 2;
      } else if (c == 'u') {
        printint(f, va_arg(ap, int), 10, 0);
      } else if (c == 'l' && c1 == 'u') {
        printint(f, va_arg(ap, uint64_t), 10, 0);
        i += 1;
      } else if (c == 'l' && c1 == 'l' && c2 == 'u') {
        printint(f, va_arg(ap, uint64_t), 10, 0);
        i += 2;
      } else if (c == 'x') {
        printint(f, va_arg(ap, int), 16, 0);
      } else if (c == 'l' && c1 == 'x') {
        printint(f, va_arg(ap, uint64_t), 16, 0);
        i += 1;
      } else if (c == 'l' && c1 == 'l' && c2 == 'x') {
        printint(f, va_arg(ap, uint64_t), 16, 0);
        i += 2;
      } else if (c == 'p') {
        printptr(f, va_arg(ap, uint64_t));
      } else if (c == 's') {
        char *s = va_arg(ap, char *);
        if (s == NULL) {
          s = "(null)";
        }
        for (; *s; s++) {
          out(f, s, 1);
        }
      } else if (c == '%') {
        out(f, "%", 1);
      } else {
        // Unknown % sequence.  Print it to draw attention.
        out(f, "%", 1);
        out(f, (char *)&c, 1);
      }
      state = 0;
    }
  }
  return 0;
}
