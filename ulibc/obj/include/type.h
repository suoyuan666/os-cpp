#pragma once
#include <stddef.h>
#include <stdarg.h>

#define _Int64 long

struct iovec {
  void *iov_base;
  size_t iov_len;
};

typedef struct _IO_FILE FILE;
typedef _Int64 off_t;
typedef long ssize_t;

typedef __builtin_va_list __isoc_va_list;

#define __BYTE_ORDER 1234
#define __LITTLE_ENDIAN 1234
#define __BIG_ENDIAN 4321

#define __LONG_MAX 0x7fffffffffffffffL

typedef unsigned mode_t;
typedef int pid_t;
typedef unsigned int uid_t;
typedef unsigned int gid_t;
