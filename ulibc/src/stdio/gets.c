#include <stddef.h>
#include <stdio.h>

#include "stdio_def.h"
#include "stdio_impl.h"

char *gets(char *s) {
  size_t i = 0;
  int c = 0;
  // FLOCK(stdin);
  while (1) {
    c = __uflow(stdin);
    if (c == EOF || c == '\n' || c == '\r') {
      break;
    }
    s[i++] = c;
  }
  s[i] = 0;
  if (c != '\n' && (!feof(stdin) || !i)) s = 0;
  // FUNLOCK(stdin);
  return s;
}
