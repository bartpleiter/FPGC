/*
 * String and memory functions for B32P3/FPGC libc.
 *
 * Algorithms derived from picolibc/newlib (BSD-3-Clause).
 * Simplified for B32P3: no wide char, no locale, size-optimized variants.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <string.h>
#include <stdint.h>
#include <stdlib.h>

/*------------------------------------------------------------------------
 * memcpy — copy n bytes, regions must not overlap
 *----------------------------------------------------------------------*/
void *
memcpy(void *dst, const void *src, size_t n)
{
    char *d = (char *)dst;
    const char *s = (const char *)src;
    while (n--)
        *d++ = *s++;
    return dst;
}

/*------------------------------------------------------------------------
 * memmove — copy n bytes, handles overlapping regions
 *----------------------------------------------------------------------*/
void *
memmove(void *dst, const void *src, size_t n)
{
    char *d = (char *)dst;
    const char *s = (const char *)src;

    if (d < s) {
        while (n--)
            *d++ = *s++;
    } else {
        d += n;
        s += n;
        while (n--)
            *--d = *--s;
    }
    return dst;
}

/*------------------------------------------------------------------------
 * memset — fill n bytes with value c (low byte)
 *----------------------------------------------------------------------*/
void *
memset(void *m, int c, size_t n)
{
    unsigned char *s = (unsigned char *)m;
    unsigned char uc = (unsigned char)c;
    while (n--)
        *s++ = uc;
    return m;
}

/*------------------------------------------------------------------------
 * memcmp — compare n bytes
 *----------------------------------------------------------------------*/
int
memcmp(const void *s1, const void *s2, size_t n)
{
    const unsigned char *p1 = (const unsigned char *)s1;
    const unsigned char *p2 = (const unsigned char *)s2;
    while (n--) {
        if (*p1 != *p2)
            return *p1 - *p2;
        p1++;
        p2++;
    }
    return 0;
}

/*------------------------------------------------------------------------
 * memchr — find first occurrence of c in n bytes
 *----------------------------------------------------------------------*/
void *
memchr(const void *s, int c, size_t n)
{
    const unsigned char *p = (const unsigned char *)s;
    unsigned char uc = (unsigned char)c;
    while (n--) {
        if (*p == uc)
            return (void *)p;
        p++;
    }
    return NULL;
}

/*------------------------------------------------------------------------
 * memccpy — copy bytes until c found or n exhausted
 *----------------------------------------------------------------------*/
void *
memccpy(void *dst, const void *src, int c, size_t n)
{
    char *d = (char *)dst;
    const char *s = (const char *)src;
    unsigned char uc = (unsigned char)c;
    while (n--) {
        *d = *s++;
        if ((unsigned char)*d++ == uc)
            return d;
    }
    return NULL;
}

/*------------------------------------------------------------------------
 * strlen — string length
 *----------------------------------------------------------------------*/
size_t
strlen(const char *s)
{
    const char *p = s;
    while (*p)
        p++;
    return (size_t)(p - s);
}

/*------------------------------------------------------------------------
 * strnlen — bounded string length
 *----------------------------------------------------------------------*/
size_t
strnlen(const char *s, size_t maxlen)
{
    const char *p = s;
    while (maxlen-- && *p)
        p++;
    return (size_t)(p - s);
}

/*------------------------------------------------------------------------
 * strcpy — copy string
 *----------------------------------------------------------------------*/
char *
strcpy(char *dst, const char *src)
{
    char *d = dst;
    while ((*d++ = *src++) != '\0')
        ;
    return dst;
}

/*------------------------------------------------------------------------
 * strncpy — copy at most n characters, pad with nulls
 *----------------------------------------------------------------------*/
char *
strncpy(char *dst, const char *src, size_t n)
{
    char *d = dst;
    while (n && (*d = *src) != '\0') {
        d++;
        src++;
        n--;
    }
    while (n--)
        *d++ = '\0';
    return dst;
}

/*------------------------------------------------------------------------
 * strcmp — compare strings
 *----------------------------------------------------------------------*/
int
strcmp(const char *s1, const char *s2)
{
    while (*s1 && *s1 == *s2) {
        s1++;
        s2++;
    }
    return (unsigned char)*s1 - (unsigned char)*s2;
}

/*------------------------------------------------------------------------
 * strncmp — compare at most n characters
 *----------------------------------------------------------------------*/
int
strncmp(const char *s1, const char *s2, size_t n)
{
    if (n == 0)
        return 0;
    while (--n && *s1 && *s1 == *s2) {
        s1++;
        s2++;
    }
    return (unsigned char)*s1 - (unsigned char)*s2;
}

/*------------------------------------------------------------------------
 * strcat — concatenate src onto dest
 *----------------------------------------------------------------------*/
char *
strcat(char *dst, const char *src)
{
    char *d = dst;
    while (*d)
        d++;
    while ((*d++ = *src++) != '\0')
        ;
    return dst;
}

/*------------------------------------------------------------------------
 * strncat — concatenate at most n characters
 *----------------------------------------------------------------------*/
char *
strncat(char *dst, const char *src, size_t n)
{
    char *d = dst;
    while (*d)
        d++;
    while (n-- && *src)
        *d++ = *src++;
    *d = '\0';
    return dst;
}

/*------------------------------------------------------------------------
 * strchr — find first occurrence of c
 *----------------------------------------------------------------------*/
char *
strchr(const char *s, int c)
{
    char ch = (char)c;
    while (*s) {
        if (*s == ch)
            return (char *)s;
        s++;
    }
    return (ch == '\0') ? (char *)s : NULL;
}

/*------------------------------------------------------------------------
 * strrchr — find last occurrence of c
 *----------------------------------------------------------------------*/
char *
strrchr(const char *s, int c)
{
    char ch = (char)c;
    const char *last = NULL;
    while (*s) {
        if (*s == ch)
            last = s;
        s++;
    }
    if (ch == '\0')
        return (char *)s;
    return (char *)last;
}

/*------------------------------------------------------------------------
 * strstr — find substring
 *----------------------------------------------------------------------*/
char *
strstr(const char *haystack, const char *needle)
{
    size_t nlen;
    if (*needle == '\0')
        return (char *)haystack;
    nlen = strlen(needle);
    while (*haystack) {
        if (*haystack == *needle && strncmp(haystack, needle, nlen) == 0)
            return (char *)haystack;
        haystack++;
    }
    return NULL;
}

/*------------------------------------------------------------------------
 * strspn — length of prefix consisting of accept chars
 *----------------------------------------------------------------------*/
size_t
strspn(const char *s, const char *accept)
{
    const char *p = s;
    while (*p) {
        const char *a = accept;
        int found = 0;
        while (*a) {
            if (*p == *a) {
                found = 1;
                break;
            }
            a++;
        }
        if (!found)
            break;
        p++;
    }
    return (size_t)(p - s);
}

/*------------------------------------------------------------------------
 * strcspn — length of prefix NOT consisting of reject chars
 *----------------------------------------------------------------------*/
size_t
strcspn(const char *s, const char *reject)
{
    const char *p = s;
    while (*p) {
        const char *r = reject;
        while (*r) {
            if (*p == *r)
                return (size_t)(p - s);
            r++;
        }
        p++;
    }
    return (size_t)(p - s);
}

/*------------------------------------------------------------------------
 * strpbrk — find first char in s that is in accept
 *----------------------------------------------------------------------*/
char *
strpbrk(const char *s, const char *accept)
{
    while (*s) {
        const char *a = accept;
        while (*a) {
            if (*s == *a)
                return (char *)s;
            a++;
        }
        s++;
    }
    return NULL;
}

/*------------------------------------------------------------------------
 * strtok_r — reentrant string tokenizer
 *----------------------------------------------------------------------*/
char *
strtok_r(char *s, const char *delim, char **saveptr)
{
    char *token;
    if (s == NULL)
        s = *saveptr;
    /* Skip leading delimiters */
    s += strspn(s, delim);
    if (*s == '\0') {
        *saveptr = s;
        return NULL;
    }
    /* Find end of token */
    token = s;
    s = strpbrk(token, delim);
    if (s) {
        *s = '\0';
        *saveptr = s + 1;
    } else {
        *saveptr = token + strlen(token);
    }
    return token;
}

/*------------------------------------------------------------------------
 * strtok — non-reentrant string tokenizer
 *----------------------------------------------------------------------*/
static char *strtok_last;

char *
strtok(char *s, const char *delim)
{
    return strtok_r(s, delim, &strtok_last);
}

/*------------------------------------------------------------------------
 * strdup — duplicate string (requires malloc)
 *----------------------------------------------------------------------*/
char *
strdup(const char *s)
{
    size_t len = strlen(s) + 1;
    char *d = (char *)malloc(len);
    if (d)
        memcpy(d, s, len);
    return d;
}

/*------------------------------------------------------------------------
 * strndup — duplicate at most n characters (requires malloc)
 *----------------------------------------------------------------------*/
char *
strndup(const char *s, size_t n)
{
    size_t len = strnlen(s, n);
    char *d = (char *)malloc(len + 1);
    if (d) {
        memcpy(d, s, len);
        d[len] = '\0';
    }
    return d;
}

/*------------------------------------------------------------------------
 * strerror — error string (minimal)
 *----------------------------------------------------------------------*/
char *
strerror(int errnum)
{
    (void)errnum;
    return "error";
}
