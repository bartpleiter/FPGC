#ifndef USER_H
#define USER_H

// User Library Orchestrator
// Handles inclusion of user modules.
// Allows inclusion of only needed libraries without needing a linker.

// Dependency resolution

// Flag based inclusion of libraries
// Header files
#ifdef USER_SYSCALL
#include "libs/user/syscall.h"
#endif

#ifdef USER_FNP
#ifndef USER_SYSCALL
#error "USER_FNP requires USER_SYSCALL"
#endif
#include "libs/user/fnp.h"
#endif

#ifdef USER_FIXED64
#include "libs/common/fixed64.h"
#endif

#ifdef USER_PLOT
#include "libs/user/gfx/plot.h"
#endif

// Implementation files
#ifdef USER_SYSCALL
#include "libs/user/syscall.c"
#endif

#ifdef USER_FNP
#include "libs/user/fnp.c"
#endif

#ifdef USER_FIXED64
#include "libs/common/fixed64.c"
#endif

#ifdef USER_PLOT
#include "libs/user/gfx/plot.c"
#endif

#endif // USER_H
