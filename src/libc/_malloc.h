#ifndef __MALLOC_H
#define __MALLOC_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

size_t _init_memory(size_t max_pages);

#ifdef __cplusplus
}
#endif

#endif