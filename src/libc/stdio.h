#ifndef _STDIO_H
#define _STDIO_H

#ifdef __cplusplus
extern "C" {
#endif

int putchar(int ch);
int puts(const char *str);
int printf(char const* fmt, ... ) __attribute__ ((format (printf, 1, 2)));
int sprintf(char* buff, char const* fmt, ... ) __attribute__ ((format (printf, 2, 3)));

#ifdef __cplusplus
}
#endif

#endif
