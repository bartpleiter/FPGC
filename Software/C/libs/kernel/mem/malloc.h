#ifndef MALLOC_H
#define MALLOC_H

/*
 * Memory Allocation Functions
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

#include "libs/common/stddef.h"

/**
 * Allocate size words of memory.
 * @param size Number of words to allocate
 * @return Pointer to allocated memory, or NULL if allocation fails
 */
void *malloc(size_t size);

/**
 * Free previously allocated memory.
 * @param ptr Pointer to memory to free
 */
void free(void *ptr);

/**
 * Allocate and zero-initialize memory for an array.
 * @param nmemb Number of elements
 * @param size Size of each element in words
 * @return Pointer to allocated memory, or NULL if allocation fails
 */
void *calloc(size_t nmemb, size_t size);

/**
 * Resize previously allocated memory.
 * @param ptr Pointer to memory to resize (can be NULL)
 * @param size New size in words
 * @return Pointer to resized memory, or NULL if allocation fails
 */
void *realloc(void *ptr, size_t size);

#endif /* MALLOC_H */
