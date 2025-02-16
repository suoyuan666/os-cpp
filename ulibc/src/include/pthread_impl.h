#include <pthread_arch.h>

#define __pthread_self() ((pthread_t)__get_tp())
