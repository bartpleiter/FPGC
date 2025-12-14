#ifndef STRING_H
#define STRING_H

/*
 * String and Memory Functions
 * Minimal implementation of standard C string library functions.
 * Note: This system is word-addressable (32-bit), so sizes are in words.
 */

#include "libs/common/stddef.h"

/* Memory functions */

/**
 * Copy n words from src to dest.
 * Memory regions must not overlap.
 * @param dest Destination pointer
 * @param src Source pointer
 * @param n Number of words to copy
 * @return dest pointer
 */
void *memcpy(void *dest, const void *src, size_t n);

/**
 * Fill n words of memory with value c (only low byte used).
 * @param s Pointer to memory
 * @param c Value to set (low byte replicated to all bytes)
 * @param n Number of words to set
 * @return s pointer
 */
void *memset(void *s, int c, size_t n);

/**
 * Copy n words from src to dest, handling overlapping regions.
 * @param dest Destination pointer
 * @param src Source pointer
 * @param n Number of words to copy
 * @return dest pointer
 */
void *memmove(void *dest, const void *src, size_t n);

/**
 * Compare n words of memory.
 * @param s1 First memory region
 * @param s2 Second memory region
 * @param n Number of words to compare
 * @return 0 if equal, <0 if s1 < s2, >0 if s1 > s2
 */
int memcmp(const void *s1, const void *s2, size_t n);

/* String functions */

/**
 * Calculate the length of a null-terminated string.
 * @param s String pointer
 * @return Length of string (not including null terminator)
 */
size_t strlen(const char *s);

/**
 * Copy string src to dest.
 * @param dest Destination buffer
 * @param src Source string
 * @return dest pointer
 */
char *strcpy(char *dest, const char *src);

/**
 * Copy at most n characters from src to dest.
 * If src is shorter than n, dest is padded with nulls.
 * @param dest Destination buffer
 * @param src Source string
 * @param n Maximum characters to copy
 * @return dest pointer
 */
char *strncpy(char *dest, const char *src, size_t n);

/**
 * Compare two strings.
 * @param s1 First string
 * @param s2 Second string
 * @return 0 if equal, <0 if s1 < s2, >0 if s1 > s2
 */
int strcmp(const char *s1, const char *s2);

/**
 * Compare at most n characters of two strings.
 * @param s1 First string
 * @param s2 Second string
 * @param n Maximum characters to compare
 * @return 0 if equal, <0 if s1 < s2, >0 if s1 > s2
 */
int strncmp(const char *s1, const char *s2, size_t n);

/**
 * Concatenate src to the end of dest.
 * @param dest Destination string
 * @param src Source string
 * @return dest pointer
 */
char *strcat(char *dest, const char *src);

/**
 * Concatenate at most n characters of src to dest.
 * @param dest Destination string
 * @param src Source string
 * @param n Maximum characters to concatenate
 * @return dest pointer
 */
char *strncat(char *dest, const char *src, size_t n);

/**
 * Find first occurrence of character c in string s.
 * @param s String to search
 * @param c Character to find
 * @return Pointer to first occurrence, or NULL if not found
 */
char *strchr(const char *s, int c);

/**
 * Find last occurrence of character c in string s.
 * @param s String to search
 * @param c Character to find
 * @return Pointer to last occurrence, or NULL if not found
 */
char *strrchr(const char *s, int c);

/**
 * Find first occurrence of substring needle in haystack.
 * @param haystack String to search in
 * @param needle Substring to find
 * @return Pointer to first occurrence, or NULL if not found
 */
char *strstr(const char *haystack, const char *needle);

#endif /* STRING_H */
