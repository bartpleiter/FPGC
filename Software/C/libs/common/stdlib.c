#include "libs/common/stdlib.h"

/*
 * Standard Library Functions Implementation
 * Minimal implementation for FPGC.
 */

/*
 * Memory Allocator
 * 
 * Simple free-list allocator. Each block has a header containing:
 * - size: Number of words in the block (including header)
 * - next: Pointer to next free block (or NULL if allocated/last)
 * 
 * Memory layout of a block:
 * [header (2 words)] [user data...]
 * 
 * Note: The heap must be initialized before use by calling malloc_init().
 * The heap start and end addresses depend on the memory map.
 */

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

/* Conversion functions */

int atoi(const char *nptr)
{
    int result = 0;
    int sign = 1;

    /* Skip whitespace */
    while (*nptr == ' ' || *nptr == '\t' || *nptr == '\n')
    {
        nptr++;
    }

    /* Handle sign */
    if (*nptr == '-')
    {
        sign = -1;
        nptr++;
    }
    else if (*nptr == '+')
    {
        nptr++;
    }

    /* Convert digits */
    while (*nptr >= '0' && *nptr <= '9')
    {
        result = result * 10 + (*nptr - '0');
        nptr++;
    }

    return sign * result;
}

long atol(const char *nptr)
{
    long result = 0;
    int sign = 1;

    /* Skip whitespace */
    while (*nptr == ' ' || *nptr == '\t' || *nptr == '\n')
    {
        nptr++;
    }

    /* Handle sign */
    if (*nptr == '-')
    {
        sign = -1;
        nptr++;
    }
    else if (*nptr == '+')
    {
        nptr++;
    }

    /* Convert digits */
    while (*nptr >= '0' && *nptr <= '9')
    {
        result = result * 10 + (*nptr - '0');
        nptr++;
    }

    return sign * result;
}

/* Utility functions */

int abs(int j)
{
    return (j < 0) ? -j : j;
}

long labs(long j)
{
    return (j < 0) ? -j : j;
}

/* Random number generator (Linear Congruential Generator) */
static unsigned int rand_seed = 1;

int rand(void)
{
    /* LCG parameters (same as glibc) */
    rand_seed = rand_seed * 1103515245 + 12345;
    return (int)((rand_seed >> 16) & RAND_MAX);
}

void srand(unsigned int seed)
{
    rand_seed = seed;
}

/* Quicksort implementation */

static void swap_words(unsigned int *a, unsigned int *b, size_t size)
{
    size_t i;
    unsigned int temp;

    for (i = 0; i < size; i++)
    {
        temp = a[i];
        a[i] = b[i];
        b[i] = temp;
    }
}

static void quicksort_internal(unsigned int *base, size_t lo, size_t hi,
                               size_t size, int (*compar)(const void *, const void *))
{
    size_t i, j;
    unsigned int *pivot;

    if (lo >= hi)
    {
        return;
    }

    /* Choose last element as pivot */
    pivot = base + hi * size;
    i = lo;

    for (j = lo; j < hi; j++)
    {
        if (compar(base + j * size, pivot) <= 0)
        {
            swap_words(base + i * size, base + j * size, size);
            i++;
        }
    }

    swap_words(base + i * size, pivot, size);

    /* Recursively sort partitions */
    if (i > 0)
    {
        quicksort_internal(base, lo, i - 1, size, compar);
    }
    quicksort_internal(base, i + 1, hi, size, compar);
}

void qsort(void *base, size_t nmemb, size_t size,
           int (*compar)(const void *, const void *))
{
    if (nmemb < 2)
    {
        return;
    }

    quicksort_internal((unsigned int *)base, 0, nmemb - 1, size, compar);
}

void *bsearch(const void *key, const void *base, size_t nmemb, size_t size,
              int (*compar)(const void *, const void *))
{
    size_t lo = 0;
    size_t hi = nmemb;
    size_t mid;
    const unsigned int *arr = (const unsigned int *)base;
    int cmp;

    while (lo < hi)
    {
        mid = lo + (hi - lo) / 2;
        cmp = compar(key, arr + mid * size);

        if (cmp == 0)
        {
            return (void *)(arr + mid * size);
        }
        else if (cmp < 0)
        {
            hi = mid;
        }
        else
        {
            lo = mid + 1;
        }
    }

    return NULL;
}

/* Program termination */

void exit(int status)
{
    /* 
     * In bare-metal mode, we halt the CPU.
     * The status is stored in r1 (return value register).
     * 
     * TODO: When OS is implemented, this should make a syscall.
     */
    (void)status; /* Use status to avoid warning */
    
    /* Inline assembly to halt the CPU */
    asm("halt");
    
    /* Should never reach here, but prevent compiler warning */
    while (1) {}
}

/* Utility functions */

int int_min(int a, int b)
{
    if (a < b)
    {
        return a;
    }
    return b;
}

int int_max(int a, int b)
{
    if (a > b)
    {
        return a;
    }
    return b;
}

int int_clamp(int x, int lo, int hi)
{
    if (x < lo)
    {
        return lo;
    }
    if (x > hi)
    {
        return hi;
    }
    return x;
}
