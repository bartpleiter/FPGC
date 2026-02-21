#ifndef STRING_H
#define STRING_H

// String and Memory Functions
// Note: This system is word-addressable (32-bit), so sizes are in words.

#include "libs/common/stddef.h"

// Memory functions

// Copy n words from src to dest.
// Memory regions must not overlap.
void *memcpy(void *dest, const void *src, size_t n);

// Fill n words of memory with value c (only low byte used).
void *memset(void *s, int c, size_t n);

// Copy n words from src to dest, handling overlapping regions.
void *memmove(void *dest, const void *src, size_t n);

// Compare n words of memory.
int memcmp(const void *s1, const void *s2, size_t n);

// String functions

// Calculate the length of a null-terminated string.
size_t strlen(const char *s);

// Copy string src to dest.
char *strcpy(char *dest, const char *src);

// Copy at most n characters from src to dest.
// If src is shorter than n, dest is padded with nulls.
char *strncpy(char *dest, const char *src, size_t n);

// Compare two strings.
int strcmp(const char *s1, const char *s2);

// Compare at most n characters of two strings.
int strncmp(const char *s1, const char *s2, size_t n);

// Concatenate src to the end of dest.
char *strcat(char *dest, const char *src);

// Concatenate at most n characters of src to dest.
char *strncat(char *dest, const char *src, size_t n);

// Find first occurrence of character c in string s.
char *strchr(const char *s, int c);

// Find last occurrence of character c in string s.
char *strrchr(const char *s, int c);

// Find first occurrence of substring needle in haystack.
char *strstr(const char *haystack, const char *needle);

#endif // STRING_H
