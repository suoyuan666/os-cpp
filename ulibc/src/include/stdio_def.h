#pragma once
#include <stdio.h>

#undef stdin
#undef stdout
#undef stderr

extern hidden FILE __stdin_FILE;
extern hidden FILE __stdout_FILE;
extern hidden FILE __stderr_FILE;

#define stdin (&__stdin_FILE)
#define stdout (&__stdout_FILE)
#define stderr (&__stderr_FILE)
