/*
 * errno.h — Error numbers for B32P3/FPGC
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _ERRNO_H
#define _ERRNO_H

extern int errno;

#define EDOM   33
#define ERANGE 34
#define EILSEQ 84
#define EINVAL 22
#define ENOMEM 12
#define ENOSYS 38
#define EBADF   9
#define ENOENT  2
#define EEXIST 17
#define EIO     5
#define EACCES 13
#define ENOSPC 28
#define ENOTEMPTY 39
#define EISDIR 21
#define ENOTDIR 20
#define ENFILE 23
#define EMFILE 24
#define ENAMETOOLONG 36

#endif /* _ERRNO_H */
