#include "libs/kernel/mem/malloc.h"

#define HEAP_HEADER_SIZE 2  /* Size of block header in words */

/* Block header structure */
typedef struct block_header
{
    size_t size;                 /* Block size including header */
    struct block_header *next;   /* Next free block (NULL if allocated) */
} block_header_t;

/* Heap management variables */
static block_header_t *free_list = NULL;  /* Head of free list */
static int heap_initialized = 0;

/* 
 * Heap configuration 
 * These values should be adjusted based on the actual memory map.
 * For FPGC, the main memory is SDRAM at 0x0000000.
 * We'll use a portion of SDRAM for the heap.
 * 
 * TODO: Adjust these values based on actual program needs and memory layout.
 */
#define HEAP_START  0x00100000  /* Start of heap (after code/data) */
#define HEAP_SIZE   0x00100000  /* 1M words = 4MB of heap */
#define HEAP_END    (HEAP_START + HEAP_SIZE)

/**
 * Initialize the heap allocator.
 * Called automatically on first malloc if not initialized.
 */
static void malloc_init(void)
{
    if (heap_initialized)
    {
        return;
    }

    /* Create initial free block spanning entire heap */
    free_list = (block_header_t *)HEAP_START;
    free_list->size = HEAP_SIZE;
    free_list->next = NULL;

    heap_initialized = 1;
}

void *malloc(size_t size)
{
    block_header_t *curr;
    block_header_t *prev;
    block_header_t *new_block;
    size_t total_size;

    if (size == 0)
    {
        return NULL;
    }

    /* Initialize heap if needed */
    if (!heap_initialized)
    {
        malloc_init();
    }

    /* Calculate total size needed (user size + header) */
    total_size = size + HEAP_HEADER_SIZE;

    /* Find first fit block */
    prev = NULL;
    curr = free_list;

    while (curr != NULL)
    {
        if (curr->size >= total_size)
        {
            /* Found a suitable block */
            
            /* Check if we should split the block */
            if (curr->size >= total_size + HEAP_HEADER_SIZE + 1)
            {
                /* Split: create new free block after this allocation */
                new_block = (block_header_t *)((unsigned int *)curr + total_size);
                new_block->size = curr->size - total_size;
                new_block->next = curr->next;

                curr->size = total_size;
                curr->next = new_block;
            }

            /* Remove block from free list */
            if (prev == NULL)
            {
                free_list = curr->next;
            }
            else
            {
                prev->next = curr->next;
            }

            /* Mark as allocated (next = NULL means allocated) */
            curr->next = NULL;

            /* Return pointer to user data (after header) */
            return (void *)((unsigned int *)curr + HEAP_HEADER_SIZE);
        }

        prev = curr;
        curr = curr->next;
    }

    /* No suitable block found */
    return NULL;
}

void free(void *ptr)
{
    block_header_t *block;
    block_header_t *curr;
    block_header_t *prev;

    if (ptr == NULL)
    {
        return;
    }

    /* Get block header */
    block = (block_header_t *)((unsigned int *)ptr - HEAP_HEADER_SIZE);

    /* Find insertion point in free list (keep list sorted by address) */
    prev = NULL;
    curr = free_list;

    while (curr != NULL && curr < block)
    {
        prev = curr;
        curr = curr->next;
    }

    /* Insert block into free list */
    block->next = curr;

    if (prev == NULL)
    {
        free_list = block;
    }
    else
    {
        prev->next = block;
    }

    /* Coalesce with next block if adjacent */
    if (curr != NULL && 
        (unsigned int *)block + block->size == (unsigned int *)curr)
    {
        block->size += curr->size;
        block->next = curr->next;
    }

    /* Coalesce with previous block if adjacent */
    if (prev != NULL && 
        (unsigned int *)prev + prev->size == (unsigned int *)block)
    {
        prev->size += block->size;
        prev->next = block->next;
    }
}

void *calloc(size_t nmemb, size_t size)
{
    size_t total;
    void *ptr;
    unsigned int *p;
    size_t i;

    total = nmemb * size;

    if (total == 0)
    {
        return NULL;
    }

    ptr = malloc(total);

    if (ptr != NULL)
    {
        /* Zero the memory */
        p = (unsigned int *)ptr;
        for (i = 0; i < total; i++)
        {
            p[i] = 0;
        }
    }

    return ptr;
}

void *realloc(void *ptr, size_t size)
{
    block_header_t *old_block;
    size_t old_size;
    void *new_ptr;
    unsigned int *src;
    unsigned int *dst;
    size_t i;
    size_t copy_size;

    if (ptr == NULL)
    {
        return malloc(size);
    }

    if (size == 0)
    {
        free(ptr);
        return NULL;
    }

    /* Get old block info */
    old_block = (block_header_t *)((unsigned int *)ptr - HEAP_HEADER_SIZE);
    old_size = old_block->size - HEAP_HEADER_SIZE;

    /* If new size fits in current block, return same pointer */
    if (size <= old_size)
    {
        return ptr;
    }

    /* Allocate new block */
    new_ptr = malloc(size);

    if (new_ptr != NULL)
    {
        /* Copy old data */
        src = (unsigned int *)ptr;
        dst = (unsigned int *)new_ptr;
        copy_size = (old_size < size) ? old_size : size;

        for (i = 0; i < copy_size; i++)
        {
            dst[i] = src[i];
        }

        /* Free old block */
        free(ptr);
    }

    return new_ptr;
}
