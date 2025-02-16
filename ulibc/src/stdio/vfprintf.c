#include <ctype.h>
#include <limits.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "stdio_impl.h"

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

#define ALT_FORM (1U << ('#' - ' '))
#define ZERO_PAD (1U << ('0' - ' '))
#define LEFT_ADJ (1U << ('-' - ' '))
#define PAD_POS (1U << (' ' - ' '))
#define MARK_POS (1U << ('+' - ' '))
#define GROUPED (1U << ('\'' - ' '))

#define FLAGMASK (ALT_FORM | ZERO_PAD | LEFT_ADJ | PAD_POS | MARK_POS | GROUPED)

enum {
  BARE,
  LPRE,
  LLPRE,
  HPRE,
  HHPRE,
  BIGLPRE,
  ZTPRE,
  JPRE,
  STOP,
  PTR,
  INT,
  UINT,
  ULLONG,
  LONG,
  ULONG,
  SHORT,
  USHORT,
  CHAR,
  UCHAR,
  LLONG,
  SIZET,
  IMAX,
  UMAX,
  PDIFF,
  UIPTR,
  DBL,
  LDBL,
  NOARG,
  MAXSTATE
};

#define S(x) [(x) - 'A']

static const unsigned char states[]['z' - 'A' + 1] = {
    {
        /* 0: bare types */
        S('d') = INT,   S('i') = INT,  S('o') = UINT,  S('u') = UINT,
        S('x') = UINT,  S('X') = UINT, S('e') = DBL,   S('f') = DBL,
        S('g') = DBL,   S('a') = DBL,  S('E') = DBL,   S('F') = DBL,
        S('G') = DBL,   S('A') = DBL,  S('c') = INT,   S('C') = UINT,
        S('s') = PTR,   S('S') = PTR,  S('p') = UIPTR, S('n') = PTR,
        S('m') = NOARG, S('l') = LPRE, S('h') = HPRE,  S('L') = BIGLPRE,
        S('z') = ZTPRE, S('j') = JPRE, S('t') = ZTPRE,
    },
    {
        /* 1: l-prefixed */
        S('d') = LONG,
        S('i') = LONG,
        S('o') = ULONG,
        S('u') = ULONG,
        S('x') = ULONG,
        S('X') = ULONG,
        S('e') = DBL,
        S('f') = DBL,
        S('g') = DBL,
        S('a') = DBL,
        S('E') = DBL,
        S('F') = DBL,
        S('G') = DBL,
        S('A') = DBL,
        S('c') = UINT,
        S('s') = PTR,
        S('n') = PTR,
        S('l') = LLPRE,
    },
    {
        /* 2: ll-prefixed */
        S('d') = LLONG,
        S('i') = LLONG,
        S('o') = ULLONG,
        S('u') = ULLONG,
        S('x') = ULLONG,
        S('X') = ULLONG,
        S('n') = PTR,
    },
    {
        /* 3: h-prefixed */
        S('d') = SHORT,
        S('i') = SHORT,
        S('o') = USHORT,
        S('u') = USHORT,
        S('x') = USHORT,
        S('X') = USHORT,
        S('n') = PTR,
        S('h') = HHPRE,
    },
    {
        /* 4: hh-prefixed */
        S('d') = CHAR,
        S('i') = CHAR,
        S('o') = UCHAR,
        S('u') = UCHAR,
        S('x') = UCHAR,
        S('X') = UCHAR,
        S('n') = PTR,
    },
    {
        /* 5: L-prefixed */
        S('e') = LDBL,
        S('f') = LDBL,
        S('g') = LDBL,
        S('a') = LDBL,
        S('E') = LDBL,
        S('F') = LDBL,
        S('G') = LDBL,
        S('A') = LDBL,
        S('n') = PTR,
    },
    {
        /* 6: z- or t-prefixed (assumed to be same size) */
        S('d') = PDIFF,
        S('i') = PDIFF,
        S('o') = SIZET,
        S('u') = SIZET,
        S('x') = SIZET,
        S('X') = SIZET,
        S('n') = PTR,
    },
    {
        /* 7: j-prefixed */
        S('d') = IMAX,
        S('i') = IMAX,
        S('o') = UMAX,
        S('u') = UMAX,
        S('x') = UMAX,
        S('X') = UMAX,
        S('n') = PTR,
    }};

#define OOB(x) ((unsigned)(x) - 'A' > 'z' - 'A')

union arg {
  uintmax_t i;
  long double f;
  void *p;
};

static void pop_arg(union arg *arg, int type, va_list *ap) {
  switch (type) {
    case PTR:
      arg->p = va_arg(*ap, void *);
      break;
    case INT:
      arg->i = va_arg(*ap, int);
      break;
    case UINT:
      arg->i = va_arg(*ap, unsigned int);
      break;
    case LONG:
      arg->i = va_arg(*ap, long);
      break;
    case ULONG:
      arg->i = va_arg(*ap, unsigned long);
      break;
    case ULLONG:
      arg->i = va_arg(*ap, unsigned long long);
      break;
    case SHORT:
      arg->i = (short)va_arg(*ap, int);
      break;
    case USHORT:
      arg->i = (unsigned short)va_arg(*ap, int);
      break;
    case CHAR:
      arg->i = (signed char)va_arg(*ap, int);
      break;
    case UCHAR:
      arg->i = (unsigned char)va_arg(*ap, int);
      break;
    case LLONG:
      arg->i = va_arg(*ap, long long);
      break;
    case SIZET:
      arg->i = va_arg(*ap, size_t);
      break;
    case IMAX:
      arg->i = va_arg(*ap, intmax_t);
      break;
    case UMAX:
      arg->i = va_arg(*ap, uintmax_t);
      break;
    case PDIFF:
      arg->i = va_arg(*ap, ptrdiff_t);
      break;
    case UIPTR:
      arg->i = (uintptr_t)va_arg(*ap, void *);
      break;
    // case DBL:
    //   arg->f = va_arg(*ap, double);
    //   break;
    case LDBL:
      arg->f = va_arg(*ap, long double);
  }
}

static void out(FILE *f, const char *s, size_t l) {
  if (!ferror(f)) __fwritex((void *)s, l, f);
}

static void pad(FILE *f, char c, int w, int l, int fl) {
  char pad[256];
  if (fl & (LEFT_ADJ | ZERO_PAD) || l >= w) return;
  l = w - l;
  memset(pad, c, (uint32_t)l > sizeof pad ? sizeof pad : l);
  for (; (uint32_t)l >= sizeof pad; l -= sizeof pad) {
    out(f, pad, sizeof pad);
  }
  out(f, pad, l);
}

static const char xdigits[16] = {"0123456789ABCDEF"};

static char *fmt_x(uintmax_t x, char *s, int lower) {
  for (; x; x >>= 4) *--s = xdigits[(x & 15)] | lower;
  return s;
}

static char *fmt_o(uintmax_t x, char *s) {
  for (; x; x >>= 3) *--s = '0' + (x & 7);
  return s;
}

static char *fmt_u(uintmax_t x, char *s) {
  unsigned long y;
  for (; x > ULONG_MAX; x /= 10) *--s = '0' + x % 10;
  for (y = x; y; y /= 10) *--s = '0' + y % 10;
  return s;
}

static int printf_core(FILE *f, const char *fmt, va_list *ap, union arg *nl_arg,
                       int *nl_type) {
  char *a = NULL, *z = NULL, *s = (char *)fmt;
  unsigned l10n = 0, fl = 0;
  int w = 0, p = 0, xp = 0;
  union arg arg;
  int argpos = 0;
  unsigned st = 0, ps = 0;
  int cnt = 0, l = 0;
  size_t i = 0;
  char buf[sizeof(uintmax_t) * 3];
  const char *prefix = NULL;
  int t = 0, pl = 0;
  // wchar_t wc[2], *ws;
  // char mb[4];

  for (;;) {
    /* This error is only specified for snprintf, but since it's
     * unspecified for other forms, do the same. Stop immediately
     * on overflow; otherwise %n could produce wrong results. */
    if (l > INT_MAX - cnt) goto overflow;

    /* Update output count, end loop when fmt is exhausted */
    cnt += l;
    if (!*s) break;

    /* Handle literal text and %% format specifiers */
    for (a = s; *s && *s != '%'; s++);
    for (z = s; s[0] == '%' && s[1] == '%'; z++, s += 2);
    if (z - a > INT_MAX - cnt) goto overflow;
    l = z - a;
    if (f) out(f, a, l);
    if (l) continue;

    /* Format specifier state machine */
    st = 0;
    do {
      if (OOB(*s)) goto inval;
      ps = st;
      st = states[st] S(*s++);
    } while (st - 1 < STOP);
    if (!st) goto inval;

    /* Check validity of argument type (nl/normal) */
    if (st == NOARG) {
      if (argpos >= 0) goto inval;
    } else {
      if (argpos >= 0) {
        if (!f)
          nl_type[argpos] = st;
        else
          arg = nl_arg[argpos];
      } else if (f)
        pop_arg(&arg, st, ap);
      else
        return 0;
    }

    if (!f) continue;

    /* Do not process any new directives once in error state. */
    if (ferror(f)) return -1;

    z = buf + sizeof(buf);
    prefix = "-+   0X0x";
    pl = 0;
    t = s[-1];

    /* Transform ls,lc -> S,C */
    if (ps && (t & 15) == 3) t &= ~32;

    /* - and 0 flags are mutually exclusive */
    if (fl & LEFT_ADJ) fl &= ~ZERO_PAD;

    switch (t) {
      case 'n':
        switch (ps) {
          case BARE:
            *(int *)arg.p = cnt;
            break;
          case LPRE:
            *(long *)arg.p = cnt;
            break;
          case LLPRE:
            *(long long *)arg.p = cnt;
            break;
          case HPRE:
            *(unsigned short *)arg.p = cnt;
            break;
          case HHPRE:
            *(unsigned char *)arg.p = cnt;
            break;
          case ZTPRE:
            *(size_t *)arg.p = cnt;
            break;
          case JPRE:
            *(uintmax_t *)arg.p = cnt;
            break;
        }
        continue;
      case 'p':
        p = MAX((uint32_t)p, 2 * sizeof(void *));
        t = 'x';
        fl |= ALT_FORM;
      case 'x':
      case 'X':
        a = fmt_x(arg.i, z, t & 32);
        if (arg.i && (fl & ALT_FORM)) prefix += (t >> 4), pl = 2;
        if (0) {
          case 'o':
            a = fmt_o(arg.i, z);
            if ((fl & ALT_FORM) && p < z - a + 1) p = z - a + 1;
        }
        if (0) {
          case 'd':
          case 'i':
            pl = 1;
            if (arg.i > INTMAX_MAX) {
              arg.i = -arg.i;
            } else if (fl & MARK_POS) {
              prefix++;
            } else if (fl & PAD_POS) {
              prefix += 2;
            } else
              pl = 0;
          case 'u':
            a = fmt_u(arg.i, z);
        }
        if (xp && p < 0) goto overflow;
        if (xp) fl &= ~ZERO_PAD;
        if (!arg.i && !p) {
          a = z;
          break;
        }
        p = MAX(p, z - a + !arg.i);
        break;
      // narrow_c:
      case 'c':
        *(a = z - (p = 1)) = arg.i;
        fl &= ~ZERO_PAD;
        break;
      case 'm':
        // if (1) a = strerror(errno); else
      case 's':
        a = arg.p ? arg.p : "(null)";
        z = a + strnlen(a, p < 0 ? INT_MAX : p);
        if (p < 0 && *z) goto overflow;
        p = z - a;
        fl &= ~ZERO_PAD;
        break;
      // case 'e':
      // case 'f':
      // case 'g':
      // case 'a':
      // case 'E':
      // case 'F':
      // case 'G':
      // case 'A':
      //   if (xp && p < 0) goto overflow;
      //   l = fmt_fp(f, arg.f, w, p, fl, t);
      //   if (l < 0) goto overflow;
      //   continue;
    }

    if (p < z - a) p = z - a;
    if (p > INT_MAX - pl) goto overflow;
    if (w < pl + p) w = pl + p;
    if (w > INT_MAX - cnt) goto overflow;

    pad(f, ' ', w, pl + p, fl);
    out(f, prefix, pl);
    pad(f, '0', w, pl + p, fl ^ ZERO_PAD);
    pad(f, '0', p, z - a, 0);
    out(f, a, z - a);
    pad(f, ' ', w, pl + p, fl ^ LEFT_ADJ);

    l = w;
  }

  if (f) return cnt;
  if (!l10n) return 0;

  for (i = 1; i <= NL_ARGMAX && nl_type[i]; i++)
    pop_arg(nl_arg + i, nl_type[i], ap);
  for (; i <= NL_ARGMAX && !nl_type[i]; i++);
  if (i <= NL_ARGMAX) goto inval;
  return 1;

inval:
  // errno = EINVAL;
  return -1;
overflow:
  // errno = EOVERFLOW;
  return -1;
}

int vfprintf(FILE *restrict f, const char *restrict fmt, va_list ap) {
  va_list ap2 = NULL;
  int nl_type[NL_ARGMAX + 1] = {0};
  union arg nl_arg[NL_ARGMAX + 1];
  unsigned char internal_buf[80], *saved_buf = 0;
  int olderr = 0;
  int ret = 0;

  /* the copy allows passing va_list* even if va_list is an array */
  va_copy(ap2, ap);
  if (printf_core(0, fmt, &ap2, nl_arg, nl_type) < 0) {
    va_end(ap2);
    return -1;
  }

  // FLOCK(f);
  olderr = f->flags & F_ERR;
  f->flags &= ~F_ERR;
  if (!f->buf_size) {
    saved_buf = f->buf;
    f->buf = internal_buf;
    f->buf_size = sizeof internal_buf;
    f->wpos = 0;
    f->wbase = 0;
    f->wend = 0;
  }
  if (!f->wend && __towrite(f))
    ret = -1;
  else
    ret = printf_core(f, fmt, &ap2, nl_arg, nl_type);
  if (saved_buf) {
    f->write(f, 0, 0);
    if (!f->wpos) ret = -1;
    f->buf = saved_buf;
    f->buf_size = 0;
    f->wpos = f->wbase = f->wend = 0;
  }
  // if (ferror(f)) ret = -1;
  f->flags |= olderr;
  // FUNLOCK(f);
  va_end(ap2);
  return ret;
}
