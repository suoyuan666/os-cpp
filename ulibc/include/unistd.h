#include "type.h"

int dup(int);
int pipe(int [2]);
int close(int);

ssize_t read(int, void *, size_t);
ssize_t write(int, const void *, size_t);

pid_t fork(void);
int execve(const char *, char *const [], char *const []);
