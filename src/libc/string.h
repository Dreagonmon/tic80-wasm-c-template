#ifndef _STRING_H
#define _STRING_H

#include <libc_const.h>
#include <features.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void *memchr(const void *src, int c, size_t n);
int memcmp(const void *vl, const void *vr, size_t n);
void *memcpy(void *restrict dest, const void *restrict src, size_t n);
void *memmove(void *dest, const void *src, size_t n);
void *memset(void *dest, int c, size_t n);

char *strcat(char *restrict dest, const char *restrict src);
char *strncat(char *restrict d, const char *restrict s, size_t n);
char *strchr(const char *s, int c);
int strcmp(const char *l, const char *r);
int strncmp(const char *_l, const char *_r, size_t n);
char *strcpy(char *restrict d, const char *restrict s);
char *strncpy(char *restrict d, const char *restrict s, size_t n);
size_t strcspn(const char *s, const char *c);
size_t strlen(const char *s);
char *strpbrk(const char *s, const char *b);
char *strrchr(const char *s, int c);
size_t strspn(const char *s, const char *c);
char *strstr(const char *h, const char *n);
char *strtok(char *restrict s, const char *restrict sep);
char *strsep(char **str, const char *sep);

char *__strchrnul(const char *s, int c);

#ifdef __cplusplus
}
#endif

#endif
