#include <stdint.h>

static inline uintptr_t __get_tp()
{
	uintptr_t tp = 0;
	__asm__ __volatile__("mv %0, tp" : "=r"(tp));
	return tp;
}
