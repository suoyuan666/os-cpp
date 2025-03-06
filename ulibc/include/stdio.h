#ifdef __cplusplus
extern "C" {
#endif

#include "type.h"
#include <features.h>
#include <stddef.h>

#define BUFSIZ 1024

int printf(const char *__restrict, ...);
int fprintf(FILE *__restrict, const char *__restrict, ...);
int sprintf(char *__restrict, const char *__restrict, ...);
int snprintf(char *__restrict, size_t, const char *__restrict, ...);

int vprintf(const char *__restrict, __isoc_va_list);
int vfprintf(FILE *__restrict, const char *__restrict, __isoc_va_list);
int vsprintf(char *__restrict, const char *__restrict, __isoc_va_list);
int vsnprintf(char *__restrict, size_t, const char *__restrict, __isoc_va_list);

int fgetc(FILE *);
int getc(FILE *);

char *fgets(char *__restrict, int, FILE *__restrict);
char *gets(char *);

extern hidden FILE __stdout_FILE;
extern hidden FILE __stdout_FILE;

extern FILE *const stdin;
extern FILE *const stdout;
extern FILE *const stderr;

#define stdin  (stdin)
#define stdout (stdout)
#define stderr (stderr)

#define EOF (0)

#ifdef __cplusplus
}
#endif
