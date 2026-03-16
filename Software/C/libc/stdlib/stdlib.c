#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <errno.h>

/*------------------------------------------------------------------------
 * strtol — convert string to long
 *----------------------------------------------------------------------*/
long
strtol(const char *nptr, char **endptr, int base)
{
    const char *s = nptr;
    long result = 0;
    int neg = 0;
    int any = 0;
    int c;
    unsigned long cutoff;
    int cutlim;

    /* Skip whitespace */
    while (isspace((unsigned char)*s))
        s++;

    /* Parse sign */
    c = (unsigned char)*s;
    if (c == '-') {
        neg = 1;
        s++;
    } else if (c == '+') {
        s++;
    }

    /* Auto-detect base */
    if ((base == 0 || base == 16) && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        base = 16;
        s += 2;
    } else if (base == 0) {
        base = (s[0] == '0') ? 8 : 10;
    }

    /* Overflow detection */
    cutoff = neg ? (unsigned long)-(LONG_MIN + LONG_MAX) + (unsigned long)LONG_MAX : (unsigned long)LONG_MAX;
    cutlim = (int)(cutoff % (unsigned long)base);
    cutoff = cutoff / (unsigned long)base;

    while ((c = (unsigned char)*s) != '\0') {
        if (isdigit(c))
            c -= '0';
        else if (isalpha(c))
            c = (c | 0x20) - 'a' + 10;
        else
            break;
        if (c >= base)
            break;

        if ((unsigned long)result > cutoff || ((unsigned long)result == cutoff && c > cutlim)) {
            /* Overflow */
            result = neg ? LONG_MIN : LONG_MAX;
            errno = ERANGE;
            any = 1;
            /* Consume remaining valid digits */
            s++;
            while (*s) {
                c = (unsigned char)*s;
                if (isdigit(c)) c -= '0';
                else if (isalpha(c)) c = (c | 0x20) - 'a' + 10;
                else break;
                if (c >= base) break;
                s++;
            }
            break;
        }
        result = result * base + c;
        any = 1;
        s++;
    }

    if (neg && any)
        result = -result;
    if (endptr)
        *endptr = (char *)(any ? s : nptr);
    return result;
}

/*------------------------------------------------------------------------
 * strtoul — convert string to unsigned long
 *----------------------------------------------------------------------*/
unsigned long
strtoul(const char *nptr, char **endptr, int base)
{
    const char *s = nptr;
    unsigned long result = 0;
    int neg = 0;
    int any = 0;
    int c;
    unsigned long cutoff;
    int cutlim;

    while (isspace((unsigned char)*s))
        s++;

    c = (unsigned char)*s;
    if (c == '-') {
        neg = 1;
        s++;
    } else if (c == '+') {
        s++;
    }

    if ((base == 0 || base == 16) && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        base = 16;
        s += 2;
    } else if (base == 0) {
        base = (s[0] == '0') ? 8 : 10;
    }

    cutoff = ULONG_MAX / (unsigned long)base;
    cutlim = (int)(ULONG_MAX % (unsigned long)base);

    while ((c = (unsigned char)*s) != '\0') {
        if (isdigit(c))
            c -= '0';
        else if (isalpha(c))
            c = (c | 0x20) - 'a' + 10;
        else
            break;
        if (c >= base)
            break;

        if (result > cutoff || (result == cutoff && (unsigned)c > (unsigned)cutlim)) {
            result = ULONG_MAX;
            errno = ERANGE;
            any = 1;
            s++;
            while (*s) {
                c = (unsigned char)*s;
                if (isdigit(c)) c -= '0';
                else if (isalpha(c)) c = (c | 0x20) - 'a' + 10;
                else break;
                if (c >= base) break;
                s++;
            }
            break;
        }
        result = result * base + c;
        any = 1;
        s++;
    }

    if (neg && any)
        result = (unsigned long)(-(long)result);
    if (endptr)
        *endptr = (char *)(any ? s : nptr);
    return result;
}

/*------------------------------------------------------------------------
 * atoi — string to int
 *----------------------------------------------------------------------*/
int
atoi(const char *s)
{
    return (int)strtol(s, NULL, 10);
}

/*------------------------------------------------------------------------
 * atol — string to long
 *----------------------------------------------------------------------*/
long
atol(const char *s)
{
    return strtol(s, NULL, 10);
}

/*------------------------------------------------------------------------
 * abs / labs — absolute value
 *----------------------------------------------------------------------*/
int abs(int j) { return j < 0 ? -j : j; }
long labs(long j) { return j < 0 ? -j : j; }

/*------------------------------------------------------------------------
 * div / ldiv — integer division with quotient and remainder
 *----------------------------------------------------------------------*/
div_t
div(int n, int d)
{
    div_t r;
    r.quot = n / d;
    r.rem = n % d;
    return r;
}

ldiv_t
ldiv(long n, long d)
{
    ldiv_t r;
    r.quot = n / d;
    r.rem = n % d;
    return r;
}

/*------------------------------------------------------------------------
 * rand / srand — pseudo-random number generator (LCG)
 *----------------------------------------------------------------------*/
static unsigned int rand_seed = 1;

int
rand(void)
{
    rand_seed = rand_seed * 1103515245 + 12345;
    return (int)((rand_seed >> 16) & RAND_MAX);
}

void
srand(unsigned int seed)
{
    rand_seed = seed;
}

/*------------------------------------------------------------------------
 * qsort — quicksort (simple implementation)
 *----------------------------------------------------------------------*/
static void
swap_bytes(char *a, char *b, size_t size)
{
    char tmp;
    while (size--) {
        tmp = *a;
        *a++ = *b;
        *b++ = tmp;
    }
}

void
qsort(void *base, size_t nmemb, size_t size,
      int (*compar)(const void *, const void *))
{
    char *arr = (char *)base;
    size_t i, j;
    /* Simple insertion sort */
    for (i = 1; i < nmemb; i++) {
        j = i;
        while (j > 0 && compar(arr + j * size, arr + (j - 1) * size) < 0) {
            swap_bytes(arr + j * size, arr + (j - 1) * size, size);
            j--;
        }
    }
}

/*------------------------------------------------------------------------
 * bsearch — binary search in sorted array
 *----------------------------------------------------------------------*/
void *
bsearch(const void *key, const void *base, size_t nmemb, size_t size,
        int (*compar)(const void *, const void *))
{
    const char *p = (const char *)base;
    size_t lo = 0;
    size_t hi = nmemb;

    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        int cmp = compar(key, p + mid * size);
        if (cmp < 0)
            hi = mid;
        else if (cmp > 0)
            lo = mid + 1;
        else
            return (void *)(p + mid * size);
    }
    return NULL;
}

/*------------------------------------------------------------------------
 * getenv — stub (no environment on FPGC)
 *----------------------------------------------------------------------*/
char *
getenv(const char *name)
{
    (void)name;
    return NULL;
}
