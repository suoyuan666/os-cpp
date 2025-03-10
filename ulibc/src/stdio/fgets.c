#include <stddef.h>
#include <string.h>

#include "stdio_impl.h"
#include "syscall.h"

// #define MIN(a, b) ((a) < (b) ? (a) : (b))

// char *fgets(char *restrict s, int n, FILE *restrict f) {
//   char *p = s;
//   unsigned char *z = NULL;
//   size_t k = 0;
//   int c = 0;
//
//   // FLOCK(f);
//
//   if (n <= 1) {
//     f->mode |= f->mode - 1;
//     // FUNLOCK(f);
//     if (n < 1) {
//       return 0;
//     }
//     *s = 0;
//     return s;
//   }
//   n--;
//
//   while (n) {
//     if (f->rpos != f->rend) {
//       z = memchr(f->rpos, '\n', f->rend - f->rpos);
//       k = z ? z - f->rpos + 1 : f->rend - f->rpos;
//       k = MIN(k, (size_t)n);
//       memcpy(p, f->rpos, k);
//       f->rpos += k;
//       p += k;
//       n -= k;
//       if (z || !n) break;
//     }
//     if ((c = getc_unlocked(f)) < 0) {
//       if (p == s || !feof(f)) s = 0;
//       break;
//     }
//     n--;
//     if ((*p++ = c) == '\n') break;
//   }
//   if (s) *p = 0;
//
//   // FUNLOCK(f);
//
//   return s;
// }

char *fgets(char *restrict s, int n, FILE *restrict f) {
  int i = 0;
  int cc = 0;
  char c = 0;

  (void)f;

  for (i = 0; i + 1 < n;) {
    cc = syscall(SYS_read, f->fd, &c, 1);
    if (cc < 1) {
      break;
    }
    s[i++] = c;
    if (c == '\n' || c == '\r') {
      break;
    }
  }
  s[i] = '\0';
  return s;
}

weak_alias(fgets, fgets_unlocked);
