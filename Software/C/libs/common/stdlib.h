#ifndef STDLIB_H
#define STDLIB_H

/*
 * Standard Library Functions
 */

#include "libs/common/stddef.h"

/* Conversion functions */

/**
 * Convert string to integer.
 * @param nptr String to convert
 * @return Integer value
 */
int atoi(const char *nptr);

/**
 * Convert unsigned integer to string.
 * @param value Unsigned integer value
 * @param buf Buffer to store string (must be large enough)
 * @param base Numerical base (e.g., 10 for decimal, 16 for hex)
 * @param uppercase Non-zero for uppercase hex digits, zero for lowercase
 * @return Pointer to buffer containing the string
 */
int *utoa(unsigned int value, int *buf, int base, int uppercase);

/**
 * Convert signed integer to string.
 * @param value Signed integer value
 * @param buf Buffer to store string (must be large enough)
 * @param base Numerical base (e.g., 10 for decimal, 16 for hex)
 * @return Pointer to buffer containing the string
 */
int *itoa(int value, int *buf, int base);

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

/* Random number generator */

/**
 * Maximum value returned by rand().
 */
#define RAND_MAX 32767

/**
 * Generate pseudo-random number.
 * @return Random number between 0 and RAND_MAX
 */
int rand();

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

#endif /* STDLIB_H */
