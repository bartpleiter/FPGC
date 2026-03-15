/*
 * malloc/free/calloc/realloc for B32P3/FPGC libc.
 *
 * Simple first-fit free list allocator. Not thread-safe (single-threaded on FPGC).
 * Memory is obtained from _sbrk(), which must be provided by the platform.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* Alignment: all allocations are word-aligned (4 bytes on B32P3) */
#define ALIGN 4
#define ALIGN_MASK (ALIGN - 1)
#define ALIGN_UP(x) (((x) + ALIGN_MASK) & ~ALIGN_MASK)

/* Block header: placed before every allocated/free region */
struct block_header {
    size_t size;                   /* size of usable region (excludes header) */
    struct block_header *next;     /* next block in free list (if free) */
};

#define HEADER_SIZE (sizeof(struct block_header))

/* External: platform provides _sbrk to grow the heap */
extern void *_sbrk(int incr);

/* Free list head */
static struct block_header *free_list = NULL;

/*------------------------------------------------------------------------
 * malloc — allocate size bytes
 *----------------------------------------------------------------------*/
void *
malloc(size_t size)
{
    struct block_header *curr, *prev, *new_block;
    size_t alloc_size;

    if (size == 0)
        return NULL;

    size = ALIGN_UP(size);
    alloc_size = size + HEADER_SIZE;

    /* Search free list (first fit) */
    prev = NULL;
    curr = free_list;
    while (curr) {
        if (curr->size >= size) {
            /* Found a fit. Split if there's enough room for another block. */
            if (curr->size >= size + HEADER_SIZE + ALIGN) {
                /* Split: create a new free block after the allocated region */
                new_block = (struct block_header *)((char *)curr + HEADER_SIZE + size);
                new_block->size = curr->size - size - HEADER_SIZE;
                new_block->next = curr->next;
                curr->size = size;
                curr->next = NULL;
                if (prev)
                    prev->next = new_block;
                else
                    free_list = new_block;
            } else {
                /* Use the whole block */
                if (prev)
                    prev->next = curr->next;
                else
                    free_list = curr->next;
                curr->next = NULL;
            }
            return (void *)((char *)curr + HEADER_SIZE);
        }
        prev = curr;
        curr = curr->next;
    }

    /* No free block found — request more memory from _sbrk */
    curr = (struct block_header *)_sbrk((int)alloc_size);
    if (curr == (struct block_header *)-1)
        return NULL;

    curr->size = size;
    curr->next = NULL;
    return (void *)((char *)curr + HEADER_SIZE);
}

/*------------------------------------------------------------------------
 * free — return block to free list
 *----------------------------------------------------------------------*/
void
free(void *ptr)
{
    struct block_header *blk, *curr, *prev;

    if (!ptr)
        return;

    blk = (struct block_header *)((char *)ptr - HEADER_SIZE);

    /* Insert into free list in address order for coalescing */
    prev = NULL;
    curr = free_list;
    while (curr && curr < blk) {
        prev = curr;
        curr = curr->next;
    }

    /* Try to coalesce with the previous block */
    if (prev && (char *)prev + HEADER_SIZE + prev->size == (char *)blk) {
        prev->size += HEADER_SIZE + blk->size;
        blk = prev;
    } else {
        blk->next = curr;
        if (prev)
            prev->next = blk;
        else
            free_list = blk;
    }

    /* Try to coalesce with the next block */
    if (curr && (char *)blk + HEADER_SIZE + blk->size == (char *)curr) {
        blk->size += HEADER_SIZE + curr->size;
        blk->next = curr->next;
    }
}

/*------------------------------------------------------------------------
 * calloc — allocate and zero
 *----------------------------------------------------------------------*/
void *
calloc(size_t nmemb, size_t size)
{
    size_t total = nmemb * size;
    void *ptr;

    /* Overflow check */
    if (nmemb != 0 && total / nmemb != size)
        return NULL;

    ptr = malloc(total);
    if (ptr)
        memset(ptr, 0, total);
    return ptr;
}

/*------------------------------------------------------------------------
 * realloc — resize allocation
 *----------------------------------------------------------------------*/
void *
realloc(void *ptr, size_t size)
{
    struct block_header *blk;
    void *new_ptr;
    size_t copy_size;

    if (!ptr)
        return malloc(size);
    if (size == 0) {
        free(ptr);
        return NULL;
    }

    blk = (struct block_header *)((char *)ptr - HEADER_SIZE);
    if (blk->size >= size)
        return ptr;  /* Current block is big enough */

    new_ptr = malloc(size);
    if (!new_ptr)
        return NULL;

    copy_size = blk->size < size ? blk->size : size;
    memcpy(new_ptr, ptr, copy_size);
    free(ptr);
    return new_ptr;
}
