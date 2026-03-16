#ifndef STDDEF_H
#define STDDEF_H

// Standard Definitions
// Common type definitions used across all libraries.

// NULL pointer constant
#ifndef NULL
#define NULL ((void *)0)
#endif

// Size type
#ifndef _SIZE_T_DEFINED
#define _SIZE_T_DEFINED
typedef unsigned int size_t;
#endif

// Pointer difference type
#ifndef _PTRDIFF_T_DEFINED
#define _PTRDIFF_T_DEFINED
typedef int ptrdiff_t;
#endif

#endif // STDDEF_H
