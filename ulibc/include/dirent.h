#pragma once
#ifdef __cplusplus
extern "C" {
#endif
#include <stdint.h>

#define DIRSIZ 14

struct dirent {
  uint16_t inum;
  char name[DIRSIZ];
};

#ifdef __cplusplus
}
#endif
