/*
 * stdlib.h — Standard library functions for B32P3/FPGC
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _STDLIB_H
#define _STDLIB_H

#include <stddef.h>

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1

#define RAND_MAX 2147483647

/* Conversion functions */
int    atoi(const char *nptr);
long   atol(const char *nptr);
long   strtol(const char *nptr, char **endptr, int base);
unsigned long strtoul(const char *nptr, char **endptr, int base);

/* Integer arithmetic */
int  abs(int j);
long labs(long j);

typedef struct {
    int quot;
    int rem;
} div_t;

typedef struct {
    long quot;
    long rem;
} ldiv_t;

div_t  div(int numer, int denom);
ldiv_t ldiv(long numer, long denom);

/* Pseudo-random numbers */
int  rand(void);
void srand(unsigned int seed);

/* Memory allocation */
void *malloc(size_t size);
void *calloc(size_t nmemb, size_t size);
void *realloc(void *ptr, size_t size);
void  free(void *ptr);

/* Searching and sorting */
void  qsort(void *base, size_t nmemb, size_t size,
             int (*compar)(const void *, const void *));
void *bsearch(const void *key, const void *base, size_t nmemb, size_t size,
              int (*compar)(const void *, const void *));

/* Program termination */
void exit(int status);
void abort(void);

/* Environment (stubs — no environment on FPGC) */
char *getenv(const char *name);

#endif /* _STDLIB_H */
