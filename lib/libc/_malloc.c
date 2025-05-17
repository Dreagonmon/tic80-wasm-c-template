#include <stdlib.h>
#include <libc_const.h>
#include <_malloc.h>
#include <umm_malloc/umm_malloc_cfgport.h>
#include <umm_malloc/umm_malloc_cfg.h>
#include <umm_malloc/umm_malloc.h>
 
size_t __heap_size = MEM_PAGESIZE;
extern void *__heap_base;

size_t _init_memory(size_t max_pages) {
    // check current size
    int page_count = __builtin_wasm_memory_size(0);
    while (max_pages > (size_t) page_count) {
        int ret = __builtin_wasm_memory_grow(0, (max_pages - page_count));
        if (ret >= 0) {
            // success
            page_count = max_pages;
            break;
        }
        max_pages -= 1;
    }
    void * heap_pointer = (void *) &__heap_base;
    __heap_size = page_count * MEM_PAGESIZE - (size_t) heap_pointer;
    umm_init_heap(&__heap_base, __heap_size);
    return __heap_size;
}

void *calloc(size_t nitems, size_t size) {
    void *ptr = umm_calloc(nitems, size);
    INTEGRITY_CHECK();
    POISON_CHECK();
    return ptr;
}

void free(void *ptr) {
    umm_free(ptr);
    INTEGRITY_CHECK();
    POISON_CHECK();
}

void *malloc(size_t size) {
    void *ptr = umm_malloc(size);
    INTEGRITY_CHECK();
    POISON_CHECK();
    return ptr;
}

void *realloc(void *ptr, size_t size) {
    void *nptr = umm_realloc(ptr, size);
    INTEGRITY_CHECK();
    POISON_CHECK();
    return nptr;
}
