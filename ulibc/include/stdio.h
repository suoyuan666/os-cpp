#ifdef __cplusplus
extern "C" {
#endif

#include <features.h>
#include <stddef.h>
#include <stdint.h>

#include "type.h"

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
char *gets_s(char *, uint32_t);

size_t fread(void *__restrict, size_t, size_t, FILE *__restrict);
size_t fwrite(const void *__restrict, size_t, size_t, FILE *__restrict);

int getchar(void);
int ungetc(int, FILE *);

int fputc(int, FILE *);
int putc(int, FILE *);
int putchar(int);

int fputs(const char *__restrict, FILE *__restrict);
int puts(const char *);

extern hidden FILE __stdout_FILE;
extern hidden FILE __stdout_FILE;

extern FILE *const stdin;
extern FILE *const stdout;
extern FILE *const stderr;

#define stdin (stdin)
#define stdout (stdout)
#define stderr (stderr)

#define EOF (0)

#ifdef __cplusplus
}
#endif
