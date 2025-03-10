#pragma once
#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define T_DIR     1   // Directory
#define T_FILE    2   // File
#define T_DEVICE  3   // Device

struct stat {
  uint32_t dev;
  uint32_t ino;
  int16_t type;
  int16_t nlink;
  uint64_t size;
};

int mknod(const char *, uint32_t, uint64_t);
int stat(const char *__restrict, struct stat *__restrict);
int fstat(int, struct stat *);

#ifdef __cplusplus
}
#endif
