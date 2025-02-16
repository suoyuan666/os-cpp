#include <stdio.h>

#include "stdio_impl.h"

#define UNGET 8

#define F_PERM 1
#define F_NORD 4
#define F_NOWR 8
#define F_EOF 16
#define F_ERR 32
#define F_SVB 64
#define F_APP 128

static unsigned char buf[BUFSIZ + UNGET];

hidden FILE __stdout_FILE = {
    .buf = buf + UNGET,
    .buf_size = sizeof buf - UNGET,
    .fd = 1,
    .flags = F_PERM | F_NORD,
    .lbf = '\n',
    .write = __stdio_write,
    // .seek = __stdio_seek,
    // .close = __stdio_close,
    .lock = -1,
};
FILE *const stdout = &__stdout_FILE;
FILE *volatile __stdout_used = &__stdout_FILE;
