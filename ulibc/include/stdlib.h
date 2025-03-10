#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define EXIT_FAILURE 1
#define EXIT_SUCCESS 0

_Noreturn void exit (int);
_Noreturn void _Exit (int);

void *malloc (uint32_t);
void free (void *);

#ifdef __cplusplus
}
#endif
