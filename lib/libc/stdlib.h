#ifndef _STDLIB_H
#define _STDLIB_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RAND_MAX (0x7fffffff)

typedef struct { int quot, rem; } div_t;
typedef struct { long quot, rem; } ldiv_t;
typedef struct { long long quot, rem; } lldiv_t;

double atof(const char *s);
int atoi(const char *s);
long atol(const char *s);
double strtod(const char* str, char** endptr);
long strtol(const char *cp,char **endp, int base);
unsigned long strtoul(const char *cp,char **endp, int base);
void *calloc(size_t nitems, size_t size);
void free(void *ptr);
void *malloc(size_t size);
void *realloc(void *ptr, size_t size);
void abort(void);
void exit(int status);
void *bsearch(const void *key, const void *base, size_t nel, size_t width, int (*cmp)(const void *, const void *));
void qsort(void *base, size_t nel, size_t width, int (*cmp)(const void *, const void *));
int abs(int a);
div_t div(int num, int den);
long labs(long a);
ldiv_t ldiv(long num, long den);
long long llabs(long long a);
lldiv_t lldiv(long long num, long long den);
int rand(void);
void srand(unsigned s);

#ifdef __cplusplus
}
#endif

#endif
