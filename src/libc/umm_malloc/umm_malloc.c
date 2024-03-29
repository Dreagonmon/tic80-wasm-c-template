/* ----------------------------------------------------------------------------
 * umm_malloc.c - a memory allocator for embedded systems (microcontrollers)
 *
 * See LICENSE for copyright notice
 * See README.md for acknowledgements and description of internals
 * ----------------------------------------------------------------------------
 *
 * R.Hempel 2007-09-22 - Original
 * R.Hempel 2008-12-11 - Added MIT License biolerplate
 *                     - realloc() now looks to see if previous block is free
 *                     - made common operations functions
 * R.Hempel 2009-03-02 - Added macros to disable tasking
 *                     - Added function to dump heap and check for valid free
 *                        pointer
 * R.Hempel 2009-03-09 - Changed name to umm_malloc to avoid conflicts with
 *                        the mm_malloc() library functions
 *                     - Added some test code to assimilate a free block
 *                        with the very block if possible. Complicated and
 *                        not worth the grief.
 * D.Frank 2014-04-02  - Fixed heap configuration when UMM_TEST_MAIN is NOT set,
 *                        added user-dependent configuration file umm_malloc_cfg.h
 * R.Hempel 2016-12-04 - Add support for Unity test framework
 *                     - Reorganize source files to avoid redundant content
 *                     - Move integrity and poison checking to separate file
 * R.Hempel 2017-12-29 - Fix bug in realloc when requesting a new block that
 *                        results in OOM error - see Issue 11
 * R.Hempel 2019-09-07 - Separate the malloc() and free() functionality into
 *                        wrappers that use critical section protection macros
 *                        and static core functions that assume they are
 *                        running in a protected con text. Thanks @devyte
 * R.Hempel 2020-01-07 - Add support for Fragmentation metric - See Issue 14
 * R.Hempel 2020-01-12 - Use explicitly sized values from stdint.h - See Issue 15
 * R.Hempel 2020-01-20 - Move metric functions back to umm_info - See Issue 29
 * R.Hempel 2020-02-01 - Macro functions are uppercased - See Issue 34
 * R.Hempel 2020-06-20 - Support alternate body size - See Issue 42
 * R.Hempel 2021-05-02 - Support explicit memory umm_init_heap() - See Issue 53
 * ----------------------------------------------------------------------------
 */

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "umm_malloc/umm_malloc_cfg.h"   // Override with umm_malloc_cfg_xxx.h
#include "umm_malloc/umm_malloc.h"

/* Use the default DBGLOG_LEVEL and DBGLOG_FUNCTION */

// #define DBGLOG_ENABLE

#define DBGLOG_LEVEL 0

#ifdef DBGLOG_ENABLE
    #include "dbglog/dbglog.h"
#endif

extern void *UMM_MALLOC_CFG_HEAP_ADDR;
extern uint32_t UMM_MALLOC_CFG_HEAP_SIZE;

/* ------------------------------------------------------------------------- */

UMM_H_ATTPACKPRE typedef struct umm_ptr_t {
    uint16_t next;
    uint16_t prev;
} UMM_H_ATTPACKSUF umm_ptr;

UMM_H_ATTPACKPRE typedef struct umm_block_t {
    union {
        umm_ptr used;
    } header;
    union {
        umm_ptr free;
        uint8_t data[UMM_BLOCK_BODY_SIZE - sizeof(struct umm_ptr_t)];
    } body;
} UMM_H_ATTPACKSUF umm_block;

#define UMM_FREELIST_MASK ((uint16_t)(0x8000))
#define UMM_BLOCKNO_MASK  ((uint16_t)(0x7FFF))

/* ------------------------------------------------------------------------- */

struct umm_heap_config {
    umm_block *pheap;
    size_t heap_size;
    uint16_t numblocks;
};

struct umm_heap_config umm_heap_current;
// struct umm_heap_config umm_heaps[UMM_NUM_HEAPS];

#define UMM_HEAP       (umm_heap_current.pheap)
#define UMM_HEAPSIZE   (umm_heap_current.heap_size)
#define UMM_NUMBLOCKS  (umm_heap_current.numblocks)

#define UMM_BLOCKSIZE  (sizeof(umm_block))
#define UMM_BLOCK_LAST (UMM_NUMBLOCKS - 1)

/* -------------------------------------------------------------------------
 * These macros evaluate to the address of the block and data respectively
 */

#define UMM_BLOCK(b)  (UMM_HEAP[b])
#define UMM_DATA(b)   (UMM_BLOCK(b).body.data)

/* -------------------------------------------------------------------------
 * These macros evaluate to the index of the block - NOT the address!!!
 */

#define UMM_NBLOCK(b) (UMM_BLOCK(b).header.used.next)
#define UMM_PBLOCK(b) (UMM_BLOCK(b).header.used.prev)
#define UMM_NFREE(b)  (UMM_BLOCK(b).body.free.next)
#define UMM_PFREE(b)  (UMM_BLOCK(b).body.free.prev)

/* ------------------------------------------------------------------------ */

static uint16_t umm_blocks(size_t size) {

    /*
     * The calculation of the block size is not too difficult, but there are
     * a few little things that we need to be mindful of.
     *
     * When a block removed from the free list, the space used by the free
     * pointers is available for data. That's what the first calculation
     * of size is doing.
     *
     * We don't check for the special case of (size == 0) here as this needs
     * special handling in the caller depending on context. For example when we
     * realloc() a block to size 0 it should simply be freed.
     *
     * We do NOT need to check for allocating more blocks than the heap can
     * possibly hold - the allocator figures this out for us.
     *
     * There are only two cases left to consider:
     *
     * 1. (size <= body)  Obviously this is just one block
     * 2. (blocks > (2^15)) This should return ((2^15)) to force a
     *                      failure when the allocator runs
     *
     * If the requested size is greater that 32677-2 blocks (max block index
     * minus the overhead of the top and bottom bookkeeping blocks) then we
     * will return an incorrectly truncated value when the result is cast to
     * a uint16_t.
     */

    if (size <= (sizeof(((umm_block *)0)->body))) {
        return 1;
    }

    /*
     * If it's for more than that, then we need to figure out the number of
     * additional whole blocks the size of an umm_block are required, so
     * reduce the size request by the number of bytes in the body of the
     * first block.
     */

    size -= (sizeof(((umm_block *)0)->body));

    /* NOTE WELL that we take advantage of the fact that INT16_MAX is the
     * number of blocks that we can index in 15 bits :-)
     *
     * The below expression looks wierd, but it's right. Assuming body
     * size of 4 bytes and a block size of 8 bytes:
     *
     * BYTES (BYTES-BODY) (BYTES-BODY-1)/BLOCKSIZE BLOCKS
     *     1        n/a                        n/a      1
     *     5          1                          0      2
     *    12          8                          0      2
     *    13          9                          1      3
     */

    size_t blocks = (2 + ((size-1) / (UMM_BLOCKSIZE)));

    if (blocks > (INT16_MAX)) {
        blocks = INT16_MAX;
    }

    return (uint16_t)blocks;
}

/* ------------------------------------------------------------------------ */
/*
 * Split the block `c` into two blocks: `c` and `c + blocks`.
 *
 * - `new_freemask` should be `0` if `c + blocks` used, or `UMM_FREELIST_MASK`
 *   otherwise.
 *
 * Note that free pointers are NOT modified by this function.
 */
static void umm_split_block(uint16_t c,
    uint16_t blocks,
    uint16_t new_freemask) {

    UMM_NBLOCK(c + blocks) = (UMM_NBLOCK(c) & UMM_BLOCKNO_MASK) | new_freemask;
    UMM_PBLOCK(c + blocks) = c;

    UMM_PBLOCK(UMM_NBLOCK(c) & UMM_BLOCKNO_MASK) = (c + blocks);
    UMM_NBLOCK(c) = (c + blocks);
}

/* ------------------------------------------------------------------------ */

static void umm_disconnect_from_free_list(uint16_t c) {
    /* Disconnect this block from the FREE list */

    UMM_NFREE(UMM_PFREE(c)) = UMM_NFREE(c);
    UMM_PFREE(UMM_NFREE(c)) = UMM_PFREE(c);

    /* And clear the free block indicator */

    UMM_NBLOCK(c) &= (~UMM_FREELIST_MASK);
}

/* ------------------------------------------------------------------------
 * The umm_assimilate_up() function does not assume that UMM_NBLOCK(c)
 * has the UMM_FREELIST_MASK bit set. It only assimilates up if the
 * next block is free.
 */

static void umm_assimilate_up(uint16_t c) {

    if (UMM_NBLOCK(UMM_NBLOCK(c)) & UMM_FREELIST_MASK) {

        UMM_FRAGMENTATION_METRIC_REMOVE(UMM_NBLOCK(c));

        /*
         * The next block is a free block, so assimilate up and remove it from
         * the free list
         */

        DBGLOG_DEBUG("Assimilate up to next block, which is FREE\n");

        /* Disconnect the next block from the FREE list */

        umm_disconnect_from_free_list(UMM_NBLOCK(c));

        /* Assimilate the next block with this one */

        UMM_PBLOCK(UMM_NBLOCK(UMM_NBLOCK(c)) & UMM_BLOCKNO_MASK) = c;
        UMM_NBLOCK(c) = UMM_NBLOCK(UMM_NBLOCK(c)) & UMM_BLOCKNO_MASK;
    }
}

/* ------------------------------------------------------------------------
 * The umm_assimilate_down() function assumes that UMM_NBLOCK(c) does NOT
 * have the UMM_FREELIST_MASK bit set. In other words, try to assimilate
 * up before assimilating down.
 */

static uint16_t umm_assimilate_down(uint16_t c, uint16_t freemask) {

    // We are going to assimilate down to the previous block because
    // it was free, so remove it from the fragmentation metric

    UMM_FRAGMENTATION_METRIC_REMOVE(UMM_PBLOCK(c));

    UMM_NBLOCK(UMM_PBLOCK(c)) = UMM_NBLOCK(c) | freemask;
    UMM_PBLOCK(UMM_NBLOCK(c)) = UMM_PBLOCK(c);

    if (freemask) {
        // We are going to free the entire assimilated block
        // so add it to the fragmentation metric. A good
        // compiler will optimize away the empty if statement
        // when UMM_INFO is not defined, so don't worry about
        // guarding it.

        UMM_FRAGMENTATION_METRIC_ADD(UMM_PBLOCK(c));
    }

    return UMM_PBLOCK(c);
}

/* ------------------------------------------------------------------------- */

void umm_init_heap(void *ptr, size_t size)
{
    /* init heap pointer and size, and memset it to 0 */
    UMM_HEAP = (umm_block *)ptr;
    UMM_HEAPSIZE = size;
    UMM_NUMBLOCKS = (UMM_HEAPSIZE / UMM_BLOCKSIZE);
    memset(UMM_HEAP, 0x00, UMM_HEAPSIZE);

    /* setup initial blank heap structure */
    UMM_FRAGMENTATION_METRIC_INIT();

    /* Set up umm_block[0], which just points to umm_block[1] */
    UMM_NBLOCK(0) = 1;
    UMM_NFREE(0) = 1;
    UMM_PFREE(0) = 1;

    /*
     * Now, we need to set the whole heap space as a huge free block. We should
     * not touch umm_block[0], since it's special: umm_block[0] is the head of
     * the free block list. It's a part of the heap invariant.
     *
     * See the detailed explanation at the beginning of the file.
     *
     * umm_block[1] has pointers:
     *
     * - next `umm_block`: the last one umm_block[n]
     * - prev `umm_block`: umm_block[0]
     *
     * Plus, it's a free `umm_block`, so we need to apply `UMM_FREELIST_MASK`
     *
     * And it's the last free block, so the next free block is 0 which marks
     * the end of the list. The previous block and free block pointer are 0
     * too, there is no need to initialize these values due to the init code
     * that memsets the entire umm_ space to 0.
     */
    UMM_NBLOCK(1) = UMM_BLOCK_LAST | UMM_FREELIST_MASK;

    /*
     * Last umm_block[n] has the next block index at 0, meaning it's
     * the end of the list, and the previous block is umm_block[1].
     *
     * The last block is a special block and can never be part of the
     * free list, so its pointers are left at 0 too.
     */

    UMM_PBLOCK(UMM_BLOCK_LAST) = 1;

// DBGLOG_FORCE(true, "nblock(0) %04x pblock(0) %04x nfree(0) %04x pfree(0) %04x\n", UMM_NBLOCK(0) & UMM_BLOCKNO_MASK, UMM_PBLOCK(0), UMM_NFREE(0), UMM_PFREE(0));
// DBGLOG_FORCE(true, "nblock(1) %04x pblock(1) %04x nfree(1) %04x pfree(1) %04x\n", UMM_NBLOCK(1) & UMM_BLOCKNO_MASK, UMM_PBLOCK(1), UMM_NFREE(1), UMM_PFREE(1));

}

void umm_init(void) {
    /* Initialize the heap from linker supplied values */

    umm_init_heap(UMM_MALLOC_CFG_HEAP_ADDR, UMM_MALLOC_CFG_HEAP_SIZE);
}

/* ------------------------------------------------------------------------
 * Must be called only from within critical sections guarded by
 * UMM_CRITICAL_ENTRY(id) and UMM_CRITICAL_EXIT(id).
 */

static void umm_free_core(void *ptr) {

    uint16_t c;

    /*
     * FIXME: At some point it might be a good idea to add a check to make sure
     *        that the pointer we're being asked to free up is actually within
     *        the umm_heap!
     *
     * NOTE:  See the new umm_info() function that you can use to see if a ptr is
     *        on the free list!
     */

    /* Figure out which block we're in. Note the use of truncated division... */

    c = (((uint8_t *)ptr) - (uint8_t *)(&(UMM_HEAP[0]))) / UMM_BLOCKSIZE;

    DBGLOG_DEBUG("Freeing block %6i\n", c);

    /* Now let's assimilate this block with the next one if possible. */

    umm_assimilate_up(c);

    /* Then assimilate with the previous block if possible */

    if (UMM_NBLOCK(UMM_PBLOCK(c)) & UMM_FREELIST_MASK) {

        DBGLOG_DEBUG("Assimilate down to previous block, which is FREE\n");

        c = umm_assimilate_down(c, UMM_FREELIST_MASK);
    } else {
        /*
         * The previous block is not a free block, so add this one to the head
         * of the free list
         */
        UMM_FRAGMENTATION_METRIC_ADD(c);

        DBGLOG_DEBUG("Just add to head of free list\n");

        UMM_PFREE(UMM_NFREE(0)) = c;
        UMM_NFREE(c) = UMM_NFREE(0);
        UMM_PFREE(c)            = 0;
        UMM_NFREE(0) = c;

        UMM_NBLOCK(c) |= UMM_FREELIST_MASK;
    }
}

/* ------------------------------------------------------------------------ */

void umm_free(void *ptr) {
    UMM_CRITICAL_DECL(id_free);

    UMM_CHECK_INITIALIZED();

    /* If we're being asked to free a NULL pointer, well that's just silly! */

    if ((void *)0 == ptr) {
        DBGLOG_DEBUG("free a null pointer -> do nothing\n");

        return;
    }

    /* Free the memory withing a protected critical section */

    UMM_CRITICAL_ENTRY(id_free);

    umm_free_core(ptr);

    UMM_CRITICAL_EXIT(id_free);
}

/* ------------------------------------------------------------------------
 * Must be called only from within critical sections guarded by
 * UMM_CRITICAL_ENTRY(id) and UMM_CRITICAL_EXIT(id).
 */

static void *umm_malloc_core(size_t size) {
    uint16_t blocks;
    uint16_t blockSize = 0;

    uint16_t bestSize;
    uint16_t bestBlock;

    uint16_t cf;

    blocks = umm_blocks(size);

    /*
     * Now we can scan through the free list until we find a space that's big
     * enough to hold the number of blocks we need.
     *
     * This part may be customized to be a best-fit, worst-fit, or first-fit
     * algorithm
     */

    cf = UMM_NFREE(0);

    bestBlock = UMM_NFREE(0);
    bestSize = 0x7FFF;

    while (cf) {
        blockSize = (UMM_NBLOCK(cf) & UMM_BLOCKNO_MASK) - cf;

        DBGLOG_TRACE("Looking at block %6i size %6i\n", cf, blockSize);

        #if defined UMM_BEST_FIT
        if ((blockSize >= blocks) && (blockSize < bestSize)) {
            bestBlock = cf;
            bestSize = blockSize;
        }
        #elif defined UMM_FIRST_FIT
        /* This is the first block that fits! */
        if ((blockSize >= blocks)) {
            break;
        }
        #else
        #error "No UMM_*_FIT is defined - check umm_malloc_cfg.h"
        #endif

        cf = UMM_NFREE(cf);
    }

    if (0x7FFF != bestSize) {
        cf = bestBlock;
        blockSize = bestSize;
    }

    if (UMM_NBLOCK(cf) & UMM_BLOCKNO_MASK && blockSize >= blocks) {

        UMM_FRAGMENTATION_METRIC_REMOVE(cf);

        /*
         * This is an existing block in the memory heap, we just need to split off
         * what we need, unlink it from the free list and mark it as in use, and
         * link the rest of the block back into the freelist as if it was a new
         * block on the free list...
         */

        if (blockSize == blocks) {
            /* It's an exact fit and we don't neet to split off a block. */
            DBGLOG_DEBUG("Allocating %6i blocks starting at %6i - exact\n", blocks, cf);

            /* Disconnect this block from the FREE list */

            umm_disconnect_from_free_list(cf);
        } else {

            /* It's not an exact fit and we need to split off a block. */
            DBGLOG_DEBUG("Allocating %6i blocks starting at %6i - existing\n", blocks, cf);

            /*
             * split current free block `cf` into two blocks. The first one will be
             * returned to user, so it's not free, and the second one will be free.
             */
            umm_split_block(cf, blocks, UMM_FREELIST_MASK /*new block is free*/);

            UMM_FRAGMENTATION_METRIC_ADD(UMM_NBLOCK(cf));

            /*
             * `umm_split_block()` does not update the free pointers (it affects
             * only free flags), but effectively we've just moved beginning of the
             * free block from `cf` to `cf + blocks`. So we have to adjust pointers
             * to and from adjacent free blocks.
             */

            /* previous free block */
            UMM_NFREE(UMM_PFREE(cf)) = cf + blocks;
            UMM_PFREE(cf + blocks) = UMM_PFREE(cf);

            /* next free block */
            UMM_PFREE(UMM_NFREE(cf)) = cf + blocks;
            UMM_NFREE(cf + blocks) = UMM_NFREE(cf);
        }

    } else {
        /* Out of memory */

        DBGLOG_DEBUG("Can't allocate %5i blocks\n", blocks);

        return (void *)NULL;
    }

    return (void *)&UMM_DATA(cf);
}

/* ------------------------------------------------------------------------ */

void *umm_malloc(size_t size) {
    UMM_CRITICAL_DECL(id_malloc);

    void *ptr = NULL;

    UMM_CHECK_INITIALIZED();

    /*
     * the very first thing we do is figure out if we're being asked to allocate
     * a size of 0 - and if we are we'll simply return a null pointer. if not
     * then reduce the size by 1 byte so that the subsequent calculations on
     * the number of blocks to allocate are easier...
     */

    if (0 == size) {
        DBGLOG_DEBUG("malloc a block of 0 bytes -> do nothing\n");

        return ptr;
    }

    /* Allocate the memory withing a protected critical section */

    UMM_CRITICAL_ENTRY(id_malloc);

    ptr = umm_malloc_core(size);

    UMM_CRITICAL_EXIT(id_malloc);

    return ptr;
}

/* ------------------------------------------------------------------------ */

void *umm_realloc(void *ptr, size_t size) {
    UMM_CRITICAL_DECL(id_realloc);

    uint16_t blocks;
    uint16_t blockSize;
    uint16_t prevBlockSize = 0;
    uint16_t nextBlockSize = 0;

    uint16_t c;

    size_t curSize;

    UMM_CHECK_INITIALIZED();

    /*
     * This code looks after the case of a NULL value for ptr. The ANSI C
     * standard says that if ptr is NULL and size is non-zero, then we've
     * got to work the same a malloc(). If size is also 0, then our version
     * of malloc() returns a NULL pointer, which is OK as far as the ANSI C
     * standard is concerned.
     */

    if (((void *)NULL == ptr)) {
        DBGLOG_DEBUG("realloc the NULL pointer - call malloc()\n");

        return umm_malloc(size);
    }

    /*
     * Now we're sure that we have a non_NULL ptr, but we're not sure what
     * we should do with it. If the size is 0, then the ANSI C standard says that
     * we should operate the same as free.
     */

    if (0 == size) {
        DBGLOG_DEBUG("realloc to 0 size, just free the block\n");

        umm_free(ptr);

        return (void *)NULL;
    }

    /*
     * Otherwise we need to actually do a reallocation. A naiive approach
     * would be to malloc() a new block of the correct size, copy the old data
     * to the new block, and then free the old block.
     *
     * While this will work, we end up doing a lot of possibly unnecessary
     * copying. So first, let's figure out how many blocks we'll need.
     */

    blocks = umm_blocks(size);

    /* Figure out which block we're in. Note the use of truncated division... */

    c = (((uint8_t *)ptr) - (uint8_t *)(&(UMM_HEAP[0]))) / UMM_BLOCKSIZE;

    /* Figure out how big this block is ... the free bit is not set :-) */

    blockSize = (UMM_NBLOCK(c) - c);

    /* Figure out how many bytes are in this block */

    curSize = (blockSize * UMM_BLOCKSIZE) - (sizeof(((umm_block *)0)->header));

    /* Protect the critical section... */
    UMM_CRITICAL_ENTRY(id_realloc);

    /* Now figure out if the previous and/or next blocks are free as well as
     * their sizes - this will help us to minimize special code later when we
     * decide if it's possible to use the adjacent blocks.
     *
     * We set prevBlockSize and nextBlockSize to non-zero values ONLY if they
     * are free!
     */

    if ((UMM_NBLOCK(UMM_NBLOCK(c)) & UMM_FREELIST_MASK)) {
        nextBlockSize = (UMM_NBLOCK(UMM_NBLOCK(c)) & UMM_BLOCKNO_MASK) - UMM_NBLOCK(c);
    }

    if ((UMM_NBLOCK(UMM_PBLOCK(c)) & UMM_FREELIST_MASK)) {
        prevBlockSize = (c - UMM_PBLOCK(c));
    }

    DBGLOG_DEBUG("realloc blocks %i blockSize %i nextBlockSize %i prevBlockSize %i\n", blocks, blockSize, nextBlockSize, prevBlockSize);

    /*
     * Ok, now that we're here we know how many blocks we want and the current
     * blockSize. The prevBlockSize and nextBlockSize are set and we can figure
     * out the best strategy for the new allocation as follows:
     *
     * 1. If the new block is the same size or smaller than the current block do
     *    nothing.
     * 2. If the next block is free and adding it to the current block gives us
     *    EXACTLY enough memory, assimilate the next block. This avoids unwanted
     *    fragmentation of free memory.
     *
     * The following cases may be better handled with memory copies to reduce
     * fragmentation
     *
     * 3. If the previous block is NOT free and the next block is free and
     *    adding it to the current block gives us enough memory, assimilate
     *    the next block. This may introduce a bit of fragmentation.
     * 4. If the prev block is free and adding it to the current block gives us
     *    enough memory, remove the previous block from the free list, assimilate
     *    it, copy to the new block.
     * 5. If the prev and next blocks are free and adding them to the current
     *    block gives us enough memory, assimilate the next block, remove the
     *    previous block from the free list, assimilate it, copy to the new block.
     * 6. Otherwise try to allocate an entirely new block of memory. If the
     *    allocation works free the old block and return the new pointer. If
     *    the allocation fails, return NULL and leave the old block intact.
     *
     * TODO: Add some conditional code to optimise for less fragmentation
     *       by simply allocating new memory if we need to copy anyways.
     *
     * All that's left to do is decide if the fit was exact or not. If the fit
     * was not exact, then split the memory block so that we use only the requested
     * number of blocks and add what's left to the free list.
     */

    //  Case 1 - block is same size or smaller
    if (blockSize >= blocks) {
        DBGLOG_DEBUG("realloc the same or smaller size block - %i, do nothing\n", blocks);
        /* This space intentionally left blank */

        //  Case 2 - block + next block fits EXACTLY
    } else if ((blockSize + nextBlockSize) == blocks) {
        DBGLOG_DEBUG("exact realloc using next block - %i\n", blocks);
        umm_assimilate_up(c);
        blockSize += nextBlockSize;

        //  Case 3 - prev block NOT free and block + next block fits
    } else if ((0 == prevBlockSize) && (blockSize + nextBlockSize) >= blocks) {
        DBGLOG_DEBUG("realloc using next block - %i\n", blocks);
        umm_assimilate_up(c);
        blockSize += nextBlockSize;

        //  Case 4 - prev block + block fits
    } else if ((prevBlockSize + blockSize) >= blocks) {
        DBGLOG_DEBUG("realloc using prev block - %i\n", blocks);
        umm_disconnect_from_free_list(UMM_PBLOCK(c));
        c = umm_assimilate_down(c, 0);
        memmove((void *)&UMM_DATA(c), ptr, curSize);
        ptr = (void *)&UMM_DATA(c);
        blockSize += prevBlockSize;

        //  Case 5 - prev block + block + next block fits
    } else if ((prevBlockSize + blockSize + nextBlockSize) >= blocks) {
        DBGLOG_DEBUG("realloc using prev and next block - %i\n", blocks);
        umm_assimilate_up(c);
        umm_disconnect_from_free_list(UMM_PBLOCK(c));
        c = umm_assimilate_down(c, 0);
        memmove((void *)&UMM_DATA(c), ptr, curSize);
        ptr = (void *)&UMM_DATA(c);
        blockSize += (prevBlockSize + nextBlockSize);

        //  Case 6 - default is we need to realloc a new block
    } else {
        DBGLOG_DEBUG("realloc a completely new block %i\n", blocks);
        void *oldptr = ptr;
        if ((ptr = umm_malloc_core(size))) {
            DBGLOG_DEBUG("realloc %i to a bigger block %i, copy, and free the old\n", blockSize, blocks);
            memcpy(ptr, oldptr, curSize);
            umm_free_core(oldptr);
        } else {
            DBGLOG_DEBUG("realloc %i to a bigger block %i failed - return NULL and leave the old block!\n", blockSize, blocks);
            /* This space intentionally left blnk */
        }
        blockSize = blocks;
    }

    /* Now all we need to do is figure out if the block fit exactly or if we
     * need to split and free ...
     */

    if (blockSize > blocks) {
        DBGLOG_DEBUG("split and free %i blocks from %i\n", blocks, blockSize);
        umm_split_block(c, blocks, 0);
        umm_free_core((void *)&UMM_DATA(c + blocks));
    }

    /* Release the critical section... */
    UMM_CRITICAL_EXIT(id_realloc);

    return ptr;
}

/* ------------------------------------------------------------------------ */

void *umm_calloc(size_t num, size_t item_size) {
    void *ret;

    ret = umm_malloc((size_t)(item_size * num));

    if (ret) {
        memset(ret, 0x00, (size_t)(item_size * num));
    }

    return ret;
}
/* ------------------------------------------------------------------------ */

/* -------------------------------------------------------------------------
 * There are additional files that may be included here - normally it's
 * not a good idea to include .c files but in this case it keeps the
 * main umm_malloc file clear and prevents issues with exposing internal
 * data structures to other programs.
 * -------------------------------------------------------------------------
 */

// #include "umm_integrity.c"
// #include "umm_poison.c"
// #include "umm_info.c"

/* add to the end of this file below */
/* ------------------------------------------------------------------------ */

// UMM_INFO

/* ------------------------------------------------------------------------ */
#ifdef UMM_INFO

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>

#include <math.h>

/* ----------------------------------------------------------------------------
 * One of the coolest things about this little library is that it's VERY
 * easy to get debug information about the memory heap by simply iterating
 * through all of the memory blocks.
 *
 * As you go through all the blocks, you can check to see if it's a free
 * block by looking at the high order bit of the next block index. You can
 * also see how big the block is by subtracting the next block index from
 * the current block number.
 *
 * The umm_info function does all of that and makes the results available
 * in the ummHeapInfo structure.
 * ----------------------------------------------------------------------------
 */

UMM_HEAP_INFO ummHeapInfo;

void compute_usage_metric(void)
{
    if (0 == ummHeapInfo.freeBlocks) {
        ummHeapInfo.usage_metric = -1;        // No free blocks!
    } else {
        ummHeapInfo.usage_metric = (int)((ummHeapInfo.usedBlocks * 100) / (ummHeapInfo.freeBlocks));
    }
}

void compute_fragmentation_metric(void)
{
    if (0 == ummHeapInfo.freeBlocks) {
        ummHeapInfo.fragmentation_metric = 0; // No free blocks ... so no fragmentation either!
    } else {
        ummHeapInfo.fragmentation_metric = 100 - (((uint32_t)(sqrtf(ummHeapInfo.freeBlocksSquared)) * 100) / (ummHeapInfo.freeBlocks));
    }
}

void *umm_info(void *ptr, __attribute__((unused)) bool force) {
    uint16_t blockNo = 0;

    UMM_CRITICAL_DECL(id_info);

    UMM_CHECK_INITIALIZED();

    /* Protect the critical section... */
    UMM_CRITICAL_ENTRY(id_info);

    /*
     * Clear out all of the entries in the ummHeapInfo structure before doing
     * any calculations..
     */
    memset(&ummHeapInfo, 0, sizeof(ummHeapInfo));

    DBGLOG_FORCE(force, "\n");
    DBGLOG_FORCE(force, "+----------+-------+--------+--------+-------+--------+--------+\n");
    DBGLOG_FORCE(force, "|0x%08x|B %5i|NB %5i|PB %5i|Z %5i|NF %5i|PF %5i|\n",
        DBGLOG_32_BIT_PTR(&UMM_BLOCK(blockNo)),
        blockNo,
        UMM_NBLOCK(blockNo) & UMM_BLOCKNO_MASK,
        UMM_PBLOCK(blockNo),
        (UMM_NBLOCK(blockNo) & UMM_BLOCKNO_MASK) - blockNo,
        UMM_NFREE(blockNo),
        UMM_PFREE(blockNo));

    /*
     * Now loop through the block lists, and keep track of the number and size
     * of used and free blocks. The terminating condition is an nb pointer with
     * a value of zero...
     */

    blockNo = UMM_NBLOCK(blockNo) & UMM_BLOCKNO_MASK;

    while (UMM_NBLOCK(blockNo) & UMM_BLOCKNO_MASK) {
        size_t curBlocks = (UMM_NBLOCK(blockNo) & UMM_BLOCKNO_MASK) - blockNo;

        ++ummHeapInfo.totalEntries;
        ummHeapInfo.totalBlocks += curBlocks;

        /* Is this a free block? */

        if (UMM_NBLOCK(blockNo) & UMM_FREELIST_MASK) {
            ++ummHeapInfo.freeEntries;
            ummHeapInfo.freeBlocks += curBlocks;
            ummHeapInfo.freeBlocksSquared += (curBlocks * curBlocks);

            if (ummHeapInfo.maxFreeContiguousBlocks < curBlocks) {
                ummHeapInfo.maxFreeContiguousBlocks = curBlocks;
            }

            DBGLOG_FORCE(force, "|0x%08x|B %5i|NB %5i|PB %5i|Z %5u|NF %5i|PF %5i|\n",
                DBGLOG_32_BIT_PTR(&UMM_BLOCK(blockNo)),
                blockNo,
                UMM_NBLOCK(blockNo) & UMM_BLOCKNO_MASK,
                UMM_PBLOCK(blockNo),
                (uint16_t)curBlocks,
                UMM_NFREE(blockNo),
                UMM_PFREE(blockNo));

            /* Does this block address match the ptr we may be trying to free? */

            if (ptr == &UMM_BLOCK(blockNo)) {

                /* Release the critical section... */
                UMM_CRITICAL_EXIT(id_info);

                return ptr;
            }
        } else {
            ++ummHeapInfo.usedEntries;
            ummHeapInfo.usedBlocks += curBlocks;

            DBGLOG_FORCE(force, "|0x%08x|B %5i|NB %5i|PB %5i|Z %5u|                 |\n",
                DBGLOG_32_BIT_PTR(&UMM_BLOCK(blockNo)),
                blockNo,
                UMM_NBLOCK(blockNo) & UMM_BLOCKNO_MASK,
                UMM_PBLOCK(blockNo),
                (uint16_t)curBlocks);
        }

        blockNo = UMM_NBLOCK(blockNo) & UMM_BLOCKNO_MASK;
    }

    /*
     * The very last block is used as a placeholder to indicate that
     * there are no more blocks in the heap, so it cannot be used
     * for anything - at the same time, the size of this block must
     * ALWAYS be exactly 1 !
     */

    DBGLOG_FORCE(force, "|0x%08x|B %5i|NB %5i|PB %5i|Z %5i|NF %5i|PF %5i|\n",
        DBGLOG_32_BIT_PTR(&UMM_BLOCK(blockNo)),
        blockNo,
        UMM_NBLOCK(blockNo) & UMM_BLOCKNO_MASK,
        UMM_PBLOCK(blockNo),
        UMM_NUMBLOCKS - blockNo,
        UMM_NFREE(blockNo),
        UMM_PFREE(blockNo));

    DBGLOG_FORCE(force, "+----------+-------+--------+--------+-------+--------+--------+\n");

    DBGLOG_FORCE(force, "Total Entries %5i    Used Entries %5i    Free Entries %5i\n",
        ummHeapInfo.totalEntries,
        ummHeapInfo.usedEntries,
        ummHeapInfo.freeEntries);

    DBGLOG_FORCE(force, "Total Blocks  %5i    Used Blocks  %5i    Free Blocks  %5i\n",
        ummHeapInfo.totalBlocks,
        ummHeapInfo.usedBlocks,
        ummHeapInfo.freeBlocks);

    DBGLOG_FORCE(force, "+--------------------------------------------------------------+\n");

    compute_usage_metric();
    DBGLOG_FORCE(force, "Usage Metric:               %5i\n", ummHeapInfo.usage_metric);

    compute_fragmentation_metric();
    DBGLOG_FORCE(force, "Fragmentation Metric:       %5i\n", ummHeapInfo.fragmentation_metric);

    DBGLOG_FORCE(force, "+--------------------------------------------------------------+\n");

    /* Release the critical section... */
    UMM_CRITICAL_EXIT(id_info);

    return NULL;
}

/* ------------------------------------------------------------------------ */

size_t umm_free_heap_size(void) {
    #ifndef UMM_INLINE_METRICS
    umm_info(NULL, false);
    #endif
    return (size_t)ummHeapInfo.freeBlocks * UMM_BLOCKSIZE;
}

size_t umm_max_free_block_size(void) {
    umm_info(NULL, false);
    return ummHeapInfo.maxFreeContiguousBlocks * sizeof(umm_block);
}

int umm_usage_metric(void) {
    #ifdef UMM_INLINE_METRICS
    compute_usage_metric();
    #else
    umm_info(NULL, false);
    #endif
    DBGLOG_DEBUG("usedBlocks %i totalBlocks %i\n", ummHeapInfo.usedBlocks, ummHeapInfo.totalBlocks);

    return ummHeapInfo.usage_metric;
}

int umm_fragmentation_metric(void) {
    #ifdef UMM_INLINE_METRICS
    compute_fragmentation_metric();
    #else
    umm_info(NULL, false);
    #endif
    DBGLOG_DEBUG("freeBlocks %i freeBlocksSquared %i\n", ummHeapInfo.freeBlocks, ummHeapInfo.freeBlocksSquared);

    return ummHeapInfo.fragmentation_metric;
}

#ifdef UMM_INLINE_METRICS
static void umm_fragmentation_metric_init(void) {
    ummHeapInfo.freeBlocks = UMM_NUMBLOCKS - 2;
    ummHeapInfo.freeBlocksSquared = ummHeapInfo.freeBlocks * ummHeapInfo.freeBlocks;
}

static void umm_fragmentation_metric_add(uint16_t c) {
    uint16_t blocks = (UMM_NBLOCK(c) & UMM_BLOCKNO_MASK) - c;
    DBGLOG_DEBUG("Add block %i size %i to free metric\n", c, blocks);
    ummHeapInfo.freeBlocks += blocks;
    ummHeapInfo.freeBlocksSquared += (blocks * blocks);
}

static void umm_fragmentation_metric_remove(uint16_t c) {
    uint16_t blocks = (UMM_NBLOCK(c) & UMM_BLOCKNO_MASK) - c;
    DBGLOG_DEBUG("Remove block %i size %i from free metric\n", c, blocks);
    ummHeapInfo.freeBlocks -= blocks;
    ummHeapInfo.freeBlocksSquared -= (blocks * blocks);
}
#endif // UMM_INLINE_METRICS

/* ------------------------------------------------------------------------ */
#endif
/* ------------------------------------------------------------------------ */

// UMM_INTEGRITY_CHECK

/* ------------------------------------------------------------------------ */
#ifdef UMM_INTEGRITY_CHECK

#include <stdint.h>
#include <stdbool.h>

/*
 * Perform integrity check of the whole heap data. Returns 1 in case of
 * success, 0 otherwise.
 *
 * First of all, iterate through all free blocks, and check that all backlinks
 * match (i.e. if block X has next free block Y, then the block Y should have
 * previous free block set to X).
 *
 * Additionally, we check that each free block is correctly marked with
 * `UMM_FREELIST_MASK` on the `next` pointer: during iteration through free
 * list, we mark each free block by the same flag `UMM_FREELIST_MASK`, but
 * on `prev` pointer. We'll check and unmark it later.
 *
 * Then, we iterate through all blocks in the heap, and similarly check that
 * all backlinks match (i.e. if block X has next block Y, then the block Y
 * should have previous block set to X).
 *
 * But before checking each backlink, we check that the `next` and `prev`
 * pointers are both marked with `UMM_FREELIST_MASK`, or both unmarked.
 * This way, we ensure that the free flag is in sync with the free pointers
 * chain.
 */
bool umm_integrity_check(void) {
    UMM_CRITICAL_DECL(id_integrity);
    bool ok = true;
    uint16_t prev;
    uint16_t cur;

    UMM_CHECK_INITIALIZED();

    /* Iterate through all free blocks */
    prev = 0;
    UMM_CRITICAL_ENTRY(id_integrity);
    while (1) {
        cur = UMM_NFREE(prev);

        /* Check that next free block number is valid */
        if (cur >= UMM_NUMBLOCKS) {
            DBGLOG_CRITICAL("Heap integrity broken: too large next free num: %d "
                "(in block %d, addr 0x%08x)\n",
                cur, prev, DBGLOG_32_BIT_PTR(&UMM_NBLOCK(prev)));
            ok = false;
            goto clean;
        }
        if (cur == 0) {
            /* No more free blocks */
            break;
        }

        /* Check if prev free block number matches */
        if (UMM_PFREE(cur) != prev) {
            DBGLOG_CRITICAL("Heap integrity broken: free links don't match: "
                "%d -> %d, but %d -> %d\n",
                prev, cur, cur, UMM_PFREE(cur));
            ok = false;
            goto clean;
        }

        UMM_PBLOCK(cur) |= UMM_FREELIST_MASK;

        prev = cur;
    }

    /* Iterate through all blocks */
    prev = 0;
    while (1) {
        cur = UMM_NBLOCK(prev) & UMM_BLOCKNO_MASK;

        /* Check that next block number is valid */
        if (cur >= UMM_NUMBLOCKS) {
            DBGLOG_CRITICAL("Heap integrity broken: too large next block num: %d "
                "(in block %d, addr 0x%08x)\n",
                cur, prev, DBGLOG_32_BIT_PTR(&UMM_NBLOCK(prev)));
            ok = false;
            goto clean;
        }
        if (cur == 0) {
            /* No more blocks */
            break;
        }

        /* make sure the free mark is appropriate, and unmark it */
        if ((UMM_NBLOCK(cur) & UMM_FREELIST_MASK)
            != (UMM_PBLOCK(cur) & UMM_FREELIST_MASK)) {
            DBGLOG_CRITICAL("Heap integrity broken: mask wrong at addr 0x%08x: n=0x%x, p=0x%x\n",
                DBGLOG_32_BIT_PTR(&UMM_NBLOCK(cur)),
                (UMM_NBLOCK(cur) & UMM_FREELIST_MASK),
                (UMM_PBLOCK(cur) & UMM_FREELIST_MASK));
            ok = false;
            goto clean;
        }

        /* make sure the block list is sequential */
        if (cur <= prev) {
            DBGLOG_CRITICAL("Heap integrity broken: next block %d is before prev this one "
                "(in block %d, addr 0x%08x)\n",
                cur, prev, DBGLOG_32_BIT_PTR(&UMM_NBLOCK(prev)));
            ok = false;
            goto clean;
        }

/* unmark */
        UMM_PBLOCK(cur) &= UMM_BLOCKNO_MASK;

        /* Check if prev block number matches */
        if (UMM_PBLOCK(cur) != prev) {
            DBGLOG_CRITICAL("Heap integrity broken: block links don't match: "
                "%d -> %d, but %d -> %d\n",
                prev, cur, cur, UMM_PBLOCK(cur));
            ok = false;
            goto clean;
        }

        prev = cur;
    }

clean:
    UMM_CRITICAL_EXIT(id_integrity);
    if (!ok) {
        UMM_HEAP_CORRUPTION_CB();
    }
    return ok;
}
/* ------------------------------------------------------------------------ */
#endif
/* ------------------------------------------------------------------------ */

// UMM_POISON_CHECK

/* ------------------------------------------------------------------------ */
#if defined(UMM_POISON_CHECK)
#define POISON_BYTE (0xa5)

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/*
 * Yields a size of the poison for the block of size `s`.
 * If `s` is 0, returns 0.
 */
static size_t poison_size(size_t s) {
    return s ? (UMM_POISON_SIZE_BEFORE +
        sizeof(UMM_POISONED_BLOCK_LEN_TYPE) +
        UMM_POISON_SIZE_AFTER)
             : 0;
}

/*
 * Print memory contents starting from given `ptr`
 */
static void dump_mem(__attribute__((unused)) const void *ptr, size_t len) {
    while (len--) {
        DBGLOG_ERROR(" 0x%.2x", (*(uint8_t *)ptr++));
    }
}

/*
 * Put poison data at given `ptr` and `poison_size`
 */
static void put_poison(void *ptr, size_t poison_size) {
    memset(ptr, POISON_BYTE, poison_size);
}

/*
 * Check poison data at given `ptr` and `poison_size`. `where` is a pointer to
 * a string, either "before" or "after", meaning, before or after the block.
 *
 * If poison is there, returns 1.
 * Otherwise, prints the appropriate message, and returns 0.
 */
static bool check_poison(const void *ptr, size_t poison_size,
    __attribute__((unused)) const void *where) {
    size_t i;
    bool ok = true;

    for (i = 0; i < poison_size; i++) {
        if (((uint8_t *)ptr)[i] != POISON_BYTE) {
            ok = false;
            break;
        }
    }

    if (!ok) {
        DBGLOG_ERROR("No poison %s block at: 0x%08x, actual data:", (char *)where, DBGLOG_32_BIT_PTR(ptr));
        dump_mem(ptr, poison_size);
        DBGLOG_ERROR("\n");
    }

    return ok;
}

/*
 * Check if a block is properly poisoned. Must be called only for non-free
 * blocks.
 */
static bool check_poison_block(umm_block *pblock) {
    bool ok = true;

    if (pblock->header.used.next & UMM_FREELIST_MASK) {
        DBGLOG_ERROR("check_poison_block is called for free block 0x%08x\n", DBGLOG_32_BIT_PTR(pblock));
    } else {
        /* the block is used; let's check poison */
        void *pc = (void *)pblock->body.data;
        void *pc_cur;

        pc_cur = pc + sizeof(UMM_POISONED_BLOCK_LEN_TYPE);
        if (!check_poison(pc_cur, UMM_POISON_SIZE_BEFORE, "before")) {
            ok = false;
            goto clean;
        }

        pc_cur = pc + *((UMM_POISONED_BLOCK_LEN_TYPE *)pc) - UMM_POISON_SIZE_AFTER;
        if (!check_poison(pc_cur, UMM_POISON_SIZE_AFTER, "after")) {
            ok = false;
            goto clean;
        }
    }

clean:
    return ok;
}

/*
 * Takes a pointer returned by actual allocator function (`umm_malloc` or
 * `umm_realloc`), puts appropriate poison, and returns adjusted pointer that
 * should be returned to the user.
 *
 * `size_w_poison` is a size of the whole block, including a poison.
 */
static void *get_poisoned(void *ptr, size_t size_w_poison) {
    if (size_w_poison != 0 && ptr != NULL) {

        /* Poison beginning and the end of the allocated chunk */
        put_poison(ptr + sizeof(UMM_POISONED_BLOCK_LEN_TYPE),
            UMM_POISON_SIZE_BEFORE);
        put_poison(ptr + size_w_poison - UMM_POISON_SIZE_AFTER,
            UMM_POISON_SIZE_AFTER);

        /* Put exact length of the user's chunk of memory */
        *(UMM_POISONED_BLOCK_LEN_TYPE *)ptr = (UMM_POISONED_BLOCK_LEN_TYPE)size_w_poison;

        /* Return pointer at the first non-poisoned byte */
        return ptr + sizeof(UMM_POISONED_BLOCK_LEN_TYPE) + UMM_POISON_SIZE_BEFORE;
    } else {
        return ptr;
    }
}

/*
 * Takes "poisoned" pointer (i.e. pointer returned from `get_poisoned()`),
 * and checks that the poison of this particular block is still there.
 *
 * Returns unpoisoned pointer, i.e. actual pointer to the allocated memory.
 */
static void *get_unpoisoned(void *ptr) {
    if (ptr != NULL) {
        uint16_t c;

        ptr -= (sizeof(UMM_POISONED_BLOCK_LEN_TYPE) + UMM_POISON_SIZE_BEFORE);

        /* Figure out which block we're in. Note the use of truncated division... */
        c = (((void *)ptr) - (void *)(&(UMM_HEAP[0]))) / UMM_BLOCKSIZE;

        check_poison_block(&UMM_BLOCK(c));
    }

    return ptr;
}

/* }}} */

/* ------------------------------------------------------------------------ */

void *umm_poison_malloc(size_t size) {
    void *ret;

    size += poison_size(size);

    ret = umm_malloc(size);

    ret = get_poisoned(ret, size);

    return ret;
}

/* ------------------------------------------------------------------------ */

void *umm_poison_calloc(size_t num, size_t item_size) {
    void *ret;
    size_t size = item_size * num;

    size += poison_size(size);

    ret = umm_malloc(size);

    if (NULL != ret) {
        memset(ret, 0x00, size);
    }

    ret = get_poisoned(ret, size);

    return ret;
}

/* ------------------------------------------------------------------------ */

void *umm_poison_realloc(void *ptr, size_t size) {
    void *ret;

    ptr = get_unpoisoned(ptr);

    size += poison_size(size);
    ret = umm_realloc(ptr, size);

    ret = get_poisoned(ret, size);

    return ret;
}

/* ------------------------------------------------------------------------ */

void umm_poison_free(void *ptr) {

    ptr = get_unpoisoned(ptr);

    umm_free(ptr);
}

/*
 * Iterates through all blocks in the heap, and checks poison for all used
 * blocks.
 */

bool umm_poison_check(void) {
    UMM_CRITICAL_DECL(id_poison);

    bool ok = true;
    unsigned short int cur;

    UMM_CHECK_INITIALIZED();

    UMM_CRITICAL_ENTRY(id_poison);

    /* Now iterate through the blocks list */
    cur = UMM_NBLOCK(0) & UMM_BLOCKNO_MASK;

    while (UMM_NBLOCK(cur) & UMM_BLOCKNO_MASK) {
        if (!(UMM_NBLOCK(cur) & UMM_FREELIST_MASK)) {
            /* This is a used block (not free), so, check its poison */
            ok = check_poison_block(&UMM_BLOCK(cur));
            if (!ok) {
                break;
            }
        }

        cur = UMM_NBLOCK(cur) & UMM_BLOCKNO_MASK;
    }
    UMM_CRITICAL_EXIT(id_poison);

    return ok;
}

/* ------------------------------------------------------------------------ */
#endif
