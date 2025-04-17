#pragma once
#include "type.h"
#include <stdint.h>

int dup(int);
int dup2(int, int);
int dup3(int, int, int);
int pipe(int [2]);
int close(int);

ssize_t read(int, void *, size_t);
ssize_t write(int, const void *, size_t);

pid_t fork(void);
int execve(const char *, char *const [], char *const []);

void *sbrk(int);

int setuid(uid_t);
int setgid(gid_t);
