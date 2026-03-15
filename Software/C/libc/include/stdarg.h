/*
 * stdarg.h — Variable argument support for B32P3/FPGC
 *
 * Maps to cproc's built-in __builtin_va_* functions.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _STDARG_H
#define _STDARG_H

typedef __builtin_va_list va_list;

#define va_start(ap, last) __builtin_va_start(ap, last)
#define va_arg(ap, type)   __builtin_va_arg(ap, type)
#define va_end(ap)         __builtin_va_end(ap)
#define va_copy(dest, src) __builtin_va_copy(dest, src)

#endif /* _STDARG_H */
