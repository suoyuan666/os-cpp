#include <stddef.h>
#include <stdint.h>

void *memset(void *s, int c, size_t n) {
    unsigned char *p = s;
    unsigned char value = (unsigned char)c;

    while (n--) {
        *p++ = value;
    }

    return s;
}
