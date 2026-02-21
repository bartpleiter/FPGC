//
// string library implementation.
//

#include "libs/common/string.h"

// ---- Memory Functions ----

// Copy n words from src to dest.
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

// Fill n words at s with value c.
void *memset(void *s, int c, size_t n)
{
  unsigned int *p = (unsigned int *)s;
  size_t i;

  for (i = 0; i < n; i++)
  {
    p[i] = c;
  }

  return s;
}

// Copy n words from src to dest, handling overlap.
void *memmove(void *dest, const void *src, size_t n)
{
  unsigned int *d = (unsigned int *)dest;
  const unsigned int *s = (const unsigned int *)src;
  size_t i;

  if (d < s)
  {
    for (i = 0; i < n; i++)
    {
      d[i] = s[i];
    }
  }
  else if (d > s)
  {
    for (i = n; i > 0; i--)
    {
      d[i - 1] = s[i - 1];
    }
  }
  return dest;
}

// Compare n words from s1 and s2.
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

// ---- String Functions ----

// Return the length of string s.
size_t strlen(const char *s)
{
  size_t len = 0;

  while (s[len] != '\0')
  {
    len++;
  }

  return len;
}

// Copy src into dest, including the null terminator.
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

// Copy up to n characters from src into dest.
char *strncpy(char *dest, const char *src, size_t n)
{
  size_t i;

  for (i = 0; i < n && src[i] != '\0'; i++)
  {
    dest[i] = src[i];
  }

  for (; i < n; i++)
  {
    dest[i] = '\0';
  }

  return dest;
}

// Compare strings s1 and s2 lexicographically.
int strcmp(const char *s1, const char *s2)
{
  while (*s1 != '\0' && *s1 == *s2)
  {
    s1++;
    s2++;
  }

  return (unsigned char)*s1 - (unsigned char)*s2;
}

// Compare up to n characters of s1 and s2.
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

// Append src to the end of dest.
char *strcat(char *dest, const char *src)
{
  char *d = dest;

  while (*d != '\0')
  {
    d++;
  }

  while (*src != '\0')
  {
    *d++ = *src++;
  }
  *d = '\0';

  return dest;
}

// Append up to n characters from src to dest.
char *strncat(char *dest, const char *src, size_t n)
{
  char *d = dest;
  size_t i;

  while (*d != '\0')
  {
    d++;
  }

  for (i = 0; i < n && src[i] != '\0'; i++)
  {
    d[i] = src[i];
  }
  d[i] = '\0';

  return dest;
}

// Return pointer to first occurrence of c in s.
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

  if (ch == '\0')
  {
    return (char *)s;
  }

  return NULL;
}

// Return pointer to last occurrence of c in s.
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

  if (ch == '\0')
  {
    return (char *)s;
  }

  return (char *)last;
}

// Return pointer to first occurrence of needle in haystack.
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
