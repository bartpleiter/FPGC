#ifndef _STDDEF_H
#define _STDDEF_H

typedef unsigned int size_t;
typedef int          ptrdiff_t;

#ifndef NULL
#define NULL ((void *)0)
#endif

#define offsetof(type, member) ((size_t)&((type *)0)->member)

/* max_align_t: B32P3 has 4-byte alignment for everything */
typedef int max_align_t;

#endif /* _STDDEF_H */
