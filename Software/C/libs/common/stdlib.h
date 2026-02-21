#ifndef STDLIB_H
#define STDLIB_H

// Standard Library Functions

#include "libs/common/stddef.h"

// Conversion functions

// Convert string to integer.
int atoi(const char *nptr);

// Convert unsigned integer to string.
int *utoa(unsigned int value, int *buf, int base, int uppercase);

// Convert signed integer to string.
int *itoa(int value, int *buf, int base);

// Utility functions

// Return absolute value of integer.
int abs(int j);

// Return absolute value of long.
long labs(long j);

// Return minimum of two values.
int int_min(int a, int b);

// Return maximum of two values.
int int_max(int a, int b);

// Clamp value between min and max.
int int_clamp(int x, int lo, int hi);

// Random number generator

// Maximum value returned by rand().
#define RAND_MAX 32767

// Generate pseudo-random number.
int rand();

// Seed the random number generator.
void srand(unsigned int seed);

// Sorting and searching

// Sort an array using quicksort algorithm.
void qsort(void *base, size_t nmemb, size_t size,
           int (*compar)(const void *, const void *));

// Binary search in sorted array.
void *bsearch(const void *key, const void *base, size_t nmemb, size_t size,
              int (*compar)(const void *, const void *));

#endif // STDLIB_H
