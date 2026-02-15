#include "libs/common/stdlib.h"

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

int *utoa(unsigned int value, int *buf, int base, int uppercase)
{
    static const char digits_lower[] = "0123456789abcdef";
    static const char digits_upper[] = "0123456789ABCDEF";
    const char *digits = uppercase ? digits_upper : digits_lower;
    int *p = buf;
    int *first = buf;
    int tmp;

    /* Generate digits in reverse order */
    do
    {
        *p++ = digits[value % base];
        value /= base;
    } while (value > 0);

    *p = '\0';

    /* Reverse the string */
    p--;
    while (first < p)
    {
        tmp = *first;
        *first++ = *p;
        *p-- = tmp;
    }

    return buf;
}

int *itoa(int value, int *buf, int base)
{
    int *p = buf;

    if (value < 0 && base == 10)
    {
        *p++ = '-';
        value = -value;
    }

    utoa((unsigned int)value, p, base, 0);

    return buf;
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
static unsigned int stdlib_rand_seed = 1;

int rand()
{
    /* LCG parameters (same as glibc) */
    stdlib_rand_seed = stdlib_rand_seed * 1103515245 + 12345;
    return (int)((stdlib_rand_seed >> 16) & RAND_MAX);
}

void srand(unsigned int seed)
{
    stdlib_rand_seed = seed;
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
