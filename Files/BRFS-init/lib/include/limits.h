#ifndef _LIMITS_H
#define _LIMITS_H

/* Number of bits in a char */
#define CHAR_BIT 8

/* Minimum and maximum values for a signed char */
#define SCHAR_MIN (-128)
#define SCHAR_MAX 127

/* Maximum value for an unsigned char */
#define UCHAR_MAX 255

/* Minimum and maximum values for a char (signed on B32P3) */
#define CHAR_MIN SCHAR_MIN
#define CHAR_MAX SCHAR_MAX

/* Maximum number of bytes in a multibyte character */
#define MB_LEN_MAX 1

/* Minimum and maximum values for a short */
#define SHRT_MIN (-32768)
#define SHRT_MAX 32767

/* Maximum value for an unsigned short */
#define USHRT_MAX 65535

/* Minimum and maximum values for an int (32-bit) */
#define INT_MIN  (-2147483647 - 1)
#define INT_MAX  2147483647

/* Maximum value for an unsigned int */
#define UINT_MAX 4294967295U

/* Minimum and maximum values for a long (32-bit on B32P3) */
#define LONG_MIN  (-2147483647L - 1)
#define LONG_MAX  2147483647L

/* Maximum value for an unsigned long */
#define ULONG_MAX 4294967295UL

/* Minimum and maximum values for a long long (64-bit) */
#define LLONG_MAX  ((long long)(ULLONG_MAX >> 1))
#define LLONG_MIN  (-LLONG_MAX - 1LL)

/* Maximum value for an unsigned long long */
#define ULLONG_MAX ((unsigned long long)-1)

#endif /* _LIMITS_H */
