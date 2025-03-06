#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define EXIT_FAILURE 1
#define EXIT_SUCCESS 0

_Noreturn void exit (int);
_Noreturn void _Exit (int);

#ifdef __cplusplus
}
#endif
