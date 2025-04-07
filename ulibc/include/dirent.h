#pragma once
#ifdef __cplusplus
extern "C" {
#endif
#include <stdint.h>

#define DIRSIZ 14

struct dirent {
  uint16_t inum;
  uint32_t uid;
  uint32_t gid;
  unsigned char mask_user;
  unsigned char mask_group;
  unsigned char mas_other;
  char name[DIRSIZ];
};

#ifdef __cplusplus
}
#endif
