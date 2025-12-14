#ifndef STDLIB_H
#define STDLIB_H

/*
 * Standard Library Functions
 * Minimal implementation for FPGC.
 * Note: Memory allocation uses a simple heap allocator.
 */

#include "libs/common/stddef.h"

/* Memory allocation functions */

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

/* Conversion functions */

/**
 * Convert string to integer.
 * @param nptr String to convert
 * @return Integer value
 */
int atoi(const char *nptr);

/**
 * Convert string to long integer.
 * @param nptr String to convert
 * @return Long integer value
 */
long atol(const char *nptr);

/* Utility functions */

/**
 * Return absolute value of integer.
 * @param j Integer value
 * @return Absolute value
 */
int abs(int j);

/**
 * Return absolute value of long.
 * @param j Long value
 * @return Absolute value
 */
long labs(long j);

/* Random number generator */

/**
 * Maximum value returned by rand().
 */
#define RAND_MAX 32767

/**
 * Generate pseudo-random number.
 * @return Random number between 0 and RAND_MAX
 */
int rand(void);

/**
 * Seed the random number generator.
 * @param seed Seed value
 */
void srand(unsigned int seed);

/* Sorting and searching */

/**
 * Sort an array using quicksort algorithm.
 * @param base Pointer to array base
 * @param nmemb Number of elements
 * @param size Size of each element in words
 * @param compar Comparison function
 */
void qsort(void *base, size_t nmemb, size_t size,
           int (*compar)(const void *, const void *));

/**
 * Binary search in sorted array.
 * @param key Pointer to search key
 * @param base Pointer to array base
 * @param nmemb Number of elements
 * @param size Size of each element in words
 * @param compar Comparison function
 * @return Pointer to matching element, or NULL if not found
 */
void *bsearch(const void *key, const void *base, size_t nmemb, size_t size,
              int (*compar)(const void *, const void *));

/* Program termination */

/**
 * Exit program with status code.
 * Note: In bare-metal, this halts the CPU.
 * @param status Exit status code
 */
void exit(int status);

/* Utility functions - inline functions instead of macros */

/**
 * Return minimum of two values.
 * @param a First value
 * @param b Second value
 * @return Minimum value
 */
int int_min(int a, int b);

/**
 * Return maximum of two values.
 * @param a First value
 * @param b Second value
 * @return Maximum value
 */
int int_max(int a, int b);

/**
 * Clamp value between min and max.
 * @param x Value to clamp
 * @param lo Minimum value
 * @param hi Maximum value
 * @return Clamped value
 */
int int_clamp(int x, int lo, int hi);

#endif /* STDLIB_H */
