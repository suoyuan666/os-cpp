#include <stddef.h>
#include <string.h>
#include "stdio_impl.h"

size_t __fwritex(const unsigned char *restrict s, size_t l, FILE *restrict f)
{
 return f->write(f, s, l);
}

size_t fwrite(const void *restrict src, size_t size, size_t nmemb, FILE *restrict f)
{
	size_t k, l = size*nmemb;
	if (!size) nmemb = 0;
	// FLOCK(f);
	k = __fwritex(src, l, f);
	// FUNLOCK(f);
	return k==l ? nmemb : k/size;
}
