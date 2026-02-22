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

// Implementation files
#ifdef USER_SYSCALL
#include "libs/user/syscall.c"
#endif

#endif // USER_H
