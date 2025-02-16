#pragma once
#include <features.h>
#include <stddef.h>

#include "type.h"

#define FLOCK(f) int __need_unlock = ((f)->lock>=0 ? __lockfile((f)) : 0)
#define FUNLOCK(f) do { if (__need_unlock) __unlockfile((f)); } while (0)

#define F_PERM 1
#define F_NORD 4
#define F_NOWR 8
#define F_EOF 16
#define F_ERR 32
#define F_SVB 64
#define F_APP 128

struct _IO_FILE {
  unsigned flags;
  unsigned char *rpos, *rend;
  int (*close)(FILE *);
  unsigned char *wend, *wpos;
  unsigned char *mustbezero_1;
  unsigned char *wbase;
  size_t (*read)(FILE *, unsigned char *, size_t);
  size_t (*write)(FILE *, const unsigned char *, size_t);
  off_t (*seek)(FILE *, off_t, int);
  unsigned char *buf;
  size_t buf_size;
  FILE *prev, *next;
  int fd;
  int pipe_pid;
  long lockcount;
  int mode;
  volatile int lock;
  int lbf;
  void *cookie;
  off_t off;
  char *getln_buf;
  void *mustbezero_2;
  unsigned char *shend;
  off_t shlim, shcnt;
  FILE *prev_locked, *next_locked;
};

// hidden int __lockfile(FILE *);
// hidden void __unlockfile(FILE *);

hidden size_t __stdio_write(FILE *, const unsigned char *, size_t);

hidden int __toread(FILE *);
hidden int __towrite(FILE *);

hidden size_t __fwritex(const unsigned char *, size_t, FILE *);

#define ferror(f) ((f)->flags & F_ERR)
