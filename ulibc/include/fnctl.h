#pragma once

#ifdef __cplusplus
extern "C" {
#endif

int open(const char *, int);

#define O_RDONLY  0x000
#define O_WRONLY  0x001
#define O_RDWR    0x002
#define O_CREATE  0x200
#define O_TRUNC   0x400

#ifdef __cplusplus
}
#endif
