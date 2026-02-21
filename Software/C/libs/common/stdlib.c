//
// stdlib library implementation.
//

#include "libs/common/stdlib.h"

// ---- Conversion Functions ----

// Convert decimal string to signed integer.
int atoi(const char *nptr)
{
  int result = 0;
  int sign = 1;

  while (*nptr == ' ' || *nptr == '\t' || *nptr == '\n')
  {
    nptr++;
  }

  if (*nptr == '-')
  {
    sign = -1;
    nptr++;
  }
  else if (*nptr == '+')
  {
    nptr++;
  }

  while (*nptr >= '0' && *nptr <= '9')
  {
    result = result * 10 + (*nptr - '0');
    nptr++;
  }

  return sign * result;
}

// Convert unsigned integer to string in the selected base.
int *utoa(unsigned int value, int *buf, int base, int uppercase)
{
  static const char digits_lower[] = "0123456789abcdef";
  static const char digits_upper[] = "0123456789ABCDEF";
  const char *digits = uppercase ? digits_upper : digits_lower;
  int *p = buf;
  int *first = buf;
  int tmp;

  do
  {
    *p++ = digits[value % base];
    value /= base;
  } while (value > 0);

  *p = '\0';

  p--;
  while (first < p)
  {
    tmp = *first;
    *first++ = *p;
    *p-- = tmp;
  }

  return buf;
}

// Convert signed integer to string in the selected base.
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

// ---- Utility Functions ----

// Return absolute value of j.
int abs(int j)
{
  return (j < 0) ? -j : j;
}

// Return absolute value of long j.
long labs(long j)
{
  return (j < 0) ? -j : j;
}

// Random number generator state (linear congruential generator).
static unsigned int stdlib_rand_seed = 1;

// Return the next pseudo-random integer.
int rand()
{
  stdlib_rand_seed = stdlib_rand_seed * 1103515245 + 12345;
  return (int)((stdlib_rand_seed >> 16) & RAND_MAX);
}

// Seed the pseudo-random number generator.
void srand(unsigned int seed)
{
  stdlib_rand_seed = seed;
}

// ---- Sorting/Search Helpers ----

// Swap two elements of size words.
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

// Quicksort the inclusive [lo, hi] range.
static void quicksort_internal(unsigned int *base, size_t lo, size_t hi,
                               size_t size, int (*compar)(const void *, const void *))
{
  size_t i, j;
  unsigned int *pivot;

  if (lo >= hi)
  {
    return;
  }

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

  if (i > 0)
  {
    quicksort_internal(base, lo, i - 1, size, compar);
  }
  quicksort_internal(base, i + 1, hi, size, compar);
}

// Sort nmemb elements with the provided comparator.
void qsort(void *base, size_t nmemb, size_t size,
           int (*compar)(const void *, const void *))
{
  if (nmemb < 2)
  {
    return;
  }

  quicksort_internal((unsigned int *)base, 0, nmemb - 1, size, compar);
}

// Binary-search nmemb sorted elements for key.
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

// ---- Integer Helpers ----

// Return the smaller of a and b.
int int_min(int a, int b)
{
  if (a < b)
  {
    return a;
  }
  return b;
}

// Return the larger of a and b.
int int_max(int a, int b)
{
  if (a > b)
  {
    return a;
  }
  return b;
}

// Clamp x to the inclusive [lo, hi] range.
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
