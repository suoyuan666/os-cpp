#pragma once

#define isalpha(a) (0 ? isalpha(a) : (((unsigned)(a)|32)-'a') < 26)
#define isdigit(a) (0 ? isdigit(a) : ((unsigned)(a)-'0') < 10)
#define islower(a) (0 ? islower(a) : ((unsigned)(a)-'a') < 26)
#define isupper(a) (0 ? isupper(a) : ((unsigned)(a)-'A') < 26)
#define isprint(a) (0 ? isprint(a) : ((unsigned)(a)-0x20) < 0x5f)
#define isgraph(a) (0 ? isgraph(a) : ((unsigned)(a)-0x21) < 0x5e)
#define isspace(a) __isspace(a)
