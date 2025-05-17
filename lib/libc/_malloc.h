#ifndef __MALLOC_H
#define __MALLOC_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * init the malloc functions.
 * in tic80, try to alloc 4 pages of memory (64k per page)
 */
size_t _init_memory(size_t max_pages);

#ifdef __cplusplus
}
#endif

#endif