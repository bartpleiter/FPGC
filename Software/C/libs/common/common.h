#ifndef COMMON_H
#define COMMON_H

// Common Library Orchestrator
// Handles inclusion of common modules between kernel and user programs.
// Allows inclusion of only needed libraries without needing a linker.

// Dependency resolution
#ifdef COMMON_STDLIB
#ifndef COMMON_STRING
#define COMMON_STRING
#endif
#endif

// Flag based inclusion of libraries
// Header files
#ifdef COMMON_STRING
#include "libs/common/string.h"
#endif

#ifdef COMMON_STDLIB
#include "libs/common/stdlib.h"
#endif

#ifdef COMMON_CTYPE
#include "libs/common/ctype.h"
#endif

#ifdef COMMON_FIXEDMATH
#include "libs/common/fixedmath.h"
#endif

// Implementation files
#ifdef COMMON_STRING
#include "libs/common/string.c"
#endif

#ifdef COMMON_STDLIB
#include "libs/common/stdlib.c"
#endif

#ifdef COMMON_CTYPE
#include "libs/common/ctype.c"
#endif

#ifdef COMMON_FIXEDMATH
#include "libs/common/fixedmath.c"
#endif

#endif // COMMON_H
