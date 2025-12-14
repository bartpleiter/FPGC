#include "libs/common/string.h"

/*
 * String and Memory Functions Implementation
 * Minimal implementation for FPGC (word-addressable architecture).
 */

/* Memory functions */

void *memcpy(void *dest, const void *src, size_t n)
{
    unsigned int *d = (unsigned int *)dest;
    const unsigned int *s = (const unsigned int *)src;
    size_t i;

    for (i = 0; i < n; i++)
    {
        d[i] = s[i];
    }

    return dest;
}

void *memset(void *s, int c, size_t n)
{
    unsigned int *p = (unsigned int *)s;
    size_t i;
    /* Replicate the low byte to all 4 bytes of the word */
    unsigned int val = (unsigned int)(c & 0xFF);
    val = val | (val << 8) | (val << 16) | (val << 24);

    for (i = 0; i < n; i++)
    {
        p[i] = val;
    }

    return s;
}

void *memmove(void *dest, const void *src, size_t n)
{
    unsigned int *d = (unsigned int *)dest;
    const unsigned int *s = (const unsigned int *)src;
    size_t i;

    if (d < s)
    {
        /* Copy forward */
        for (i = 0; i < n; i++)
        {
            d[i] = s[i];
        }
    }
    else if (d > s)
    {
        /* Copy backward to handle overlap */
        for (i = n; i > 0; i--)
        {
            d[i - 1] = s[i - 1];
        }
    }
    /* If d == s, no copy needed */

    return dest;
}

int memcmp(const void *s1, const void *s2, size_t n)
{
    const unsigned int *p1 = (const unsigned int *)s1;
    const unsigned int *p2 = (const unsigned int *)s2;
    size_t i;

    for (i = 0; i < n; i++)
    {
        if (p1[i] < p2[i])
        {
            return -1;
        }
        if (p1[i] > p2[i])
        {
            return 1;
        }
    }

    return 0;
}

/* String functions */

size_t strlen(const char *s)
{
    size_t len = 0;

    while (s[len] != '\0')
    {
        len++;
    }

    return len;
}

char *strcpy(char *dest, const char *src)
{
    char *d = dest;

    while (*src != '\0')
    {
        *d++ = *src++;
    }
    *d = '\0';

    return dest;
}

char *strncpy(char *dest, const char *src, size_t n)
{
    size_t i;

    for (i = 0; i < n && src[i] != '\0'; i++)
    {
        dest[i] = src[i];
    }

    /* Pad with nulls if src is shorter than n */
    for (; i < n; i++)
    {
        dest[i] = '\0';
    }

    return dest;
}

int strcmp(const char *s1, const char *s2)
{
    while (*s1 != '\0' && *s1 == *s2)
    {
        s1++;
        s2++;
    }

    return (unsigned char)*s1 - (unsigned char)*s2;
}

int strncmp(const char *s1, const char *s2, size_t n)
{
    size_t i;

    for (i = 0; i < n; i++)
    {
        if (s1[i] != s2[i])
        {
            return (unsigned char)s1[i] - (unsigned char)s2[i];
        }
        if (s1[i] == '\0')
        {
            return 0;
        }
    }

    return 0;
}

char *strcat(char *dest, const char *src)
{
    char *d = dest;

    /* Find end of dest */
    while (*d != '\0')
    {
        d++;
    }

    /* Copy src to end */
    while (*src != '\0')
    {
        *d++ = *src++;
    }
    *d = '\0';

    return dest;
}

char *strncat(char *dest, const char *src, size_t n)
{
    char *d = dest;
    size_t i;

    /* Find end of dest */
    while (*d != '\0')
    {
        d++;
    }

    /* Copy at most n characters from src */
    for (i = 0; i < n && src[i] != '\0'; i++)
    {
        d[i] = src[i];
    }
    d[i] = '\0';

    return dest;
}

char *strchr(const char *s, int c)
{
    char ch = (char)c;

    while (*s != '\0')
    {
        if (*s == ch)
        {
            return (char *)s;
        }
        s++;
    }

    /* Also check for null terminator if searching for '\0' */
    if (ch == '\0')
    {
        return (char *)s;
    }

    return NULL;
}

char *strrchr(const char *s, int c)
{
    char ch = (char)c;
    const char *last = NULL;

    while (*s != '\0')
    {
        if (*s == ch)
        {
            last = s;
        }
        s++;
    }

    /* Check for null terminator if searching for '\0' */
    if (ch == '\0')
    {
        return (char *)s;
    }

    return (char *)last;
}

char *strstr(const char *haystack, const char *needle)
{
    const char *h;
    const char *n;

    if (*needle == '\0')
    {
        return (char *)haystack;
    }

    while (*haystack != '\0')
    {
        h = haystack;
        n = needle;

        while (*h == *n && *n != '\0')
        {
            h++;
            n++;
        }

        if (*n == '\0')
        {
            return (char *)haystack;
        }

        haystack++;
    }

    return NULL;
}
